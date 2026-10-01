// File: src/userctl/features/FeatureBase.cpp
#include "FeatureBase.h"

#include "../UserConfig.h"

namespace dp::features {

QString FeatureBase::configPath() {
    // Delegated so there is only one definition of the user config path. UserConfig is
    // the natural owner: it is what opens and watches that file.
    return UserConfig::userConfigPath();
}

QString FeatureBase::normalizePolicy(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "min" || t == "max") return t;
    return "unchanged";
}

} // namespace dp::features
