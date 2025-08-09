
#include "Config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <yaml-cpp/yaml.h>

static const char* FILENAME_PRIMARY = "dynamic-power-user.yaml";
static const char* FILENAME_FALLBACK = "config.yaml";

Config::Config() {
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/dynamic_power";
    QDir().mkpath(cfgDir);
    QString primary = cfgDir + "/" + FILENAME_PRIMARY;
    QString fallback = cfgDir + "/" + FILENAME_FALLBACK;
    if (QFile::exists(primary)) m_path = primary;
    else if (QFile::exists(fallback)) m_path = fallback;
    else m_path = primary;
}

void Config::ensureExists() {
    if (QFile::exists(m_path)) return;
    // Try to copy from template: /usr/share/dynamic_power/dynamic-power-user.yaml
    QString tmpl = "/usr/share/dynamic_power/dynamic-power-user.yaml";
    if (QFile::exists(tmpl)) {
        QFile::copy(tmpl, m_path);
        QFile::setPermissions(m_path, QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        return;
    }
    // Otherwise create a minimal default
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream out(&f);
        out << "features:\n"
               "  kde_autohide_on_battery: false\n"
               "  auto_panel_overdrive: true\n"
               "  screen_refresh: false\n"
               "power:\n"
               "  load_thresholds:\n"
               "    low: 1\n"
               "    high: 2\n"
               "process_overrides: []\n";
    }
}

bool Config::load() {
    try {
        YAML::Node root = YAML::LoadFile(m_path.toStdString());
        if (root["power"] && root["power"]["load_thresholds"]) {
            auto lt = root["power"]["load_thresholds"];
            m_thresholds.first = lt["low"] ? lt["low"].as<double>() : 1.0;
            m_thresholds.second = lt["high"] ? lt["high"].as<double>() : 2.0;
        }
        m_rules.clear();
        if (root["process_overrides"] && root["process_overrides"].IsSequence()) {
            for (const auto& n : root["process_overrides"]) {
                ProcessRule r;
                r.name = QString::fromStdString(n["name"] ? n["name"].as<std::string>() : "");
                r.process_name = QString::fromStdString(n["process_name"] ? n["process_name"].as<std::string>() : "");
                r.active_profile = QString::fromStdString(n["active_profile"] ? n["active_profile"].as<std::string>() : "");
                r.priority = n["priority"] ? n["priority"].as<int>() : 0;
                m_rules.push_back(r);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool Config::save() const {
    YAML::Node root;
    root["features"]["kde_autohide_on_battery"] = false;
    root["features"]["auto_panel_overdrive"] = true;
    root["features"]["screen_refresh"] = false;
    root["power"]["load_thresholds"]["low"] = m_thresholds.first;
    root["power"]["load_thresholds"]["high"] = m_thresholds.second;
    YAML::Node arr(YAML::NodeType::Sequence);
    for (const auto& r : m_rules) {
        YAML::Node n;
        n["name"] = r.name.toStdString();
        n["process_name"] = r.process_name.toStdString();
        n["active_profile"] = r.active_profile.toStdString();
        n["priority"] = r.priority;
        arr.push_back(n);
    }
    root["process_overrides"] = arr;
    try {
        std::ofstream fout(m_path.toStdString());
        fout << root;
        return true;
    } catch (...) {
        return false;
    }
}
