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
        log_info("dynamic_power_cpp starting in DEBUG mode");
    } else {
        log_info("dynamic_power_cpp starting normally");
    }

    Thresholds thresholds = Config::loadThresholds();

    // Load and optionall display thresholds
    log_info(QString("Loaded thresholds: low=%1 high=%2")
            .arg(thresholds.low)
            .arg(thresholds.high)
            .toUtf8().constData());

    if (DEBUG_MODE) {
        printf("[DEBUG] Loaded thresholds: low=%.2f high=%.2f\n", thresholds.low, thresholds.high);
        fflush(stdout);
    }
    Daemon daemon(thresholds);
    return app.exec();
}
