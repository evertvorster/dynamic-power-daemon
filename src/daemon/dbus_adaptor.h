#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QVariantMap>

class Daemon;

class DynamicPowerAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.dynamic_power.Daemon")

public:
    explicit DynamicPowerAdaptor(Daemon *parent);

public slots:
    QString Ping();
    QVariantMap GetDaemonState();
    bool SetPollInterval(uint interval);
    bool SetProfile(const QString &profile, bool is_user);
    void SetLoadThresholds(double low, double high);

private:
    Daemon *m_daemon;
};
