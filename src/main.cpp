#include <QCoreApplication>
#include <QStringList>
#include "daemon/daemon.h"
#include "daemon/log.h"
#include "config/config.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();

    if (args.contains("--debug")) {
        enable_debug_logging();  // enable first
        log_info("dynamic_power starting in DEBUG mode");
    } else {
        log_info("dynamic_power starting normally");
    }

    Settings settings = Config::loadSettings(DEFAULT_CONFIG_PATH);
    Thresholds thresholds = settings.thresholds;
    int grace = settings.gracePeriodSeconds;

    // Load and optionall display thresholds
    log_info(QString("Loaded thresholds: low=%1 high=%2")
            .arg(thresholds.low)
            .arg(thresholds.high)
            .toUtf8().constData());

    if (DEBUG_MODE) {
        printf("[DEBUG] Loaded thresholds: low=%.2f high=%.2f\n", thresholds.low, thresholds.high);
        fflush(stdout);
    }
    Daemon daemon(settings.thresholds, settings.gracePeriodSeconds);
    return app.exec();
}
