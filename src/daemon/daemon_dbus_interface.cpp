#include "log.h"
#include "daemon.h"
#include "daemon_dbus_interface.h"
#include <QDateTime> 

DaemonDBusInterface::DaemonDBusInterface(Daemon *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
    log_info("DBus adaptor constructed");
}

QString DaemonDBusInterface::Ping() {
    log_info("Ping() received");
    return "Pong from dynamic_power";
}

QVariantMap DaemonDBusInterface::GetDaemonState() {
    log_debug("GetDaemonState() called");

    Daemon* daemon = static_cast<Daemon*>(parent());

    QVariantMap state;
    state.insert("active_profile", daemon->getActiveProfile());
    state.insert("threshold_low", daemon->getLowThreshold());
    state.insert("threshold_high", daemon->getHighThreshold());
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
