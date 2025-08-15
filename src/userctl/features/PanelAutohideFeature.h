#pragma once
#include <QString>

namespace dp::features {

class PanelAutohideFeature {
public:
    struct State {
        bool enabled = false;
        QString ac = "unchanged";      // allowed: unchanged | on | off
        QString battery = "unchanged"; // allowed: unchanged | on | off
    };

    State readState() const;
    bool writeState(const State& s) const;

    QString statusText() const;                  // simple, shows configured policies
    void applyForPowerState(bool onBattery) const;
    void refreshStatus() const {}                // no-op for now

private:
    static QString normalize(const QString& s);  // -> "on"/"off"/"unchanged"
    static bool setAutohide(bool on);            // via DBus: org.kde.PlasmaShell.evaluateScript
};

} // namespace dp::features
