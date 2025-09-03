#pragma once

#include <QString>
#include <string>
#include <vector>
#include <map>

static const inline QString DEFAULT_CONFIG_PATH = "/etc/dynamic_power.yaml";

struct Thresholds {
    double low = 0.0;
    double high = 0.0;
};

struct Settings {
    Thresholds thresholds;
    int gracePeriodSeconds = 0;
};

struct HardwareSetting {
    std::string path;
    std::vector<std::string> modes;
};

struct ProfileSetting {
    std::string cpu_governor;
    std::string epp_profile;
    std::string acpi_platform_profile;
    std::string aspm;
};

struct HardwareConfig {
    HardwareSetting cpu_governor;
    HardwareSetting epp_profile;
    HardwareSetting acpi_platform_profile;
    HardwareSetting aspm;
};

struct RootFeature {
    bool enabled = false;
    std::string path;
    std::string ac_value;
    std::string battery_value;
};

struct RootFeaturesConfig {
    bool disclaimerAccepted = false;
    std::vector<RootFeature> items;
};

extern RootFeaturesConfig rootFeatures;
extern HardwareConfig hardware;
extern std::map<std::string, ProfileSetting> profiles;

class Config {
public:
    static Settings loadSettings(const QString& path);
};
