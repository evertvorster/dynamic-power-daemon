#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QVariantMap>

class Daemon;

class DynamicPowerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.dynamic_power.DaemonCpp")

public:
    explicit DynamicPowerAdaptor(Daemon *parent);

public slots:
    QString Ping();
    QVariantMap GetDaemonState();
    bool SetLoadThresholds(double low, double high);
    bool SetPollInterval(uint interval);
    bool SetProfile(const QString &profile, bool is_user);
};
