
#include "App.h"
#include "DbusClient.h"
#include "Config.h"
#include "TrayController.h"
#include "MainWindow.h"
#include "ProcessMonitor.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

App::~App() = default; 

App::App(QObject* parent) : QObject(parent) {}

void App::start() {
    // Load config (ensure exists)
    m_config = std::make_unique<Config>();
    m_config->ensureExists();
    m_config->load();

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
    if (m_mainWindow) {
        m_mainWindow->showNormal();
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
        // Apply current thresholds from daemon to graph
        auto state = m_dbus->getDaemonState();
        double low = state.value("threshold_low").toDouble();
        double high = state.value("threshold_high").toDouble();
        m_mainWindow->setThresholds(low, high);
        m_mainWindow->setActiveProfile(state.value("active_profile").toString());
    }
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
        // Set low threshold to 0, keep current high from config, and set boss flag via a balanced profile "anchor"
        auto state = m_dbus->getDaemonState();
        double high = state.value("threshold_high").toDouble();
        m_dbus->setLoadThresholds(0.0, high);
        m_dbus->setProfile(QStringLiteral("balanced"), true);
    } else if (mode == QStringLiteral("Dynamic")) {
        // Clear boss by setting profile according to daemon policy (no boss)
        // We just send a balanced request with boss=false to let daemon take over
        m_dbus->setProfile(QStringLiteral("balanced"), false);
        // Restore thresholds from config
        auto th = m_config->thresholds();
        m_dbus->setLoadThresholds(th.first, th.second);
    } else {
        // Exact profile
        m_dbus->setProfile(mode.toLower(), boss);
    }
    updateTrayFromState();
}

void App::onThresholdsAdjusted(double low, double high) {
    // Persist to config, send to daemon, then fetch applied back
    m_config->setThresholds(low, high);
    m_config->save();
    m_dbus->setLoadThresholds(low, high);
    auto state = m_dbus->getDaemonState();
    m_mainWindow->setThresholds(state.value("threshold_low").toDouble(),
                                state.value("threshold_high").toDouble());
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
