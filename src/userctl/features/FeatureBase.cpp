// File: src/userctl/features/FeatureBase.cpp
#include "FeatureBase.h"

#include "../Config.h"

namespace dp::features {

QString FeatureBase::configPath() {
    // Delegated so there is only one definition of the user config path. Config is
    // the natural owner: it is what opens and watches that file.
    return Config::userConfigPath();
}

QString FeatureBase::normalizePolicy(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "min" || t == "max") return t;
    return "unchanged";
}

} // namespace dp::features
