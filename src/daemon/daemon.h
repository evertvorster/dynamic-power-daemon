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

private Q_SLOTS:
    void handlePropertiesChanged(const QDBusMessage &message);
    void handleUPowerChanged(const QDBusMessage &message);
    // New slot: called every 5 seconds to check system load
    void checkLoadAverage();

private:
    Thresholds m_thresholds;   // Stores low/high thresholds from config
    QMap<QString, QString> m_profileMap;  // internal name → actual DBus name
    QTimer *m_timer = nullptr;  // Pointer to the timer that triggers load check
};
