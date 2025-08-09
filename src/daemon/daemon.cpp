#include "log.h"
#include "daemon.h"
#include "daemon_dbus_interface.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QVariantMap>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDBusError>
#include <QDBusVariant>
#include <QMap>

// Constructor.
Daemon::Daemon(const Thresholds &thresholds, 
                int graceSeconds,  
                QObject *parent)
    : QObject(parent), m_thresholds(thresholds)
{
    // Connect to the new dbus interface for user
    m_dbusInterface = new DaemonDBusInterface(this);
    QDBusConnection::systemBus().registerObject("/org/dynamic_power", this, QDBusConnection::ExportAdaptors);
    QDBusConnection::systemBus().registerService("org.dynamic_power.Daemon");

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
        updatePowerSource();
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
    if (!bus.registerObject("/org/dynamic_power/Daemon", this)) {
        log_error("Failed to register DBus object path");
    }
    // Register service name 
    if (!bus.registerService("org.dynamic_power.Daemon")) {
        log_error("Failed to register DBus service name");
    }
    // Attach the DBus adaptor to this object
    new DaemonDBusInterface(this);
    log_info("DBus service org.dynamic_power.Daemon is now live");
    // load available power profiles
    if (!loadAvailableProfiles()) {
        log_warning("Warning: Failed to load available power profiles. Switching may not work.");
    }
    // Read Grace period
    if (graceSeconds > 0) {
        m_graceActive = true;
        setProfile("performance");

        m_graceTimer = new QTimer(this);
        m_graceTimer->setSingleShot(true);
        m_graceTimer->setInterval(graceSeconds * 1000);  // convert to ms

        connect(m_graceTimer, &QTimer::timeout, this, [this]() {
            m_graceActive = false;
            log_debug("Grace period ended – resuming normal switching");
        });

        m_graceTimer->start();
        log_debug(QString("Grace period active: forcing 'performance' for %1 seconds")
                .arg(graceSeconds).toUtf8().constData());
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
            m_currentProfile = newProfile;
            log_info(QString("Confirmed active profile: %1").arg(newProfile).toUtf8().constData());
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
    // Send this signal to user space
    if (m_dbusInterface)
        Q_EMIT m_dbusInterface->PowerStateChanged();

    for (auto it = changedProps.begin(); it != changedProps.end(); ++it) {
        const QString &key = it.key();
        const QVariant &value = it.value();

        if (DEBUG_MODE) {
            log_info(QString("  %1 → %2").arg(key, value.toString()).toUtf8().constData());
        }

        if (key == "OnBattery") {
            bool onBattery = value.toBool();
            m_powerSource = onBattery ? "battery" : "AC";
            log_info(QString("Power source changed: now on %1").arg(m_powerSource).toUtf8().constData());
        }
    }
}

bool Daemon::loadAvailableProfiles()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "net.hadess.PowerProfiles",
        "/net/hadess/PowerProfiles",
        "org.freedesktop.DBus.Properties",
        "Get"
    );

    msg << "net.hadess.PowerProfiles" << "Profiles";

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);

    if (reply.type() == QDBusMessage::ErrorMessage) {
        log_error(QString("Failed to get Profiles: %1").arg(reply.errorMessage()).toUtf8().constData());
        return false;
    }

    QVariant outer = reply.arguments().at(0).value<QDBusVariant>().variant();

    // Unpack the array of {sv} maps from the QDBusArgument
    const QDBusArgument arg = outer.value<QDBusArgument>();

    m_profileMap.clear();

    arg.beginArray();
    while (!arg.atEnd()) {
        QVariantMap profileEntry;
        arg >> profileEntry;

        QString actualProfile = profileEntry.value("Profile").toString();
        if (actualProfile.isEmpty())
            continue;

        log_info(QString("Found profile: %1").arg(actualProfile).toUtf8().constData());

        if (actualProfile == "performance") {
            m_profileMap["performance"] = actualProfile;
        } else if (actualProfile == "balanced") {
            m_profileMap["balanced"] = actualProfile;
        } else if (actualProfile == "power-saver" || actualProfile == "powersave") {
            m_profileMap["powersave"] = actualProfile;
        }
    }
    arg.endArray();

    for (const QString& role : { "performance", "balanced", "powersave" }) {
        if (!m_profileMap.contains(role)) {
            log_warning(QString("Missing mapping for internal role '%1'").arg(role).toUtf8().constData());
        } else {
            log_debug(QString("Mapped internal profile '%1' → DBus profile '%2'")
                      .arg(role, m_profileMap[role])
                      .toUtf8().constData());
        }
    }

    return true;
}

