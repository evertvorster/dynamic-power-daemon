
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
    });

    // DBus client
    m_dbus = std::make_unique<DbusClient>(this);
    connect(m_dbus.get(), &DbusClient::powerStateChanged, this, &App::onDaemonStateChanged);

    // Tray
    m_tray = std::make_unique<TrayController>();
    connect(m_tray.get(), &TrayController::showMainRequested, this, &App::onShowMainRequested);

    // Main window
    m_mainWindow = std::make_unique<MainWindow>(m_dbus.get(), m_config.get());
    connect(m_mainWindow.get(), &MainWindow::userOverrideSelected, this, &App::onUserOverrideChanged);
    connect(m_mainWindow.get(), &MainWindow::thresholdsAdjusted, this, &App::onThresholdsAdjusted);
    connect(m_mainWindow.get(), &MainWindow::visibilityChanged, this, &App::onWindowVisibilityChanged);
    m_mainWindow->refreshProcessButtons();

    // Process monitor (starts only when window hidden)
    m_procMon = std::make_unique<ProcessMonitor>(this);
    connect(m_procMon.get(), &ProcessMonitor::matchedProfile, this, [this](const QString& profile) {
        // Send non-boss override to daemon
        if (m_mainWindow) {
            // Only if user is in Dynamic mode
            if (m_mainWindow->currentUserMode() == QStringLiteral("Dynamic")) {
                m_dbus->setProfile(profile, false);
                updateTrayFromState();
            }
        }
    });

    // Initial state
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
    updateTrayFromState();
}

void App::onUserOverrideChanged(const QString& mode, bool boss) {
    if (mode == QStringLiteral("Inhibit Powersave")) {
        auto state = m_dbus->getDaemonState();
        double high = state.value("threshold_high").toDouble();
        m_dbus->setLoadThresholds(0.0, high);
        m_dbus->setProfile(QStringLiteral("balanced"), true);
    } else if (mode == QStringLiteral("Dynamic")) {
        m_dbus->setProfile(QStringLiteral("balanced"), false);
        auto th = m_config->thresholds();
        m_dbus->setLoadThresholds(th.first, th.second);
    } else {
        m_dbus->setProfile(mode.toLower(), boss);
    }

    // NEW: reflect applied state on the button immediately
    auto state = m_dbus->getDaemonState();
    if (m_mainWindow) {
        m_mainWindow->setActiveProfile(state.value("active_profile").toString());
    }
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
    if (visible) {
        m_procMon->stop();
    } else {
        // Provide rules to monitor
        m_procMon->setRules(m_config->processRules());
        m_procMon->start();
    }
}

void App::updateTrayFromState() {
    auto state = m_dbus->getDaemonState();
    bool onBattery = state.value("on_battery").toBool(); // may be absent; default false
    QString active = state.value("active_profile").toString();

    // Determine icon state
    QString userMode = m_mainWindow ? m_mainWindow->currentUserMode() : QStringLiteral("Dynamic");
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
        m_tray->setTooltip(QStringLiteral("dynamic_power: %1").arg(active));
    }
}
