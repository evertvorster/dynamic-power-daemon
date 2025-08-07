#pragma once

void log_info(const char *msg);
void log_error(const char *msg);
void enable_debug_logging();
extern bool DEBUG_MODE;