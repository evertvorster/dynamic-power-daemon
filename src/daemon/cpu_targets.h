#pragma once

#include <string>
#include <vector>

namespace dp::daemon {

// Which sysfs nodes a single configured example path stands for.
//
// The config names one path - /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
// in the shipped template - but the kernel exposes one knob per policy or per CPU,
// and the daemon must write all of them. Two shapes exist:
//
//   /sys/.../cpufreq/policyN/<field>        the attribute sits directly in policyN
//   /sys/.../cpu/cpuN/cpufreq/<field>       the attribute sits under cpuN/cpufreq
//
// Given a path whose filename is `fieldName`, returns every node of the matching
// shape, in directory order. Given anything else - a path that does not end in
// `fieldName` at all - returns just that path, so an unusual layout still gets the
// single write the user asked for.
//
// Pure: it reads directories but writes nothing, so it can be pointed at a
// temporary tree in a test.
std::vector<std::string> cpuTargetsFor(const std::string& configuredPath,
                                       const std::string& fieldName);

// The sibling attribute in which a knob advertises the values it will accept, used
// to explain a rejected write. Empty when the field is not one of the known two.
std::string availableValuesFileName(const std::string& fieldName);

} // namespace dp::daemon
