#include "PanelAutohideFeature.h"
#include "FeatureBase.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QFile>
#include <QIODevice>
#include <QGuiApplication>
#include <QScreen>

#include <yaml-cpp/yaml.h>

namespace dp::features {
using FB = FeatureBase;
using K  = Keys;

PanelAutohideFeature::State PanelAutohideFeature::readState() const {
    State st;
    YAML::Node root;
    try { root = YAML::LoadFile(FB::configPath().toStdString()); }
    catch (...) { root = YAML::Node(YAML::NodeType::Map); }

    if (root && root[K::features] && root[K::features][K::user]
        && root[K::features][K::user][K::kde_panel_autohide]) {
        const auto n = root[K::features][K::user][K::kde_panel_autohide];
        if (n[K::enabled])  st.enabled = n[K::enabled].as<bool>();
        if (n[K::ac])       st.ac = QString::fromStdString(n[K::ac].as<std::string>());
        if (n[K::battery])  st.battery = QString::fromStdString(n[K::battery].as<std::string>());
    }
    return st; // no read-side normalization (we only normalize on write)
}

bool PanelAutohideFeature::writeState(const State& s) const {
    YAML::Node root;
    try { root = YAML::LoadFile(FB::configPath().toStdString()); }
    catch (...) { root = YAML::Node(YAML::NodeType::Map); }

    YAML::Node features = root[K::features];
    if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);

    YAML::Node user = features[K::user];
    if (!user || !user.IsMap()) user = YAML::Node(YAML::NodeType::Map);

    YAML::Node pa = user[K::kde_panel_autohide];
    if (!pa || !pa.IsMap()) pa = YAML::Node(YAML::NodeType::Map);

    pa[K::enabled] = s.enabled;
    pa[K::ac]      = normalize(s.ac).toStdString();
    pa[K::battery] = normalize(s.battery).toStdString();

    user[K::kde_panel_autohide] = pa;
    features[K::user] = user;
    root[K::features] = features;

    YAML::Emitter out; out << root;
    QFile f(FB::configPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(out.c_str());
    return true;
}

QString PanelAutohideFeature::statusText() const {
    QDBusInterface iface("org.kde.plasmashell", "/PlasmaShell",
                         "org.kde.PlasmaShell", QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return QStringLiteral("KDE Panel Autohide — Status: unavailable (PlasmaShell DBus invalid)");
    }

    // Ask Plasma for each panel's screen index and hiding mode
    const QString js = QStringLiteral(
        "panels().forEach(p => print(p.screen + ':' + p.hiding + '\\n'))"
    );
    const QDBusMessage reply = iface.call(QStringLiteral("evaluateScript"), js);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return QStringLiteral("KDE Panel Autohide — Status: unknown");
    }

    const QString out = reply.arguments().at(0).toString();
    const auto screens = QGuiApplication::screens();

    // Aggregate by screen index (one summary per output)
    QMap<int, QString> modePerScreen;
    const auto lines = out.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const int sep = line.indexOf(':');
        if (sep <= 0) continue;
        bool ok = false;
        const int idx = line.left(sep).toInt(&ok);
        if (!ok) continue;
        const QString mode = line.mid(sep + 1).trimmed();
        modePerScreen[idx] = mode; // last one wins if multiple panels on same screen
    }

    if (modePerScreen.isEmpty()) {
        return QStringLiteral("KDE Panel Autohide — no panels detected");
    }

    QStringList parts;
    for (auto it = modePerScreen.cbegin(); it != modePerScreen.cend(); ++it) {
        QString name;
        if (it.key() >= 0 && it.key() < screens.size()) {
            name = screens.at(it.key())->name();      // e.g. "eDP-1", "HDMI-A-1"
        } else {
            name = QStringLiteral("Screen %1").arg(it.key());
        }
        parts << QStringLiteral("%1: %2").arg(name, it.value());
    }
    return QStringLiteral("KDE Panel Autohide — %1").arg(parts.join(QStringLiteral(", ")));
}

void PanelAutohideFeature::applyForPowerState(bool onBattery) const {
    const auto st = readState();
    const QString policy = normalize(onBattery ? st.battery : st.ac); // canonical
    if (!st.enabled || policy == QStringLiteral("unchanged")) return;
    setAutohide(policy);
}

// canonical → none/autohide/dodgewindows/windowsgobelow/unchanged
QString PanelAutohideFeature::normalize(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "none" || t == "off" || t == "normal" || t == "always") return "none";
    if (t == "autohide" || t == "on") return "autohide";
    if (t == "dodgewindows" || t == "dodge") return "dodgewindows";
    if (t == "windowsgobelow" || t == "windowsbelow" || t == "below") return "windowsgobelow";
    return "unchanged";
}

// Use PlasmaShell DBus JS API to toggle all panels’ hide mode
bool PanelAutohideFeature::setAutohide(const QString& mode) {
    QDBusInterface iface("org.kde.plasmashell", "/PlasmaShell",
                         "org.kde.PlasmaShell", QDBusConnection::sessionBus());
    if (!iface.isValid()) return false;

    const QString m = normalize(mode); // accept synonyms here
    if (m == QStringLiteral("unchanged")) return true; // no-op by design

    // Plasma 6: panels().forEach(p => p.hiding = "<mode>")
    const QString js = QStringLiteral("panels().forEach(p => p.hiding = '%1')").arg(m);
    iface.call(QStringLiteral("evaluateScript"), js);
    return true;
}

} // namespace dp::features
