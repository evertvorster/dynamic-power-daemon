#pragma once

#include <QObject>
#include <QTimer> 
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>
#include <QMap>
#include "config/config.h"  // for Thresholds struct
#include <QFileSystemWatcher>

class DaemonDBusInterface;

class Daemon : public QObject {
    Q_OBJECT

public:
    // Constructor now takes thresholds loaded from config
    Daemon(const Thresholds &thresholds,  int graceSeconds, QObject *parent = nullptr);
    bool loadAvailableProfiles();
    bool setProfile(const QString& internalName);
    QString m_requestedProfile;
    bool    m_userRequestedProfile = false;
    // getters.
    void ProfileChanged(const QString& internalName);
    QString getActiveProfile() const { return m_activeProfile; }
    double getLowThreshold() const { return m_actualThresholds.low; }
    double getHighThreshold() const { return m_actualThresholds.high; }
    // setters.
    void setRequestedThresholds(double low, double high) {
        m_requestedThresholds.low  = low;
        m_requestedThresholds.high = high;
    }
    void setRequestedProfile(const QString& name, bool userRequested) {
        m_requestedProfile = name;
        m_userRequestedProfile = userRequested;
    }
    
private Q_SLOTS:
    void handlePropertiesChanged(const QDBusMessage &message);
    void handleUPowerChanged(const QDBusMessage &message);
    void checkLoadAverage();              // New slot: called every 5 seconds to check system load
    void onConfigFileChanged(const QString &path);

private:
    Thresholds m_thresholds;              // Low/high thresholds from config
    Thresholds m_actualThresholds;        // Low/high thresholds actually used. 
    Thresholds m_requestedThresholds;     // Low/high thresholds requested from user.
    QMap<QString, QString> m_profileMap;  // Internal → actual DBus profile name
    QTimer* m_timer = nullptr;            // Polling timer for load average
    QTimer* m_graceTimer = nullptr;       // Timer used for the grace period
    QString m_powerSource;                // "AC" or "battery"
    QString m_currentProfile;             // Actual current active profile
    QString m_overrideProfile;            // Optional override profile
    QString m_activeProfile;              // Variable for dbus interface
    bool m_isBossOverride = false;        // Override flag
    bool m_graceActive = false;           // Whether we're currently in the grace period
    int graceSeconds;                     // Number of seconds for grace period

    void updatePowerSource();             // Reads OnBattery and sets m_powerSource
    DaemonDBusInterface* m_dbusInterface = nullptr; // Dbus comms with user class
    QFileSystemWatcher* m_configWatcher = nullptr;  // watches DEFAULT_CONFIG_PATH
};

