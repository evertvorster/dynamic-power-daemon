#include "config.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include "../daemon/log.h"
#include <functional>


// Single definitions for globals declared in config.h
HardwareConfig hardware{};
std::map<std::string, ProfileSetting> profiles{};
RootFeaturesConfig rootFeatures{};

static const QString TEMPLATE_CONFIG_PATH = "/usr/share/dynamic-power/dynamic_power.yaml";

namespace {

struct PersistedRootNode {
    std::string id;
    std::string parentId;
    std::string path;
    bool enabled = false;
    std::string acValue;
    std::string batteryValue;
    std::string policyScope = "override";
};

static std::string readScalar(const YAML::Node& node) {
    try {
        if (node.IsScalar()) return node.as<std::string>();
    } catch (...) {}
    try { return std::to_string(node.as<int>()); } catch (...) {}
    try { return std::to_string(node.as<double>()); } catch (...) {}
    return {};
}

static void resolveRootNodeTree(const std::vector<PersistedRootNode>& nodes, RootFeaturesConfig& out) {
    std::map<std::string, std::vector<const PersistedRootNode*>> byParent;
    std::vector<const PersistedRootNode*> roots;
    for (const auto& node : nodes) {
        if (node.parentId.empty()) roots.push_back(&node);
        byParent[node.parentId].push_back(&node);
    }

    std::function<void(const PersistedRootNode*, bool, const std::string&, const std::string&)> walk;
    walk = [&](const PersistedRootNode* node, bool inheritedEnabled,
               const std::string& inheritedAc, const std::string& inheritedBat) {
        const bool inherits = node->policyScope == "inherit";
        const bool effectiveEnabled = inherits ? inheritedEnabled : node->enabled;
        const std::string effectiveAc = inherits ? inheritedAc : node->acValue;
        const std::string effectiveBat = inherits ? inheritedBat : node->batteryValue;

        if (!node->path.empty()) {
            RootFeature rf;
            rf.enabled = effectiveEnabled;
            rf.path = node->path;
            rf.ac_value = effectiveAc;
            rf.battery_value = effectiveBat;
            out.items.push_back(std::move(rf));
        }

        const auto it = byParent.find(node->id);
        if (it == byParent.end()) return;
        for (const PersistedRootNode* child : it->second) walk(child, effectiveEnabled, effectiveAc, effectiveBat);
    };

    for (const PersistedRootNode* root : roots) walk(root, false, {}, {});
}

} // namespace

