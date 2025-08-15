
#include "App.h"
#include "DbusClient.h"
#include "Config.h"
#include "TrayController.h"
#include "MainWindow.h"
#include "ProcessMonitor.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include "UPowerClient.h"
#include <QSet>
#include "UserFeatures.h"
#include "features/FeatureRegistry.h"

App::~App() = default; 

App::App(QObject* parent) : QObject(parent) {}

void App::start() {
    m_config = std::make_unique<Config>();
    m_config->ensureExists();
    m_config->load();
    connect(m_config.get(), &Config::reloaded, this, [this]() {
        // Update UI lists + process monitor rules
        if (m_mainWindow) m_mainWindow->refreshProcessButtons();
        if (m_procMon)    m_procMon->setRules(m_config->processRules());
        // Thresholds: UI shows what daemon applied; if thresholds changed externally,
        // you can optionally push them to daemon here:
        auto th = m_config->thresholds();
        m_dbus->setLoadThresholds(th.first, th.second);
        if (m_procMon) {
            m_procMon->setRules(m_config->processRules());
        }
    });

    // DBus client
    m_dbus = std::make_unique<DbusClient>(this);
    connect(m_dbus.get(), &DbusClient::powerStateChanged, this, &App::onDaemonStateChanged);

    // Startup: push configured thresholds to the daemon so it doesn’t keep defaults
    auto th = m_config->thresholds();
    m_dbus->setLoadThresholds(th.first, th.second);

    // UPower client
    m_power = std::make_unique<UPowerClient>(this);
    connect(m_power.get(), &UPowerClient::powerInfoChanged, this, [this]() {
        updateTrayFromState();
        if (m_mainWindow) m_mainWindow->setPowerInfo(m_power->summaryText());
    });

    // Tray
    m_tray = std::make_unique<TrayController>();
    connect(m_tray.get(), &TrayController::showMainRequested, this, &App::onShowMainRequested);

    // Main window
    m_mainWindow = std::make_unique<MainWindow>(m_dbus.get(), m_config.get());
    connect(m_mainWindow.get(), &MainWindow::userOverrideSelected, this, &App::onUserOverrideChanged);
    connect(m_mainWindow.get(), &MainWindow::thresholdsAdjusted, this, &App::onThresholdsAdjusted);
    connect(m_mainWindow.get(), &MainWindow::visibilityChanged, this, &App::onWindowVisibilityChanged);
    m_features = std::make_unique<dp::features::FeatureRegistry>();
    m_mainWindow->refreshProcessButtons();

    // Process monitor (runs continuously)
    m_procMon = std::make_unique<ProcessMonitor>(this);
    m_procMon->setRules(m_config->processRules());
    m_procMon->start(5000);
    connect(m_procMon.get(), &ProcessMonitor::matchesUpdated, this,
            [this](const QSet<QString>& matches, const QString& winner){
                if (m_mainWindow) m_mainWindow->setProcessMatchState(matches, winner);
            });

    connect(m_procMon.get(), &ProcessMonitor::matchedProfile, this, [this](const QString& mode) {
        if (!m_mainWindow || m_mainWindow->currentUserMode() != QStringLiteral("Dynamic"))
            return;

        // thresholds first
        auto th = m_config->thresholds();
        double low = th.first, high = th.second;

        if (mode.compare("Inhibit Powersave", Qt::CaseInsensitive) == 0) {
            m_dbus->setLoadThresholds(0.0, high);
            m_dbus->setProfile(QString(), false); // boss = false for process
        } else if (mode.compare("Dynamic", Qt::CaseInsensitive) == 0) {
            m_dbus->setLoadThresholds(low, high);
            m_dbus->setProfile(QString(), false);
        } else {
            // performance/balanced/powersave
            m_dbus->setLoadThresholds(low, high);
            m_dbus->setProfile(mode.toLower(), false); // boss = false
        }

        // optional: immediate readback to keep UI aligned
        auto st = m_dbus->getDaemonState();
        if (m_mainWindow) {
            m_mainWindow->setActiveProfile(st.value("active_profile").toString());
            m_mainWindow->setThresholds(st.value("threshold_low").toDouble(),
                                        st.value("threshold_high").toDouble());
        }
        QTimer::singleShot(5000, this, [this]() {
            auto st2 = m_dbus->getDaemonState();
            if (m_mainWindow) {
                m_mainWindow->setActiveProfile(st2.value("active_profile").toString());
                m_mainWindow->setThresholds(st2.value("threshold_low").toDouble(),
                                            st2.value("threshold_high").toDouble());
            }
        });

        updateTrayFromState();
    });
    
    connect(m_procMon.get(), &ProcessMonitor::noMatch, this, [this]() {
        // Only act if the user override is Dynamic
        if (!m_mainWindow || m_mainWindow->currentUserMode() != QStringLiteral("Dynamic"))
            return;

        // Send configured thresholds, then clear profile (boss=false)
        auto th = m_config->thresholds();
        m_dbus->setLoadThresholds(th.first, th.second);
        m_dbus->setProfile(QString(), false);

        // Read back now and after 5s
        auto st = m_dbus->getDaemonState();
        if (m_mainWindow) {
            m_mainWindow->setActiveProfile(st.value("active_profile").toString());
            m_mainWindow->setThresholds(st.value("threshold_low").toDouble(),
                                        st.value("threshold_high").toDouble());
        }
        QTimer::singleShot(5000, this, [this]() {
            auto st2 = m_dbus->getDaemonState();
            if (m_mainWindow) {
                m_mainWindow->setActiveProfile(st2.value("active_profile").toString());
                m_mainWindow->setThresholds(st2.value("threshold_low").toDouble(),
                                            st2.value("threshold_high").toDouble());
            }
        });

        updateTrayFromState();
    });


    // Initial state
    if (m_mainWindow) m_mainWindow->setPowerInfo(m_power->summaryText());
    updateTrayFromState();
}

