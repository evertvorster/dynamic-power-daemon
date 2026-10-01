#include "log.h"
#include "daemon.h"
#include "daemon_dbus_interface.h"
#include "cpu_targets.h"

using dp::daemon::cpuTargetsFor;
using dp::daemon::availableValuesFileName;
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
#include <vector>

// Helpers

static QString toBatteryStateString(int s) {
    switch (s) {
    case 1: return "charging";
    case 2: return "discharging";
    case 3: return "empty";
    case 4: return "charged";
    case 5: return "pending-charge";
    case 6: return "pending-discharge";
    default: return "unknown";
    }
}

// Constructor.
Daemon::Daemon(const Thresholds &thresholds, 
                int graceSeconds,  
                QObject *parent)
    : QObject(parent), m_thresholds(thresholds), m_actualThresholds(thresholds)
{
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

    // Listen for battery DisplayDevice state changes
    bool deviceConnected = QDBusConnection::systemBus().connect(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(handleUPowerDeviceChanged(QDBusMessage))
    );
    if (deviceConnected) {
        log_info("Connected to UPower DisplayDevice signal");
        updateBatteryState(); // prime cached state
    } else {
        log_error("Failed to connect to UPower DisplayDevice signal");
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

    // Register DBus object, service, and adaptor once.
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.registerObject("/org/dynamic_power/Daemon", this)) {
        log_error("Failed to register DBus object path");
    }
    if (!bus.registerService("org.dynamic_power.Daemon")) {
        log_error("Failed to register DBus service name");
    }
    m_dbusInterface = new DaemonDBusInterface(this);
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
    
    // Watch the config for changes and hot-reload thresholds/profiles
    m_configWatcher = new QFileSystemWatcher(this);
    m_configWatcher->addPath(DEFAULT_CONFIG_PATH);
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, &Daemon::onConfigFileChanged);

    refreshState();
}

void Daemon::handleUPowerDeviceChanged(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (args.size() < 2) return;

    QVariantMap changedProps = qdbus_cast<QVariantMap>(args.at(1));
    if (changedProps.contains("State")) {
        int st = changedProps.value("State").toInt();
        m_batteryState = toBatteryStateString(st);
        log_debug(QString("Battery state changed: %1").arg(m_batteryState).toUtf8().constData());
        emitPowerStateChanged();
    }
}


void Daemon::onConfigFileChanged(const QString &path)
{
    log_info(QString("Config changed on disk: %1 — reloading").arg(path).toUtf8().constData());

    // Reload settings (also repopulates global 'hardware' and 'profiles')
    Settings s = Config::loadSettings(DEFAULT_CONFIG_PATH);

    // Apply new thresholds immediately (requested overrides still win)
    m_thresholds = s.thresholds;
    loadAvailableProfiles();
    applyRootPowerTweaks();
    refreshState();
    log_info(QString("New thresholds: low=%1 high=%2")
             .arg(m_thresholds.low).arg(m_thresholds.high).toUtf8().constData());

    // Re-arm the watcher (accounts for editors that replace the file)
    if (QFile::exists(DEFAULT_CONFIG_PATH))
        m_configWatcher->addPath(DEFAULT_CONFIG_PATH);
}


void Daemon::handleUPowerChanged(const QDBusMessage &message) {
    const auto args = message.arguments();
    if (args.size() < 2) return;

    QString interface = args.at(0).toString();
    QVariantMap changedProps = qdbus_cast<QVariantMap>(args.at(1));

    if (DEBUG_MODE) {
        log_info(QString("UPower PropertiesChanged from interface: %1").arg(interface).toUtf8().constData());
    }
    // Refresh cached power source and notify UI once per event
    updatePowerSource();

    for (auto it = changedProps.begin(); it != changedProps.end(); ++it) {
        const QString &key = it.key();
        const QVariant &value = it.value();

        if (DEBUG_MODE) {
            log_info(QString("  %1 → %2").arg(key, value.toString()).toUtf8().constData());
        }
    }
}

