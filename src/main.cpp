#include <QCoreApplication>
#include <QStringList>
#include "daemon/daemon.h"
#include "daemon/log.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();

    if (args.contains("--debug")) {
        enable_debug_logging();  // enable first
        log_info("dynamic_power_cpp starting in DEBUG mode");
    } else {
        log_info("dynamic_power_cpp starting normally");
    }


    Daemon daemon;
    return app.exec();
}
