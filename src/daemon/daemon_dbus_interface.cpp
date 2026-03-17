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
    QString ap = daemon->getActiveProfile();
    state.insert("active_profile", ap.isEmpty() ? QString("Error") : ap);
    state.insert("threshold_low",  daemon->getLowThreshold());
    state.insert("threshold_high", daemon->getHighThreshold());
    state.insert("timestamp", QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    state.insert("power_source",   daemon->getPowerSource());
    state.insert("battery_state",  daemon->getBatteryState());
    return state;
}

bool DaemonDBusInterface::SetLoadThresholds(double low, double high) {
    log_debug(QString("SetLoadThresholds(%1, %2)").arg(low).arg(high).toUtf8().constData());
    auto *daemon = static_cast<Daemon*>(parent());
    daemon->setRequestedThresholds(low, high);
    daemon->refreshState();
    return true;
}

bool DaemonDBusInterface::SetPollInterval(uint interval) {
    auto *daemon = qobject_cast<Daemon*>(parent());
    return daemon && daemon->setPollInterval(interval);
}

bool DaemonDBusInterface::SetProfile(const QString& name, bool userRequested) {
    log_debug(QString("SetProfile('%1', user=%2)")
              .arg(name).arg(userRequested ? "true" : "false")
              .toUtf8().constData());

    auto *daemon = qobject_cast<Daemon*>(parent());
    if (!daemon) return false;

    daemon->setRequestedProfile(name, userRequested);  // observational only
    daemon->refreshState();
    return true;
}
