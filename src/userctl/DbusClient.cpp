
#include "DbusClient.h"
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>
#include <QVariant>

static const char* SERVICE = "org.dynamic_power.Daemon";
static const char* PATH = "/org/dynamic_power/Daemon";
static const char* IFACE = "org.dynamic_power.Daemon";

DbusClient::DbusClient(QObject* parent)
    : QObject(parent),
      m_iface(new QDBusInterface(SERVICE, PATH, IFACE, QDBusConnection::systemBus(), this)) {
    connectSignals();
}

void DbusClient::connectSignals() {
    // PowerStateChanged signal (no args)
    QDBusConnection::systemBus().connect(
        SERVICE, PATH, IFACE, "PowerStateChanged",
        this, SLOT(powerStateChanged()));
}

void DbusClient::onPowerStateChanged() {
    emit powerStateChanged();
}

QVariantMap DbusClient::getDaemonState() const {
    QDBusReply<QVariantMap> reply = m_iface->call("GetDaemonState");
    if (reply.isValid()) {
        return reply.value();
    }
    return {};
}

bool DbusClient::setProfile(const QString& profile, bool boss) {
    QDBusReply<bool> reply = m_iface->call("SetProfile", profile, boss);
    return reply.isValid() && reply.value();
}

bool DbusClient::setLoadThresholds(double low, double high) {
    QDBusReply<bool> reply = m_iface->call("SetLoadThresholds", low, high);
    return reply.isValid() && reply.value();
}

bool DbusClient::setPollInterval(unsigned int ms) {
    QDBusReply<bool> reply = m_iface->call("SetPollInterval", ms);
    return reply.isValid() && reply.value();
}
