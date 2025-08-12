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
#include <algorithm>
#include <fstream>
#include <string>
#include <filesystem>
#include <cctype>

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

    // Watch the config for changes and hot-reload thresholds/profiles
    m_configWatcher = new QFileSystemWatcher(this);
    m_configWatcher->addPath(DEFAULT_CONFIG_PATH);
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, &Daemon::onConfigFileChanged);

    }
}

void Daemon::onConfigFileChanged(const QString &path)
{
    log_info(QString("Config changed on disk: %1 — reloading").arg(path).toUtf8().constData());

    // Reload settings (also repopulates global 'hardware' and 'profiles')
    Settings s = Config::loadSettings(DEFAULT_CONFIG_PATH);

    // Apply new thresholds immediately (requested overrides still win)
    m_thresholds = s.thresholds;
    log_info(QString("New thresholds: low=%1 high=%2")
             .arg(m_thresholds.low).arg(m_thresholds.high).toUtf8().constData());

    // Re-arm the watcher (accounts for editors that replace the file)
    if (QFile::exists(DEFAULT_CONFIG_PATH))
        m_configWatcher->addPath(DEFAULT_CONFIG_PATH);
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
    m_profileMap.clear();

    // Populate from YAML-defined profiles (config.h globals)
    for (const auto &kv : profiles) {
        const QString name = QString::fromStdString(kv.first);
        m_profileMap[name] = name; // internal → same name
    }

    // Sanity: warn if our canonical trio are missing
    for (const QString &role : { "performance", "balanced", "powersave" }) {
        if (!m_profileMap.contains(role)) {
            log_warning(QString("Missing mapping for internal role '%1'").arg(role).toUtf8().constData());
        } else {
            log_debug(QString("Mapped internal profile '%1'").arg(role).toUtf8().constData());
        }
    }
    return true;
}

bool Daemon::setProfile(const QString& internalName)
{
    // Skip redundant apply if the requested profile is already active.
    if (internalName == m_currentProfile) {
        log_debug("setProfile(): requested profile equals current; skipping.");
        return true;
    }
    const std::string key = internalName.toStdString();
    
    // Look up the desired profile in YAML
    auto it = profiles.find(key);
    if (it == profiles.end()) {
        log_warning(QString("setProfile(): Unknown internal profile name '%1'")
                    .arg(internalName).toUtf8().constData());
        return false;
    }
    const ProfileSetting &ps = it->second;

    // Helper to write a single value to a sysfs path
    auto write_value = [&](const std::string &path, const std::string &value, const char *label) -> bool {
        if (path.empty() || value.empty()) {
            // Nothing to do for this knob
            return true;
        }
        std::ofstream ofs(path);
        if (!ofs) {
            log_error(QString("setProfile(): cannot open %1 path '%2'")
                      .arg(label, QString::fromStdString(path)).toUtf8().constData());
            return false;
        }
        ofs << value << std::endl;
        if (!ofs.good()) {
            log_error(QString("setProfile(): failed writing '%1' to %2")
                      .arg(QString::fromStdString(value), QString::fromStdString(path)).toUtf8().constData());
            return false;
        }
        log_info(QString("Applied %1 = %2").arg(label, QString::fromStdString(value)).toUtf8().constData());
        return true;
    };

    // Treat "Disabled" (case-insensitive) as a no-op for this knob
    auto is_disabled = [](const std::string& v) -> bool {
        return QString::fromStdString(v).compare("disabled", Qt::CaseInsensitive) == 0;
    };

    // Write the governor to all CPU policy/cpu nodes based on the single path in config
    auto write_governor_all = [&](const std::string &path, const std::string &value) -> bool {
        if (path.empty() || value.empty()) return true;

        namespace fs = std::filesystem;
        fs::path p(path);
        bool all_ok = true;
        std::error_code ec;

        if (p.filename() == "scaling_governor") {
            fs::path parent = p.parent_path(); // .../policyX or .../cpufreq
            const std::string parent_name = parent.filename().string();

            // Case A: /sys/.../cpufreq/policyX/scaling_governor  → write to all policyN/scaling_governor
            fs::path root = parent.parent_path(); // .../cpufreq
            for (const auto &e : fs::directory_iterator(root, ec)) {
                if (!e.is_directory(ec)) continue;
                const std::string name = e.path().filename().string();
                // only policyN dirs (skip anything else)
                bool is_policyN = name.rfind("policy", 0) == 0 &&
                                std::all_of(name.begin() + 6, name.end(),
                                            [](unsigned char ch){ return std::isdigit(ch); });
                if (!is_policyN) continue;
                fs::path target = e.path() / "scaling_governor";
                all_ok &= write_value(target.string(), value, "cpu_governor");
            }
            return all_ok;


            // Case B: /sys/.../cpu/cpuX/cpufreq/scaling_governor  → write to all cpuN/cpufreq/scaling_governor
            fs::path cpu_root = parent.parent_path().parent_path(); // .../cpu

            // Probe just cpu0 as a sanity check (your requested behavior)
            fs::path probe = cpu_root / "cpu0" / "cpufreq" / "scaling_governor";
            if (!fs::exists(probe, ec)) {
                // If cpu0 path isn't present, fall back to the single configured node
                return write_value(path, value, "cpu_governor");
            }

            for (const auto &e : fs::directory_iterator(cpu_root, ec)) {
                if (!e.is_directory(ec)) continue;
                const std::string name = e.path().filename().string();
                // only cpuN dirs (skip 'cpufreq' and anything else)
                bool is_cpuN = name.rfind("cpu", 0) == 0 &&
                            std::all_of(name.begin() + 3, name.end(),
                                        [](unsigned char ch){ return std::isdigit(ch); });
                if (!is_cpuN) continue;
                fs::path target = e.path() / "cpufreq" / "scaling_governor";
                all_ok &= write_value(target.string(), value, "cpu_governor");
            }
            return all_ok;

        }

        // Fallback: single write (if the configured path isn’t a known pattern)
        return write_value(path, value, "cpu_governor");
    };


    bool ok = true;
    if (!is_disabled(ps.cpu_governor))
        ok &= write_value(hardware.cpu_governor.path,          ps.cpu_governor,          "cpu_governor");
    else
        log_info("cpu_governor disabled in profile; skipping write");

    if (!is_disabled(ps.acpi_platform_profile))
        ok &= write_value(hardware.acpi_platform_profile.path, ps.acpi_platform_profile, "acpi_platform_profile");
    else
        log_info("acpi_platform_profile disabled in profile; skipping write");

    if (!is_disabled(ps.aspm))
        ok &= write_value(hardware.aspm.path,                  ps.aspm,                  "aspm");
    else
        log_info("aspm disabled in profile; skipping write");
    if (!ok) {
        log_error(QString("setProfile(): one or more hardware writes failed for '%1'")
                  .arg(internalName).toUtf8().constData());
        return false;
    }

    // Track what we just applied
    m_currentProfile = internalName;       // actual active (for our daemon)
    m_activeProfile  = internalName;       // reflected to our DBus iface
    log_info(QString("Profile switched to '%1'").arg(internalName).toUtf8().constData());
    if (m_dbusInterface)
        Q_EMIT m_dbusInterface->PowerStateChanged();
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