bool Daemon::loadAvailableProfiles()
{
    m_availableProfiles.clear();

    // Populate from YAML-defined profiles (config.h globals)
    for (const auto &kv : profiles) {
        m_availableProfiles.insert(QString::fromStdString(kv.first));
    }

    // Sanity: warn if our canonical trio are missing
    for (const char *role : { "performance", "balanced", "powersave" }) {
        if (!m_availableProfiles.contains(role)) {
            log_warning(QString("Missing profile '%1' in the config").arg(role).toUtf8().constData());
        } else {
            log_debug(QString("Profile '%1' is configured").arg(role).toUtf8().constData());
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

    namespace fs = std::filesystem;
    // Look up the desired profile in YAML
    auto it = profiles.find(key);
    if (it == profiles.end()) {
        log_warning(QString("setProfile(): Unknown internal profile name '%1'")
                    .arg(internalName).toUtf8().constData());
        return false;
    }
    const ProfileSetting &ps = it->second;

    // Track what's been applied (keyed by "label = value") to collapse repeated writes
    QMap<QString, int> appliedCount;

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
        appliedCount[QString("%1 = %2").arg(label, QString::fromStdString(value))]++;
        return true;
    };

    // Helper to read a single line from a sysfs file (best-effort). Returns empty on failure.
    auto read_sysfs_line = [&](const std::string &path) -> std::string {
        if (path.empty()) return {};
        std::ifstream ifs(path);
        if (!ifs.is_open()) return {};
        std::string line;
        std::getline(ifs, line);
        // trim trailing whitespace/newlines
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || std::isspace(static_cast<unsigned char>(line.back())))) {
            line.pop_back();
        }
        return line;
    };


    // Treat "Disabled" (case-insensitive) as a no-op for this knob
    auto is_disabled = [](const std::string& v) -> bool {
        return QString::fromStdString(v).compare("disabled", Qt::CaseInsensitive) == 0;
    };

    // Write the governor to all CPU policy/cpu nodes based on the single path in config
    // Write one field to every CPU node the config's single example path implies,
    // returning the nodes that refused so the caller can explain it in its terms.
    // An empty return means every target took the value.
    //
    // The old code had a copy of this per field, and the copies had drifted: the
    // governor path reported a failed write as success, and only the EPP path said
    // anything when the kernel refused a value.
    auto write_all = [&](const std::string& configuredPath, const std::string& value,
                         const std::string& fieldName, const char* label) {
        std::vector<std::string> rejected;
        if (configuredPath.empty() || value.empty()) return rejected;
        for (const std::string& target : cpuTargetsFor(configuredPath, fieldName)) {
            if (!write_value(target, value, label)) rejected.push_back(target);
        }
        return rejected;
    };

    // A refused value is the one failure worth explaining: say what the kernel would
    // have accepted. The governor is included because EPP acceptance depends on it.
    auto report_rejections = [&](const std::vector<std::string>& rejected,
                                 const std::string& value, const std::string& fieldName) {
        for (const std::string& target : rejected) {
            const fs::path dir = fs::path(target).parent_path();
            const std::string avail = read_sysfs_line((dir / availableValuesFileName(fieldName)).string());
            const std::string gov   = read_sysfs_line((dir / "scaling_governor").string());
            log_error(QString("setProfile(): request '%1' rejected at %2 (governor=%3; available=%4)")
                      .arg(QString::fromStdString(value),
                           QString::fromStdString(target),
                           QString::fromStdString(gov.empty()   ? std::string("<unknown>") : gov),
                           QString::fromStdString(avail.empty() ? std::string("<unknown>") : avail))
                      .toUtf8().constData());
        }
    };


    // A knob the kernel refuses is a warning, not a failure. The valid values for one
    // knob can depend on another - EPP follows the governor - so a mapping can be
    // partly invalid on one machine and entirely fine on another. No shipped default
    // can be right everywhere, which is why one refused knob must not mark the whole
    // profile as failed. Only a profile that achieved nothing is an error.
    bool anyApplied = false;
    QStringList skipped;

    auto noteSkipped = [&skipped](const char* label, const std::string& value) {
        skipped << QStringLiteral("%1 = %2").arg(QString::fromUtf8(label),
                                                  QString::fromStdString(value));
    };

    if (!is_disabled(ps.cpu_governor)) {
        const auto rejected = write_all(hardware.cpu_governor.path, ps.cpu_governor,
                                        "scaling_governor", "cpu_governor");
        report_rejections(rejected, ps.cpu_governor, "scaling_governor");
        if (rejected.empty()) anyApplied = true;
        else noteSkipped("cpu_governor", ps.cpu_governor);
    } else
        log_info("cpu_governor disabled in profile; skipping write");

    if (!is_disabled(ps.epp_profile)) {
        const auto rejected = write_all(hardware.epp_profile.path, ps.epp_profile,
                                        "energy_performance_preference", "epp_profile");
        report_rejections(rejected, ps.epp_profile, "energy_performance_preference");
        if (rejected.empty()) anyApplied = true;
        else noteSkipped("epp_profile", ps.epp_profile);
    } else
        log_info("epp_profile disabled in profile; skipping write");

    if (!is_disabled(ps.acpi_platform_profile)) {
        if (write_value(hardware.acpi_platform_profile.path, ps.acpi_platform_profile, "acpi_platform_profile"))
            anyApplied = true;
        else
            noteSkipped("acpi_platform_profile", ps.acpi_platform_profile);
    } else
        log_info("acpi_platform_profile disabled in profile; skipping write");

    if (!is_disabled(ps.aspm)) {
        if (write_value(hardware.aspm.path, ps.aspm, "aspm"))
            anyApplied = true;
        else
            noteSkipped("aspm", ps.aspm);
    } else
        log_info("aspm disabled in profile; skipping write");

    // What was refused, for the user session to show. Cleared whenever a profile
    // applies cleanly, so it heals itself rather than going stale.
    m_lastSkipped = skipped.join(QStringLiteral("; "));

    if (!anyApplied) {
        log_error(QString("setProfile(): no setting could be applied for '%1'")
                  .arg(internalName).toUtf8().constData());
        return false;
    }
    if (!skipped.isEmpty())
        log_warning(QString("setProfile(): applied '%1', but %2 setting(s) were refused by the kernel: %3")
                    .arg(internalName).arg(skipped.size()).arg(m_lastSkipped).toUtf8().constData());

    // Summarise what was applied (collapsing repeated writes like per-CPU governor/EPP)
    for (auto it = appliedCount.constBegin(); it != appliedCount.constEnd(); ++it) {
        if (it.value() > 1)
            log_info(QString("Applied %1 (%2\u00d7)").arg(it.key()).arg(it.value()).toUtf8().constData());
        else
            log_info(QString("Applied %1").arg(it.key()).toUtf8().constData());
    }

    // Track what we just applied
    m_currentProfile = internalName;       // actual active (for our daemon)
    m_activeProfile  = internalName;       // reflected to our DBus iface
    log_info(QString("Profile switched to '%1'").arg(internalName).toUtf8().constData());
    Q_EMIT ProfileChanged(internalName);
    emitPowerStateChanged();
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
    applyRootPowerTweaks();
    // Notify UI/subscribers even at startup (Unknown → AC/BAT) and on any change
    emitPowerStateChanged();
}

