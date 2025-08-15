
#pragma once
#include <QObject>
#include <memory>
class DbusClient;
class Config;
class TrayController;
class MainWindow;
class ProcessMonitor;
class UPowerClient;

namespace dp { namespace features { class FeatureRegistry; } }
class App : public QObject {
    Q_OBJECT
public:
    explicit App(QObject* parent = nullptr);
    ~App(); 
    void start();

private:
    std::unique_ptr<DbusClient> m_dbus;
    std::unique_ptr<Config> m_config;
    std::unique_ptr<TrayController> m_tray;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<ProcessMonitor> m_procMon;
    std::unique_ptr<UPowerClient> m_power;
    std::unique_ptr<dp::features::FeatureRegistry> m_features;

    void onShowMainRequested();
    void onDaemonStateChanged();
    void onUserOverrideChanged(const QString& mode, bool boss);
    void onThresholdsAdjusted(double low, double high);
    void onWindowVisibilityChanged(bool visible);
    void updateTrayFromState();
};