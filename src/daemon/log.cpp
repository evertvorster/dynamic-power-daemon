#include "log.h"
#include <systemd/sd-journal.h>

void log_info(const char *msg) {
    sd_journal_print(LOG_INFO, "%s", msg);
}

void log_error(const char *msg) {
    sd_journal_print(LOG_ERR, "%s", msg);
}