void Daemon::emitPowerStateChanged()
{
    if (m_dbusInterface)
        Q_EMIT m_dbusInterface->PowerStateChanged();
}

bool Daemon::setPollInterval(uint intervalMs)
{
    if (!m_timer || intervalMs < 5000) {
        log_warning(QString("Rejected poll interval %1 ms (minimum is 5000 ms)")
                    .arg(intervalMs).toUtf8().constData());
        return false;
    }

    m_timer->setInterval(static_cast<int>(intervalMs));
    log_info(QString("Updated poll interval to %1 ms").arg(intervalMs).toUtf8().constData());
    return true;
}

void Daemon::refreshState()
{
    checkLoadAverage();
}

void Daemon::updateBatteryState()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice",
        "org.freedesktop.DBus.Properties",
        "Get"
    );
    msg << "org.freedesktop.UPower.Device" << "State";

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        log_warning("Failed to read battery State from UPower");
        return;
    }

    QVariant variant = reply.arguments().at(0).value<QDBusVariant>().variant();
    int st = variant.toInt();
    m_batteryState = toBatteryStateString(st);
    log_debug(QString("Battery state detected: %1").arg(m_batteryState).toUtf8().constData());
}

void Daemon::applyRootPowerTweaks()
{
    // Only proceed if disclaimer has been accepted
    if (!rootFeatures.disclaimerAccepted) {
        log_info("Root power tweaks disabled (disclaimer not accepted)");
        return;
    }
    for (const auto &feat : rootFeatures.items) {
        if (!feat.enabled) continue;
        const QString p = QString::fromStdString(feat.path);
        const std::string value = (m_powerSource == "AC")
                                  ? feat.ac_value
                                  : feat.battery_value;
        if (p.isEmpty() || value.empty()) continue;

        if (!QFile::exists(p)) {
            log_error(QString("Root tweak path missing: %1").arg(p).toUtf8().constData());
            continue;
        }

        std::ofstream ofs(p.toStdString());
        if (!ofs) {
            log_error(QString("Root tweak not writable: %1").arg(p).toUtf8().constData());
            continue;
        }
        ofs << value << std::endl;
        if (!ofs.good()) {
            log_error(QString("Root tweak write failed to %1").arg(p).toUtf8().constData());
            continue;
        }
        log_info(QString("Root tweak applied %1 = %2").arg(p, QString::fromStdString(value)).toUtf8().constData());
    }
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
        m_activeProfile = "Error";
        Q_EMIT ProfileChanged(QStringLiteral("Error"));
        log_error("Failed to apply profile based on load.");
    }
}
