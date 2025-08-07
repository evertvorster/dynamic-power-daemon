#include "config.h"
#include <yaml-cpp/yaml.h>
#include <QFile>
#include <QTextStream>
#include "../daemon/log.h"

Settings Config::loadSettings(const QString& path) {
    Settings settings;

    if (!QFile::exists(path)) {
        log_error(("Config file not found: " + path).toUtf8().constData());
        return settings;
    }

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

    } catch (const std::exception &e) {
        log_error(("Failed to parse config: " + QString(e.what())).toUtf8().constData());
    }

    return settings;
}