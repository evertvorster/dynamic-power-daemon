#pragma once
#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>

class QVBoxLayout;
class QGridLayout;
class QToolButton;
class QLabel;
class QCloseEvent;

namespace YAML { class Node; }

struct CapabilityInfo {
    QString key;       // e.g. "cpu_governor"
    QString path;      // sysfs path
    QStringList modes; // detected/declared modes
    bool exists = false;
};

class ProfileConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProfileConfigDialog(QWidget* parent = nullptr,
                                 const QString& configPath = "/etc/dynamic_power.yaml");
    ~ProfileConfigDialog() override;   // make dtor public so stack allocation works
private slots:
    void onSave();
    void onCancel();

private:
    QString m_configPath;
    YAML::Node* m_root = nullptr;    // allocated on load, freed on dtor
    QMap<QString, CapabilityInfo> m_caps;  // key -> info
    QStringList m_profiles = {"powersave","balanced","performance"};

    // UI state
    QVBoxLayout* m_outer = nullptr;
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
