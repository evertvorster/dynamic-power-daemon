// File: src/userctl/UserFeatures.h
#pragma once
#include <QWidget>
#include <QString>

class QLabel;
class QCheckBox;
class QPushButton;

class UserFeaturesWidget : public QWidget {
public:
    explicit UserFeaturesWidget(QWidget* parent = nullptr);

    // Load from ~/.config/dynamic_power/config.yaml
    void load();

    // Apply user features for current power source (AC/BAT).
    // Reads ~/.config/dynamic_power/config.yaml and applies policies.
    static void applyForPowerState(bool onBattery);

    // Re-probe current state (non-UI), same detection used by the dialog.
    static void refreshStatusProbe();


    // Save to ~/.config/dynamic_power/config.yaml
    bool save();

    // Update the live status text (current screen refresh per monitor)
    void refreshLiveStatus();

private:
    // UI for the "Screen Refresh Rate" feature line
    QCheckBox*  m_screenEnabled{};
    QLabel*     m_status{};
    QPushButton* m_acBtn{};
    QPushButton* m_batBtn{};
    QCheckBox*  m_panelEnabled{};
    QPushButton* m_panelAcBtn{};
    QPushButton* m_panelBatBtn{};

    // Helpers
    static QString configPath();           // ~/.config/dynamic_power/config.yaml
    static QString cycle3(const QString&); // Unchanged -> Min -> Max -> Unchanged
    static QString normalizePolicy(const QString&); // "min"/"max"/"unchanged" (lower)
    static QString cycleOnOff(const QString&); // Unchanged -> On -> Off -> Unchanged
    static QString cyclePanelMode(const QString&); // Unchanged -> None -> Autohide -> DodgeWindows -> WindowsGoBelow -> Unchanged


    QStringList detectDisplayRates() const;
};
