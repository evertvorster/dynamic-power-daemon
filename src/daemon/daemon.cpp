#include "daemon.h"
#include "log.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>

Daemon::Daemon(const Thresholds &thresholds, QObject *parent)
    : QObject(parent), m_thresholds(thresholds)
{
    // Connection to power profiles daemon
    bool success = QDBusConnection::systemBus().connect(
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(handlePropertiesChanged(QDBusMessage))
    );

    if (success) {
        log_info("Connected to org.freedesktop.PowerProfiles signal");
    } else {
        log_error("Failed to connect to PowerProfiles signal");
    }

    // Connection to Upower
    bool upowerConnected = QDBusConnection::systemBus().connect(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(handleUPowerChanged(QDBusMessage))
    );

    if (upowerConnected) {
        log_info("Connected to org.freedesktop.UPower signal");
    } else {
        log_error("Failed to connect to UPower signal");
    }
}



void Daemon::handlePropertiesChanged(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (args.size() < 2) return;

    QString interface = args.at(0).toString();
    QVariantMap changedProps = qdbus_cast<QVariantMap>(args.at(1));

    if (DEBUG_MODE) {
        log_info(QString("DBus PropertiesChanged from interface: %1").arg(interface).toUtf8().constData());
    }

    for (auto it = changedProps.begin(); it != changedProps.end(); ++it) {
        const QString &key = it.key();
        const QVariant &value = it.value();

        if (DEBUG_MODE) {
            QString val = value.toString();
            log_info(QString("  %1 → %2").arg(key, val).toUtf8().constData());
        }

        // ✅ you can still act on the key/value here regardless of mode
        if (key == "ActiveProfile") {
            QString newProfile = value.toString();
            // TODO: take action based on newProfile
        }
    }
}

void Daemon::handleUPowerChanged(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (args.size() < 2) return;

    QString interface = args.at(0).toString();
    QVariantMap changedProps = qdbus_cast<QVariantMap>(args.at(1));

    if (DEBUG_MODE) {
        log_info(QString("UPower PropertiesChanged from interface: %1").arg(interface).toUtf8().constData());
    }

    for (auto it = changedProps.begin(); it != changedProps.end(); ++it) {
        const QString &key = it.key();
        const QVariant &value = it.value();

        if (DEBUG_MODE) {
            log_info(QString("  %1 → %2").arg(key, value.toString()).toUtf8().constData());
        }

        if (key == "OnBattery") {
            bool onBattery = value.toBool();
            // TODO: react to power source change here
        }
    }
}

