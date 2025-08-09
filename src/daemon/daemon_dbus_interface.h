#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QVariantMap>

class Daemon;

class DaemonDBusInterface : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.dynamic_power.Daemon")

public:
    explicit DaemonDBusInterface(Daemon *parent);
    
public slots:
    QString Ping();
    QVariantMap GetDaemonState();
    bool SetLoadThresholds(double low, double high);
    bool SetProfile(const QString& name, bool userRequested);
    bool SetPollInterval(uint interval);

Q_SIGNALS:
    void PowerStateChanged();
};
