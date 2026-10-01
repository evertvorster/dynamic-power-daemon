#pragma once

#include "common/config_paths.h"
#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>

class QVBoxLayout;
class QWidget;
class QGridLayout;
class QToolButton;
class QLabel;
class QCloseEvent;

namespace YAML { class Node; }

class DbusClient;

struct CapabilityInfo {
    QString key;       // e.g. "cpu_governor"
    QString path;      // sysfs path
    // What the config declares this knob may be set to: the intent, and what gets
    // written back on save.
    QStringList modes;
    // What the machine accepts right now, read from options_path. For display and
    // diagnostics only - never persisted, or the config ends up holding a snapshot of
    // one moment's governor, which is how the EPP list came to be narrowed to
    // "performance" alone.
    QStringList accepted;
    bool exists = false;
};

class ProfileConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProfileConfigDialog(QWidget* parent = nullptr,
                                 const QString& configPath = DEFAULT_CONFIG_PATH,
                                 DbusClient* dbus = nullptr);
    ~ProfileConfigDialog() override;   // make dtor public so stack allocation works

signals:
    // Asks to put the machine into a profile. It goes through the same override the mode
    // button in the main window uses, so there is one mechanism and one piece of state.
    // "Dynamic" means "let the daemon decide again".
    void modeRequested(const QString& mode);

private slots:
    void onSave();
    void onCancel();

private:
    QString m_configPath;
    YAML::Node* m_root = nullptr;    // allocated on load, freed on dtor
    QMap<QString, CapabilityInfo> m_caps;  // key -> info
    QStringList m_profiles = {"powersave","balanced","performance"};
    // NEW: per-capability path editor and status
    QMap<QString, class QLineEdit*> m_pathEdits;       // capKey -> QLineEdit*
    QMap<QString, class QLineEdit*> m_optEdits;        // capKey -> options path editor
    QMap<QString, class QLabel*>    m_statusLabels;    // capKey -> QLabel*
    void validateAndReload(const QString& capKey);     // check path exists, read modes, refresh menus
    QStringList readModesFromFile(const QString& path) const;
    void refreshMenusForCap(const QString& capKey);      // rebuild menus for all profiles for this cap

    // "[declared modes]", plus what the machine currently accepts when that differs.
    QString modesLabelText(const QString& key) const;

    // Selecting a profile puts the machine into it and greys the other rows, so the
    // option lists shown are read from the mode that is actually running.
    void onProfileSelected(const QString& profile);

    // Re-reads what the machine accepts, for every capability. Called when a profile is
    // selected: the accepted set follows the mode the machine is in, and it has just been
    // put into the selected profile, so that is the first moment the answer is right.
    void detectAccepted();

    // Marks a value the profile in effect will refuse. Only the selected profile can be
    // judged: the accepted set is read from the mode that is running, so it says nothing
    // about a profile the machine is not in.
    void refreshValueMarking();

    // Once per dialog, not once per change: comparing governors should not re-ask.
    void maybeShowGovernorNote();

    // What the daemon refused on the last apply, if the caller gave us a way to ask.
    void refreshDaemonWarnings();

    QString m_selectedProfile;
    bool m_governorNoteShown = false;
    DbusClient* m_dbus = nullptr;
    class QLabel* m_daemonNote = nullptr;
    
    // UI state
    QVBoxLayout* m_outer = nullptr;
    QWidget*     m_capsContainer = nullptr;
    QGridLayout* m_profilesGrid = nullptr;
    // profile -> (capKey -> selectedMode)
    QMap<QString, QMap<QString, QString>> m_selection;
    // Keep pointers to buttons to update labels on selection
    // profile::capKey -> button
    QMap<QString, QMap<QString, QToolButton*>> m_buttons;

    void loadYaml();
    void buildCapabilitiesUI();
    void buildProfilesUI();
    void updateButtonMenu(const QString& profile, const QString& capKey, QToolButton* btn);

    QByteArray emitUpdatedYaml() const;       // serialize updated YAML
    bool writeSystemFileWithPkexec(const QByteArray& data); // escalate only for write

    void showError(const QString& msg);
    void showInfo(const QString& msg);

protected:
    void closeEvent(QCloseEvent* ev) override;
};