bool Daemon::setProfile(const QString& internalName)
{
    if (!m_profileMap.contains(internalName)) {
        log_warning(QString("setProfile(): Unknown internal profile name '%1'")
                    .arg(internalName).toUtf8().constData());
        return false;
    }
    
    // 🆕 Fallback: if we don't know the current profile yet, ask DBus
    if (m_currentProfile.isEmpty()) {
        QDBusMessage getMsg = QDBusMessage::createMethodCall(
            "net.hadess.PowerProfiles",
            "/net/hadess/PowerProfiles",
            "org.freedesktop.DBus.Properties",
            "Get"
        );
        getMsg << "net.hadess.PowerProfiles" << "ActiveProfile";

        QDBusMessage reply = QDBusConnection::systemBus().call(getMsg);
        if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1) {
            QVariant variant = reply.arguments().at(0).value<QDBusVariant>().variant();
            m_currentProfile = variant.toString();
            log_debug(QString("Queried current profile: %1").arg(m_currentProfile).toUtf8().constData());
        } else {
            log_warning("Could not query current profile from DBus");
        }
    }
    QString actualProfile = m_profileMap.value(internalName);   
    // If the current profile is already set, skip it. 
    if (actualProfile == m_currentProfile) {
        if (DEBUG_MODE) {
            log_debug(QString("setProfile(): '%1' already active – skipping").arg(actualProfile).toUtf8().constData());
        }
        return true;
    }
    // Get the internal name for dbus.
    // m_activeProfile = internalName;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "net.hadess.PowerProfiles",               // service
        "/net/hadess/PowerProfiles",              // object path
        "org.freedesktop.DBus.Properties",        // this is the interface we're calling ON
        "Set"                                     // this is the method name
    );

    // Set(interface name, property name, value)
    msg << "net.hadess.PowerProfiles"            // target interface for the property
        << "ActiveProfile"                        // property name
        << QVariant::fromValue(QDBusVariant(actualProfile));  // value

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    
    if (reply.type() == QDBusMessage::ErrorMessage) {
        log_error(QString("setProfile(): Failed to set profile '%1': %2")
                  .arg(actualProfile, reply.errorMessage()).toUtf8().constData());
        return false;
    }

    log_debug(QString("Requested profile switch to '%1'").arg(actualProfile).toUtf8().constData());
    m_activeProfile = internalName;
    log_info(QString("Profile set to '%1' via DBus").arg(actualProfile).toUtf8().constData());   
    return true;
}

void Daemon::updatePowerSource()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower",
        "org.freedesktop.DBus.Properties",
        "Get"
    );

    msg << "org.freedesktop.UPower" << "OnBattery";

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);

    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        log_warning("Failed to read OnBattery from UPower – leaving power source unchanged");
        return;
    }

    QVariant variant = reply.arguments().at(0).value<QDBusVariant>().variant();
    bool onBattery = variant.toBool();

    m_powerSource = onBattery ? "battery" : "AC";

    log_debug(QString("Power source detected: %1").arg(m_powerSource).toUtf8().constData());
}

// ⚠️ This function is called every 5 seconds by the QTimer, and drives the daemon.
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
    double load = parts[0].toDouble(&ok);
    if (!ok) {
        log_error("Failed to convert loadavg value to double");
        return;
    }
    
    QString level;
    // Determine effective thresholds and publish them
    const bool useConfig = (m_requestedThresholds.low == 0.0 && m_requestedThresholds.high == 0.0);
    const Thresholds eff = useConfig ? m_thresholds : m_requestedThresholds;
    m_actualThresholds = eff;  // <- what DBus reports

    // debug
    log_debug(QString("Effective thresholds: low=%1, high=%2")
              .arg(m_actualThresholds.low).arg(m_actualThresholds.high)
              .toUtf8().constData());
    log_debug(QString("Requested thresholds: low=%1, high=%2")
              .arg(m_requestedThresholds.low).arg(m_requestedThresholds.high)
              .toUtf8().constData());
    log_debug(QString("Requested profile: %1 (user flag=%2)")
              .arg(m_requestedProfile)
              .arg(m_userRequestedProfile ? "true" : "false")
              .toUtf8().constData());

    // --- Determine load level from thresholds (for reporting + baseline decision)
    if (load < m_actualThresholds.low) {
        level = "low";
    } else if (load > m_actualThresholds.high) {
        level = "high";
    } else {
        level = "medium";
    }

    if (DEBUG_MODE) {
        log_info(QString("Current load: %1 (%2)")
                 .arg(load, 0, 'f', 2)
                 .arg(level)
                 .toUtf8().constData());
    }

    // --- Baseline target from thresholds
    QString targetProfile;
    if (level == "low") {
        targetProfile = "powersave";
    } else if (level == "high") {
        targetProfile = "performance";
    } else {
        targetProfile = "balanced";
    }

    // --- Optional user/boss override (wins over thresholds)
    bool hasOverride = false;
    QString overrideTarget;
    if (!m_requestedProfile.isEmpty()) {
        overrideTarget = (m_powerSource == "battery" && !m_userRequestedProfile)
                         ? "powersave"         // battery rule unless boss
                         : m_requestedProfile;
        hasOverride = true;
    }

    // Start with either override or threshold decision
    QString finalTarget = hasOverride ? overrideTarget : targetProfile;

    // --- Battery downgrade (only when no explicit override above)
    if (!hasOverride && m_powerSource == "battery" && finalTarget != "powersave") {
        if (m_currentProfile != "powersave") {
            log_debug(QString("On battery: downgrading '%1' → 'powersave'")
                      .arg(finalTarget).toUtf8().constData());
        }
        finalTarget = "powersave";
    }

    // --- Grace period last (wins over everything unless you decide otherwise)
    if (m_graceActive && finalTarget != "performance") {
        if (m_currentProfile != "performance") {
            log_debug(QString("Grace active: overriding '%1' → 'performance'")
                      .arg(finalTarget).toUtf8().constData());
        }
        finalTarget = "performance";
    }

    // --- Apply once, at the end
    if (!setProfile(finalTarget)) {
        log_error("Failed to apply profile based on load.");
    }
}
