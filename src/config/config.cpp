#include "config.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QTextStream>
#include "../daemon/log.h"

Thresholds Config::loadThresholds(const QString &path) {
    Thresholds result;

    if (!QFile::exists(path)) {
        log_error(("Config file not found: " + path).toUtf8().constData());
        return result;
    }

    try {
        YAML::Node root = YAML::LoadFile(path.toStdString());
        auto node = root["thresholds"];
        if (node) {
            result.low = node["low"] ? node["low"].as<double>() : result.low;
            result.high = node["high"] ? node["high"].as<double>() : result.high;
        } else {
            log_info("Config found but no thresholds section.");
        }
    } catch (const std::exception &e) {
        log_error(("Failed to parse config: " + QString(e.what())).toUtf8().constData());
    }

    return result;
}
