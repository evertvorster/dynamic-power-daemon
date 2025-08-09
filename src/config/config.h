#pragma once

#include <QString>

static const inline QString DEFAULT_CONFIG_PATH = "/etc/dynamic_power.yaml";

struct Thresholds {
    double low = 0.0;
    double high = 0.0;
};

struct Settings {
    Thresholds thresholds;
    int gracePeriodSeconds = 0;
};

class Config {
public:
    static Settings loadSettings(const QString& path);
};
