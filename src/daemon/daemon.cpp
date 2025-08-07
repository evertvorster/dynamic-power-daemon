#include "daemon.h"
#include "log.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>

Daemon::Daemon(QObject *parent) : QObject(parent) {
    QDBusConnection systemBus = QDBusConnection::systemBus();

    bool success = systemBus.connect(
        "org.freedesktop.PowerProfiles",                         // service name
        "/org/freedesktop/PowerProfiles",                        // object path
        "org.freedesktop.DBus.Properties",                       // interface
        "PropertiesChanged",                                     // signal name
        this, SLOT(handlePropertiesChanged(QDBusMessage))        // slot
    );

    if (success) {
        log_info("Connected to org.freedesktop.PowerProfiles signal");
    } else {
        log_error("Failed to connect to PowerProfiles signal");
    }
}

void Daemon::handlePropertiesChanged(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (args.size() < 2) return;

    QVariant interface = args.at(0);
    QVariantMap changedProps = qdbus_cast<QVariantMap>(args.at(1));

    if (interface.toString() != "org.freedesktop.PowerProfiles") return;

    if (changedProps.contains("ActiveProfile")) {
        QString profile = changedProps["ActiveProfile"].toString();
        log_info(QString("Power profile changed to: %1").arg(profile).toUtf8().constData());
    }
}
