#include "PanelAutohideFeature.h"
#include "FeatureBase.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QFile>
#include <QIODevice>

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
    const auto st = readState();
    return QStringLiteral("KDE Panel Autohide — AC: %1 — BAT: %2")
            .arg(st.ac, st.battery);
}

void PanelAutohideFeature::applyForPowerState(bool onBattery) const {
    const auto st = readState();
    const QString policy = normalize(onBattery ? st.battery : st.ac); // "on"/"off"/"unchanged"
    if (!st.enabled || policy == QStringLiteral("unchanged")) return;
    setAutohide(policy == QStringLiteral("on"));
}

// "on" | "off" | "unchanged"
QString PanelAutohideFeature::normalize(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "on" || t == "off") return t;
    return "unchanged";
}

// Use PlasmaShell DBus JS API to toggle all panels’ hide mode
bool PanelAutohideFeature::setAutohide(bool on) {
    QDBusInterface iface("org.kde.plasmashell", "/PlasmaShell",
                         "org.kde.PlasmaShell", QDBusConnection::sessionBus());
    if (!iface.isValid()) return false;
    const QString mode = on ? QStringLiteral("autohide") : QStringLiteral("normal");
    const QString js =
        QStringLiteral("var d=desktops();"
                       "for (var i=0;i<d.length;i++){"
                       "  var p=d[i].panels();"
                       "  for (var j=0;j<p.length;j++){ p[j].hiding='%1'; }"
                       "}").arg(mode);
    iface.call(QStringLiteral("evaluateScript"), js);
    return true;
}

} // namespace dp::features
