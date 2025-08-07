#include "log.h"
#include <systemd/sd-journal.h>
#include <cstdio>

bool DEBUG_MODE = false;

void enable_debug_logging() {
    DEBUG_MODE = true;
}

void log_info(const char *msg) {
    if (DEBUG_MODE) {
        printf("[DEBUG] %s\n", msg);
        fflush(stdout);  // flush to terminal
    }
    sd_journal_print(LOG_INFO, "%s", msg);
}

void log_error(const char *msg) {
    if (DEBUG_MODE) {
        printf("[ERROR] %s\n", msg);
        fflush(stdout);
    }
    sd_journal_print(LOG_ERR, "%s", msg);
}
