#pragma once
#include <QString>

namespace dp::features {

class PanelAutohideFeature {
public:
    struct State {
        bool enabled = false;
        QString ac = "unchanged";      // allowed: unchanged | none | autohide | dodgewindows | windowsgobelow
        QString battery = "unchanged"; // allowed: unchanged | none | autohide | dodgewindows | windowsgobelow
    };

    State readState() const;
    bool writeState(const State& s) const;

    QString statusText() const;                  // simple, shows configured policies
    void applyForPowerState(bool onBattery) const;
    void refreshStatus() const {}                // no-op for now

private:
    static QString normalize(const QString& s);  // -> canonical: none/autohide/dodgewindows/windowsgobelow/unchanged
    static bool setAutohide(const QString& mode); // via DBus: org.kde.PlasmaShell.evaluateScript (panels().forEach)
};

} // namespace dp::features
