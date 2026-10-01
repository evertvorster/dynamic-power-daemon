// Split out of daemon.cpp so that it can be tested on its own. Both the governor
// and the EPP field used to carry a private copy of this logic, and the copies had
// drifted; the paths computed here decide what the daemon actually writes.

#include "cpu_targets.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace dp::daemon {

namespace fs = std::filesystem;

namespace {

// `policyN` and `cpuN` are the two shapes the kernel uses for per-CPU knobs, and
// the two walks below match one of them and nothing else.
bool isNodeNamed(const std::string& dirName, const std::string& prefix)
{
    if (dirName.rfind(prefix, 0) != 0) return false;
    const std::string digits = dirName.substr(prefix.size());
    return !digits.empty() &&
           std::all_of(digits.begin(), digits.end(),
                       [](unsigned char c) { return std::isdigit(c); });
}

} // namespace

std::string availableValuesFileName(const std::string& fieldName)
{
    if (fieldName == "energy_performance_preference") return "energy_performance_available_preferences";
    if (fieldName == "scaling_governor")              return "scaling_available_governors";
    return {};
}

std::vector<std::string> cpuTargetsFor(const std::string& configuredPath,
                                       const std::string& fieldName)
{
    const fs::path configured(configuredPath);
    if (configured.filename() != fieldName) return { configuredPath };

    std::error_code ec;
    const fs::path parent = configured.parent_path();   // .../policyN or .../cpufreq

    // Case A: /sys/.../cpufreq/policyN/<field> - the attribute sits in policyN.
    std::vector<std::string> targets;
    const fs::path policyRoot = parent.parent_path();   // .../cpufreq
    for (const auto& e : fs::directory_iterator(policyRoot, ec)) {
        if (!e.is_directory(ec)) continue;
        if (!isNodeNamed(e.path().filename().string(), "policy")) continue;
        targets.push_back((e.path() / fieldName).string());
    }
    if (!targets.empty()) return targets;

    // Case B: /sys/.../cpu/cpuN/cpufreq/<field>. cpu0 is probed first: if it is not
    // there, the cpufreq tree is not shaped this way and the configured path wins.
    const fs::path cpuRoot = parent.parent_path().parent_path();   // .../cpu
    if (!fs::exists(cpuRoot / "cpu0" / "cpufreq" / fieldName, ec)) return { configuredPath };

    for (const auto& e : fs::directory_iterator(cpuRoot, ec)) {
        if (!e.is_directory(ec)) continue;
        if (!isNodeNamed(e.path().filename().string(), "cpu")) continue;
        targets.push_back((e.path() / "cpufreq" / fieldName).string());
    }
    return targets.empty() ? std::vector<std::string>{ configuredPath } : targets;
}

} // namespace dp::daemon
