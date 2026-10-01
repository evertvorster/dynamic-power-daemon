#pragma once

#include <QString>

// The root-owned daemon config file. Both binaries need this path: the daemon reads
// and watches it, and the user application writes it through pkexec.
//
// It lives in its own header rather than in config/config.h, which already declares
// it, because config/config.h also declares a class called Config - and userctl has
// one of those too. Including the daemon's header from the user application is
// therefore a name collision, which is why this path used to be written out as a
// literal in five places over there.
static const inline QString DEFAULT_CONFIG_PATH = "/etc/dynamic_power.yaml";
