#pragma once

#include <QString>

struct Thresholds {
    double low = 1.0;
    double high = 2.0;
};

class Config {
public:
    static Thresholds loadThresholds(const QString &path = "/etc/dynamic_power.yaml");
};
