#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>
#include "config/config.h"

class Daemon : public QObject {
    Q_OBJECT

public:
    Daemon(const Thresholds &thresholds, QObject *parent = nullptr);

private Q_SLOTS:
    void handlePropertiesChanged(const QDBusMessage &message);
    void handleUPowerChanged(const QDBusMessage &message); 

private:
    Thresholds m_thresholds;
};
