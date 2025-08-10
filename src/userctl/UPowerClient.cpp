#include "UPowerClient.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariantMap>

static const char* SVC  = "org.freedesktop.UPower";
static const char* ROOT = "/org/freedesktop/UPower";
static const char* IFUP = "org.freedesktop.UPower";
static const char* IFPR = "org.freedesktop.DBus.Properties";
static const char* IFDEV= "org.freedesktop.UPower.Device";

UPowerClient::UPowerClient(QObject* parent) : QObject(parent) {
    // subscribe to PropertiesChanged on root and display device (root is enough for OnBattery)
    QDBusConnection::systemBus().connect(SVC, ROOT, IFPR, "PropertiesChanged",
                                         this, SLOT(onUPowerPropsChanged(QString,QVariantMap,QStringList)));
    refresh();
}

QString UPowerClient::displayDevicePath() const {
    QDBusMessage call = QDBusMessage::createMethodCall(SVC, ROOT, IFUP, "GetDisplayDevice");
    QDBusReply<QDBusObjectPath> reply = QDBusConnection::systemBus().call(call);
    return reply.isValid() ? reply.value().path() : QString();
}

bool UPowerClient::getBool(const QString& path, const QString& iface, const QString& prop, bool def) const {
    QDBusMessage m = QDBusMessage::createMethodCall(SVC, path, IFPR, "Get");
    m << iface << prop;
    QDBusReply<QVariant> r = QDBusConnection::systemBus().call(m);
    return r.isValid() ? r.value().toBool() : def;
}

double UPowerClient::getDouble(const QString& path, const QString& iface, const QString& prop, double def) const {
    QDBusMessage m = QDBusMessage::createMethodCall(SVC, path, IFPR, "Get");
    m << iface << prop;
    QDBusReply<QVariant> r = QDBusConnection::systemBus().call(m);
    return r.isValid() ? r.value().toDouble() : def;
}

uint UPowerClient::getUint(const QString& path, const QString& iface, const QString& prop, uint def) const {
    QDBusMessage m = QDBusMessage::createMethodCall(SVC, path, IFPR, "Get");
    m << iface << prop;
    QDBusReply<QVariant> r = QDBusConnection::systemBus().call(m);
    return r.isValid() ? r.value().toUInt() : def;
}

void UPowerClient::refresh() {
    const QString disp = displayDevicePath();
    bool onBat = getBool(ROOT, IFUP, "OnBattery", false);
    double pct = disp.isEmpty() ? 0.0 : getDouble(disp, IFDEV, "Percentage", 0.0);
    uint st = disp.isEmpty() ? 0u : getUint(disp, IFDEV, "State", 0u);
    bool changed = (onBat != m_onBattery) || (qFuzzyCompare(pct+1, m_percentage+1) == false) || (st != m_state);
    m_onBattery = onBat;
    m_percentage = pct;
    m_state = st;
    if (changed) emit powerInfoChanged();
}

void UPowerClient::onUPowerPropsChanged(QString iface, QVariantMap, QStringList) {
    if (iface == IFUP) refresh(); // OnBattery changed
}

QString UPowerClient::stateText() const {
    // Minimal mapping of UPower.Device.State
    switch (m_state) {
        case 1: return "Charging";
        case 2: return "Discharging";
        case 3: return "Empty";
        case 4: return "Fully charged";
        case 5: return "Pending charge";
        case 6: return "Pending discharge";
        default: return "Unknown";
    }
}

QString UPowerClient::summaryText() const {
    const QString src = m_onBattery ? "Battery" : "AC";
    if (m_percentage > 0.0)
        return QString("%1 • %2% (%3)").arg(src).arg(QString::number(m_percentage, 'f', 0)).arg(stateText());
    return QString("%1 (%2)").arg(src, stateText());
}
