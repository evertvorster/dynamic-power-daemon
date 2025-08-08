#include "dbus_adaptor.h"
#include "log.h"
#include "daemon.h"
#include <QDateTime> 

DaemonDBusInterface::DaemonDBusInterface(Daemon *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
    log_info("DBus adaptor constructed");
}

QString DaemonDBusInterface::Ping() {
    log_info("Ping() received");
    return "Pong from dynamic_power_cpp";
}

QVariantMap DaemonDBusInterface::GetDaemonState() {
    log_info("GetDaemonState() called");

    QVariantMap state;
    state.insert("active_profile", "unknown");
    state.insert("threshold_low", 1.0);
    state.insert("threshold_high", 2.0);
    state.insert("timestamp", QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    return state;
}

bool DaemonDBusInterface::SetLoadThresholds(double low, double high) {
    log_info(QString("SetLoadThresholds(%1, %2)")
             .arg(low).arg(high).toUtf8().constData());
    return true;
}

bool DaemonDBusInterface::SetPollInterval(uint interval) {
    log_info(QString("SetPollInterval(%1)").arg(interval).toUtf8().constData());
    return true;
}

bool DaemonDBusInterface::SetProfile(const QString &profile, bool is_user) {
    log_info(QString("SetProfile('%1', is_user=%2)")
             .arg(profile).arg(is_user).toUtf8().constData());
    return true;
}
