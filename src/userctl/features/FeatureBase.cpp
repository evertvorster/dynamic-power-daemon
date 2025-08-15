// File: src/userctl/features/FeatureBase.cpp
#include "FeatureBase.h"

#include <QStandardPaths>

namespace dp::features {

QString FeatureBase::configPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + "/dynamic_power/config.yaml";
}

QString FeatureBase::normalizePolicy(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "min" || t == "max") return t;
    return "unchanged";
}

} // namespace dp::features
