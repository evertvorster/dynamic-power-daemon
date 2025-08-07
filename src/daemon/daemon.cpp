#include "daemon.h"
#include "log.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDBusError>
#include "dbus_adaptor.h"

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

    // Create a timer that fires every 5 seconds (5000 ms)
    m_timer = new QTimer(this);
    // Connect the timer’s timeout signal to our checkLoadAverage() slot
    connect(m_timer, &QTimer::timeout, this, &Daemon::checkLoadAverage);
    // Set the interval to 5 seconds (don’t go below this — loadavg updates every 5s)
    m_timer->setInterval(5000);
    // Start the timer — it begins ticking immediately
    m_timer->start();
    if (DEBUG_MODE) {
        log_info("QTimer started for load average polling every 5s");
    }

    // ✅ Register DBus object and name
    QDBusConnection bus = QDBusConnection::systemBus();
    // Export our daemon object at a custom path
    if (!bus.registerObject("/org/dynamic_power/DaemonCpp", this)) {
        log_error("Failed to register DBus object path");
    }
    // Register service name (non-conflicting with Python daemon)
    if (!bus.registerService("org.dynamic_power.DaemonCpp")) {
        log_error("Failed to register DBus service name");
    }
    // Attach the DBus adaptor to this object
    new DynamicPowerAdaptor(this);
    log_info("DBus service org.dynamic_power.DaemonCpp is now live");
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

// This function is called every 5 seconds by the QTimer
void Daemon::checkLoadAverage() {
    // Open /proc/loadavg to read the current 1-minute load average
    QFile file("/proc/loadavg");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        log_error("Failed to read /proc/loadavg");
        return;
    }

    QTextStream in(&file);
    QString line = in.readLine();  // example: "0.33 0.47 0.59 1/1234 56789"
    file.close();

    // Split the line into parts and parse the first value as the 1-minute load average
    QStringList parts = line.split(" ");
    if (parts.isEmpty()) {
        log_error("Failed to parse loadavg line");
        return;
    }

    bool ok = false;
    double load = parts[0].toDouble(&ok);  // converts "0.33" to 0.33
    if (!ok) {
        log_error("Failed to convert loadavg value to double");
        return;
    }

    QString level;
    if (load < m_thresholds.low) {
        level = "low";
    } else if (load > m_thresholds.high) {
        level = "high";
    } else {
        level = "medium";
    }

    if (DEBUG_MODE) {
        log_info(QString("Current load: %1 (%2)")
                 .arg(load, 0, 'f', 2)  // format to 2 decimal places
                 .arg(level)
                 .toUtf8().constData());
    }

    // ⚠️ In future: add logic here to switch power profile or notify other components
}
