// File: src/userctl/features/RootCompositeFeature.cpp
#include "RootCompositeFeature.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace dp::features {

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
    if (fr && fr["features"] && fr["features"].IsSequence()) {
        for (const auto& n : fr["features"]) {
            Item it;
            try { it.absPath = QString::fromStdString(n["path"].as<std::string>()); } catch (...) { it.absPath = QStringLiteral("/sys/"); }
            try { it.enabled = n["enabled"].as<bool>(); } catch (...) { it.enabled = false; }
            try { it.ac = QString::fromStdString(n["ac"].as<std::string>()); } catch (...) {}
            try { it.battery = QString::fromStdString(n["battery"].as<std::string>()); } catch (...) {}
            st.items.push_back(it);
        }
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
    if (!s.acceptedAt.isEmpty())
        disc["accepted_at"] = s.acceptedAt.toStdString();
    fr["disclaimer"] = disc;

    YAML::Node arr(YAML::NodeType::Sequence);
    for (const auto& it : s.items) {
        YAML::Node n(YAML::NodeType::Map);
        n["enabled"] = it.enabled;
        n["path"]    = it.absPath.toStdString();
        if (!it.ac.isEmpty()) n["ac"] = it.ac.toStdString();
        if (!it.battery.isEmpty()) n["battery"] = it.battery.toStdString();
        arr.push_back(n);
    }
    fr["features"] = arr;
    features["root"] = fr;
    root["features"] = features;

    YAML::Emitter out;
    out << root;
    QByteArray data(out.c_str());
    return data;
}

bool RootCompositeFeature::write(const State& s) const {
    QFileInfo fi(m_etcPath);
    QDir().mkpath(fi.dir().absolutePath());

    QByteArray data = serialize(s);
    QFile f(m_etcPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(data);
    f.close();
    return true;
}

} // namespace dp::features
