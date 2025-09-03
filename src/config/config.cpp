#include "config.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include "../daemon/log.h"


// Single definitions for globals declared in config.h
HardwareConfig hardware{};
std::map<std::string, ProfileSetting> profiles{};
RootFeaturesConfig rootFeatures{};

static const QString TEMPLATE_CONFIG_PATH = "/usr/share/dynamic-power/dynamic_power.yaml";

Settings Config::loadSettings(const QString& path) {
    Settings settings;

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
            // Feature list
            if (rootFeaturesNode["features"] && rootFeaturesNode["features"].IsSequence()) {
                rootFeatures.items.clear();
                for (const auto &item : rootFeaturesNode["features"]) {
                    RootFeature rf;
                    if (item["enabled"])       rf.enabled = item["enabled"].as<bool>(false);
                    if (item["path"])          rf.path = item["path"].as<std::string>("");
                    if (item["ac_value"])      rf.ac_value = item["ac_value"].as<std::string>("");
                    if (item["battery_value"]) rf.battery_value = item["battery_value"].as<std::string>("");
                    rootFeatures.items.push_back(std::move(rf));
                }
            }
        }        
    } catch (const std::exception &e) {
        log_error(("Failed to parse config: " + QString(e.what())).toUtf8().constData());
    }

    return settings;
}