Settings Config::loadSettings(const QString& path) {
    Settings settings;
    hardware = HardwareConfig{};
    profiles.clear();
    rootFeatures = RootFeaturesConfig{};

    // Step 1: Check if config exists
    if (!QFile::exists(path)) {
        log_info(("Config file not found: " + path + ", attempting to install default").toUtf8().constData());

        // Step 2: Try to copy from template
        if (QFile::exists(TEMPLATE_CONFIG_PATH)) {
            if (!QFile::copy(TEMPLATE_CONFIG_PATH, path)) {
                log_error(("Failed to copy default config from " + TEMPLATE_CONFIG_PATH + " to " + path).toUtf8().constData());
                return settings;
            } else {
                log_info(("Default config installed to " + path).toUtf8().constData());
            }
        } else {
            log_error(("Template config missing at " + TEMPLATE_CONFIG_PATH).toUtf8().constData());
            return settings;
        }
    }

    // Step 3: Load the config
    try {
        YAML::Node root = YAML::LoadFile(path.toStdString());

        if (!root["thresholds"]) {
            log_error("Missing 'thresholds' section in config file.");
        } else {
            auto node = root["thresholds"];

            if (node["low"]) {
                settings.thresholds.low = node["low"].as<double>();
            } else {
                log_error("Missing 'low' value in thresholds.");
            }

            if (node["high"]) {
                settings.thresholds.high = node["high"].as<double>();
            } else {
                log_error("Missing 'high' value in thresholds.");
            }
        }

        if (root["grace_period"]) {
            settings.gracePeriodSeconds = root["grace_period"].as<int>();
        } else {
            log_error("Missing 'grace_period' entry in config.");
        }

        YAML::Node hwNode = root["hardware"];
        if (hwNode) {
            if (hwNode["cpu_governor"]) {
                hardware.cpu_governor.path = hwNode["cpu_governor"]["path"].as<std::string>("");
                if (hwNode["cpu_governor"]["modes"])
                    hardware.cpu_governor.modes = hwNode["cpu_governor"]["modes"].as<std::vector<std::string>>();
            }
            if (hwNode["epp_profile"]) {
                hardware.epp_profile.path = hwNode["epp_profile"]["path"].as<std::string>("");
                if (hwNode["epp_profile"]["modes"])
                    hardware.epp_profile.modes = hwNode["epp_profile"]["modes"].as<std::vector<std::string>>();
            }
            if (hwNode["acpi_platform_profile"]) {
                hardware.acpi_platform_profile.path = hwNode["acpi_platform_profile"]["path"].as<std::string>("");
                if (hwNode["acpi_platform_profile"]["modes"])
                    hardware.acpi_platform_profile.modes = hwNode["acpi_platform_profile"]["modes"].as<std::vector<std::string>>();
            }
            if (hwNode["aspm"]) {
                hardware.aspm.path = hwNode["aspm"]["path"].as<std::string>("");
                if (hwNode["aspm"]["modes"])
                    hardware.aspm.modes = hwNode["aspm"]["modes"].as<std::vector<std::string>>();
            }
        }

        if (root["profiles"]) {
            for (auto it : root["profiles"]) {
                ProfileSetting ps;
                if (it.second["cpu_governor"])
                    ps.cpu_governor = it.second["cpu_governor"].as<std::string>("");
                if (it.second["epp_profile"])
                    ps.epp_profile = it.second["epp_profile"].as<std::string>("");
                if (it.second["acpi_platform_profile"])
                    ps.acpi_platform_profile = it.second["acpi_platform_profile"].as<std::string>("");
                if (it.second["aspm"])
                    ps.aspm = it.second["aspm"].as<std::string>("");
                profiles[it.first.as<std::string>()] = ps;
            }
        }
        // Parse root features toggles
        if (root["features"] && root["features"]["root"]) {
            YAML::Node rootFeaturesNode = root["features"]["root"];
            // Disclaimer acceptance
            if (rootFeaturesNode["disclaimer"] && rootFeaturesNode["disclaimer"]["accepted"]) {
                rootFeatures.disclaimerAccepted = rootFeaturesNode["disclaimer"]["accepted"].as<bool>(false);
            }
            if (rootFeaturesNode["nodes"] && rootFeaturesNode["nodes"].IsSequence()) {
                std::vector<PersistedRootNode> nodes;
                for (const auto& item : rootFeaturesNode["nodes"]) {
                    PersistedRootNode n;
                    if (item["id"]) n.id = readScalar(item["id"]);
                    if (item["parent_id"]) n.parentId = readScalar(item["parent_id"]);
                    if (item["path"]) n.path = readScalar(item["path"]);
                    if (item["enabled"]) n.enabled = item["enabled"].as<bool>(false);
                    if (item["ac_value"]) n.acValue = readScalar(item["ac_value"]);
                    if (item["battery_value"]) n.batteryValue = readScalar(item["battery_value"]);
                    if (item["policy_scope"]) n.policyScope = readScalar(item["policy_scope"]);
                    if (n.id.empty()) n.id = n.path;
                    nodes.push_back(std::move(n));
                }
                resolveRootNodeTree(nodes, rootFeatures);
            } else if (rootFeaturesNode["features"] && rootFeaturesNode["features"].IsSequence()) {
                // COMPAT(legacy-root-features): remove this flat-list fallback after the
                // node-based features.root.nodes format has shipped for a full release cycle.
                for (const auto &item : rootFeaturesNode["features"]) {
                    RootFeature rf;
                    if (item["enabled"])       rf.enabled = item["enabled"].as<bool>(false);
                    if (item["path"])          rf.path = item["path"].as<std::string>("");
                    if (item["ac_value"])      rf.ac_value = readScalar(item["ac_value"]);
                    else if (item["ac"])       rf.ac_value = readScalar(item["ac"]);
                    if (item["battery_value"]) rf.battery_value = readScalar(item["battery_value"]);
                    else if (item["battery"])  rf.battery_value = readScalar(item["battery"]);
                    rootFeatures.items.push_back(std::move(rf));
                }
            }
        }
    } catch (const std::exception &e) {
        log_error(("Failed to parse config: " + QString(e.what())).toUtf8().constData());
    }

    return settings;
}
