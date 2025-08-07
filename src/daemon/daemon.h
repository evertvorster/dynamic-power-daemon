#pragma once

#include <QObject>
#include <QTimer> 
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>
#include <QMap>
#include "config/config.h"  // for Thresholds struct

class Daemon : public QObject {
    Q_OBJECT

public:
    // Constructor now takes thresholds loaded from config
    Daemon(const Thresholds &thresholds, QObject *parent = nullptr);
    bool loadAvailableProfiles();
    bool setProfile(const QString& internalName);
    
private Q_SLOTS:
    void handlePropertiesChanged(const QDBusMessage &message);
    void handleUPowerChanged(const QDBusMessage &message);
    void checkLoadAverage();              // New slot: called every 5 seconds to check system load

private:
    Thresholds m_thresholds;              // Low/high thresholds from config
    QMap<QString, QString> m_profileMap;  // Internal → actual DBus profile name
    QTimer *m_timer = nullptr;            // Polling timer for load average
    QString m_powerSource;                // "AC" or "battery"
    QString m_currentProfile;             // Actual current DBus profile
    QString m_overrideProfile;            // Optional override profile
    bool m_isBossOverride = false;        // Override flag

    void updatePowerSource();             // Reads OnBattery and sets m_powerSource
};
