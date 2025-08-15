// File: src/userctl/features/ScreenRefreshFeature.h
#pragma once
#include <QString>
#include <QStringList>

namespace dp::features {

class ScreenRefreshFeature {
public:
    struct State {
        bool enabled = false;
        QString ac = "unchanged";
        QString battery = "unchanged";
    };

    // Read/write state from ~/.config/dynamic_power/config.yaml
    State readState() const;
    bool writeState(const State& s) const;

    // Human-readable status text for the UI.
    QString statusText() const;

    // Apply according to current config for the given power state
    void applyForPowerState(bool onBattery) const;

    // Passive probe used for verification; no UI side-effects.
    void refreshStatus() const;

private:
    // Helpers (moved from UserFeatures.cpp anon namespace)
    struct Mode { QString id; int w=0; int h=0; double hz=0.0; };
    struct Output { QString id, name, currentModeId; int w=0, h=0; QList<Mode> modes; };

    static QList<Output> readOutputs();
    static QString selectSameResModeId(const Output& o, const QString& policyLower);
    static bool applyMode(const QString& outId, const QString& modeId);
    static QStringList probeCurrentRefreshStrings();

    static QString normalizePolicy(const QString& s);
};

} // namespace dp::features
