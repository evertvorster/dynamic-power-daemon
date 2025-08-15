// File: src/userctl/features/FeatureBase.h
#pragma once
#include <QString>

namespace dp::features {

struct Keys {
    static constexpr const char* features       = "features";
    static constexpr const char* user           = "user";
    static constexpr const char* screen_refresh = "screen_refresh";
    static constexpr const char* enabled        = "enabled";
    static constexpr const char* ac             = "ac";
    static constexpr const char* battery        = "battery";
};

// Shared helpers for feature implementations and legacy UI wrappers.
class FeatureBase {
public:
    // Returns ~/.config/dynamic_power/config.yaml
    static QString configPath();

    // Normalizes a policy label to "min", "max", or "unchanged".
    static QString normalizePolicy(const QString& s);
};

} // namespace dp::features