void App::onShowMainRequested() {
    if (!m_mainWindow) return;
    if (m_mainWindow->isVisible()) {           // new: toggle
        m_mainWindow->hide();
        return;
    }
    m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
    auto state = m_dbus->getDaemonState();
    m_mainWindow->setThresholds(state.value("threshold_low").toDouble(),
                                state.value("threshold_high").toDouble());
    m_mainWindow->setActiveProfile(state.value("active_profile").toString());
}

void App::onDaemonStateChanged() {
    // Refresh UI elements that mirror daemon state
    if (m_mainWindow) {
        auto state = m_dbus->getDaemonState();
        m_mainWindow->setActiveProfile(state.value("active_profile").toString());
    }
    const bool onBattery = m_power ? m_power->onBattery()
                                : m_dbus->getDaemonState().value("on_battery").toBool();
    if (m_features) m_features->applyAll(onBattery);
    /* dialog handles status on open; no mid-session probe */
    updateTrayFromState();
}

void App::onUserOverrideChanged(const QString& mode, bool /*bossParamIgnored*/) {
    // 1) thresholds first
    auto th = m_config->thresholds();
    double low = th.first, high = th.second;

    if (mode == QStringLiteral("Inhibit Powersave")) {
        // Send 0..high but do NOT save to config
        m_dbus->setLoadThresholds(0.0, high);
        // Profile: empty + boss=true
        m_dbus->setProfile(QString(), true);
    } else if (mode == QStringLiteral("Dynamic")) {
        // Configured thresholds
        m_dbus->setLoadThresholds(low, high);
        // Profile: empty + boss=false
        m_dbus->setProfile(QString(), false);
    } else {
        // Performance/Balanced/Powersave → configured thresholds + boss=true
        m_dbus->setLoadThresholds(low, high);
        m_dbus->setProfile(mode.toLower(), true);
    }

    // Immediate readback
    auto state = m_dbus->getDaemonState();
    if (m_mainWindow) {
        m_mainWindow->setActiveProfile(state.value("active_profile").toString());
        m_mainWindow->setThresholds(state.value("threshold_low").toDouble(),
                                    state.value("threshold_high").toDouble());
    }

    // Delayed 5s readback
    QTimer::singleShot(5000, this, [this]() {
        auto st = m_dbus->getDaemonState();
        if (m_mainWindow) {
            m_mainWindow->setActiveProfile(st.value("active_profile").toString());
            m_mainWindow->setThresholds(st.value("threshold_low").toDouble(),
                                        st.value("threshold_high").toDouble());
        }
    });

    updateTrayFromState();
}


void App::onThresholdsAdjusted(double low, double high) {
    // Sanity-check & normalize thresholds
    Config::normalizeThresholds(low, high);
    
    // Persist to config
    m_config->setThresholds(low, high);
    m_config->save();

    // Send to daemon as a pair
    m_dbus->setLoadThresholds(low, high);

    // Fetch applied now
    auto stateNow = m_dbus->getDaemonState();
    if (m_mainWindow) {
        m_mainWindow->setThresholds(stateNow.value("threshold_low").toDouble(),
                                    stateNow.value("threshold_high").toDouble());
    }

    // Fetch again after daemon’s 5s cycle
    QTimer::singleShot(5000, this, [this]() {
        auto stateLater = m_dbus->getDaemonState();
        if (m_mainWindow) {
            m_mainWindow->setThresholds(stateLater.value("threshold_low").toDouble(),
                                        stateLater.value("threshold_high").toDouble());
        }
    });
}

void App::onWindowVisibilityChanged(bool visible) {
       // No-op for ProcessMonitor; it runs continuously.
}

void App::updateTrayFromState() {
    auto state = m_dbus->getDaemonState();
    const bool onBattery = m_power ? m_power->onBattery() : state.value("on_battery").toBool();
    const QString active = state.value("active_profile").toString();
    const QString userMode = m_mainWindow ? m_mainWindow->currentUserMode() : QStringLiteral("Dynamic");

    QString iconKey;
    if (userMode != QStringLiteral("Dynamic")) {
        iconKey = onBattery ? "override_battery" : "override_ac";
    } else if (m_procMon && m_procMon->hasActiveMatch()) {
        iconKey = onBattery ? "match_battery" : "match_ac";
    } else {
        iconKey = onBattery ? "default_battery" : "default_ac";
    }
    if (m_tray) {
        m_tray->setIconByKey(iconKey);
        const QString tip = m_power ? m_power->summaryText() : QString();
        m_tray->setTooltip(QString("dynamic_power: %1\n%2").arg(active, tip));
    }
}