// File: src/userctl/features/RootCompositeFeature.cpp
#include "RootCompositeFeature.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace dp::features {
namespace {

using Node = RootCompositeFeature::Node;
using State = RootCompositeFeature::State;

QString classForPath(const QString& path) {
    if (path.startsWith("/proc/sys/")) return QStringLiteral("kernel");
    if (path.startsWith("/sys/module/")) return QStringLiteral("kernel");
    if (path.startsWith("/sys/firmware/")) return QStringLiteral("hardware");
    return QStringLiteral("device");
}

QString canonicalPath(const QString& path) {
    if (path.isEmpty()) return {};
    QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    return canonical.isEmpty() ? path : canonical;
}

QString readScalar(const YAML::Node& n) {
    try {
        if (n.IsScalar()) return QString::fromStdString(n.as<std::string>());
        if (n.IsNull()) return {};
        return QString::fromStdString(YAML::Dump(n));
    } catch (...) {
        try { return QString::number(n.as<int>()); } catch (...) {}
        try { return QString::number(n.as<double>()); } catch (...) {}
    }
    return {};
}

Node nodeFromYaml(const YAML::Node& n) {
    Node node;
    if (n["id"]) node.id = readScalar(n["id"]);
    if (n["parent_id"]) node.parentId = readScalar(n["parent_id"]);
    if (n["path"]) node.absPath = canonicalPath(readScalar(n["path"]));
    if (n["label"]) node.label = readScalar(n["label"]);
    if (n["class"]) node.nodeClass = readScalar(n["class"]);
    if (n["enabled"]) node.enabled = n["enabled"].as<bool>(false);
    if (n["ac_value"]) node.acValue = readScalar(n["ac_value"]);
    if (n["battery_value"]) node.batteryValue = readScalar(n["battery_value"]);
    if (n["policy_scope"]) node.policyScope = readScalar(n["policy_scope"]);
    if (node.id.isEmpty()) node.id = node.absPath.isEmpty() ? node.label : QString("node:%1").arg(node.absPath);
    if (node.label.isEmpty()) node.label = node.absPath.isEmpty() ? node.id : QFileInfo(node.absPath).fileName();
    if (node.nodeClass.isEmpty()) node.nodeClass = node.absPath.isEmpty() ? QStringLiteral("group") : classForPath(node.absPath);
    node.isGroup = node.absPath.isEmpty();
    return node;
}

Node legacyNodeFromYaml(const YAML::Node& n) {
    Node node;
    node.absPath = canonicalPath(readScalar(n["path"]));
    node.id = QString("node:%1").arg(node.absPath);
    node.label = node.absPath;
    node.nodeClass = classForPath(node.absPath);
    node.enabled = n["enabled"] ? n["enabled"].as<bool>(false) : false;
    node.acValue = n["ac_value"] ? readScalar(n["ac_value"]) : readScalar(n["ac"]);
    node.batteryValue = n["battery_value"] ? readScalar(n["battery_value"]) : readScalar(n["battery"]);
    node.policyScope = QStringLiteral("override");
    node.legacy = true;
    return node;
}

} // namespace

RootCompositeFeature::RootCompositeFeature(const QString& etcPath)
    : m_etcPath(etcPath) {}

RootCompositeFeature::State RootCompositeFeature::read() const {
    State st;
    YAML::Node root;
    try { root = YAML::LoadFile(m_etcPath.toStdString()); } catch (...) {
        root = YAML::Node(YAML::NodeType::Map);
    }

    YAML::Node fr = root["features"]["root"];
    if (fr && fr["disclaimer"]) {
        try { st.disclaimerAccepted = fr["disclaimer"]["accepted"].as<bool>(); } catch (...) {}
        try { st.acceptedAt = QString::fromStdString(fr["disclaimer"]["accepted_at"].as<std::string>()); } catch (...) {}
    }

    if (fr && fr["nodes"] && fr["nodes"].IsSequence()) {
        for (const auto& n : fr["nodes"]) st.nodes.push_back(nodeFromYaml(n));
        return st;
    }

    // COMPAT(legacy-root-features): remove this flat-list fallback after the node-based
    // features.root.nodes format has been the only supported persisted shape for a full release cycle.
    if (fr && fr["features"] && fr["features"].IsSequence()) {
        for (const auto& n : fr["features"]) st.nodes.push_back(legacyNodeFromYaml(n));
    }
    return st;
}

QByteArray RootCompositeFeature::serialize(const State& s) const {
    YAML::Node root;
    try { root = YAML::LoadFile(m_etcPath.toStdString()); } catch (...) {
        root = YAML::Node(YAML::NodeType::Map);
    }
    YAML::Node features = root["features"];
    if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);
    YAML::Node fr = features["root"];
    if (!fr || !fr.IsMap()) fr = YAML::Node(YAML::NodeType::Map);

    YAML::Node disc = fr["disclaimer"];
    if (!disc || !disc.IsMap()) disc = YAML::Node(YAML::NodeType::Map);
    disc["accepted"] = s.disclaimerAccepted;
    if (!s.acceptedAt.isEmpty()) disc["accepted_at"] = s.acceptedAt.toStdString();
    fr["disclaimer"] = disc;

    YAML::Node arr(YAML::NodeType::Sequence);
    for (const auto& node : s.nodes) {
        YAML::Node n(YAML::NodeType::Map);
        n["id"] = node.id.toStdString();
        if (!node.parentId.isEmpty()) n["parent_id"] = node.parentId.toStdString();
        if (!node.absPath.isEmpty()) n["path"] = node.absPath.toStdString();
        if (!node.label.isEmpty()) n["label"] = node.label.toStdString();
        if (!node.nodeClass.isEmpty()) n["class"] = node.nodeClass.toStdString();
        n["enabled"] = node.enabled;
        if (!node.acValue.isEmpty()) n["ac_value"] = node.acValue.toStdString();
        if (!node.batteryValue.isEmpty()) n["battery_value"] = node.batteryValue.toStdString();
        if (!node.policyScope.isEmpty()) n["policy_scope"] = node.policyScope.toStdString();
        arr.push_back(n);
    }
    fr["nodes"] = arr;
    fr.remove("features");
    features["root"] = fr;
    root["features"] = features;

    YAML::Emitter out;
    out << root;
    return QByteArray(out.c_str());
}

bool RootCompositeFeature::write(const State& s) const {
    QFileInfo fi(m_etcPath);
    QDir().mkpath(fi.dir().absolutePath());

    const QByteArray data = serialize(s);
    QFile f(m_etcPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(data);
    f.close();
    return true;
}

} // namespace dp::features
