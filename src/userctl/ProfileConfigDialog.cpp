#include "ProfileConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryFile>
#include <QProcess>
#include <QMessageBox>
#include <QScrollArea>

#include <yaml-cpp/yaml.h>

static QStringList capKeys() {
    return {"cpu_governor","acpi_platform_profile","aspm"};
}

ProfileConfigDialog::ProfileConfigDialog(QWidget* parent, const QString& configPath)
    : QDialog(parent), m_configPath(configPath)
{
    setWindowTitle("Profile Configuration");
    resize(720, 560);

    m_outer = new QVBoxLayout(this);

    loadYaml();            // fills m_root
    buildCapabilitiesUI(); // fills m_caps
    buildProfilesUI();     // builds grid + dropdowns

    // Buttons row
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    auto* cancel = new QPushButton("Cancel", this);
    auto* save   = new QPushButton("Save");
    connect(cancel, &QPushButton::clicked, this, &ProfileConfigDialog::onCancel);
    connect(save,   &QPushButton::clicked, this, &ProfileConfigDialog::onSave);
    btnRow->addWidget(cancel);
    btnRow->addWidget(save);
    m_outer->addLayout(btnRow);
}

ProfileConfigDialog::~ProfileConfigDialog() {
    delete m_root;
}

void ProfileConfigDialog::loadYaml() {
    try {
        auto node = YAML::LoadFile(m_configPath.toStdString());
        m_root = new YAML::Node(node);
    } catch (const std::exception& e) {
        m_root = new YAML::Node(YAML::NodeType::Map);
        showError(QString("Failed to read %1: %2").arg(m_configPath, e.what()));
    }
}

void ProfileConfigDialog::buildCapabilitiesUI() {
    auto* groupLabel = new QLabel(QString("Detected Capabilities — %1").arg(m_configPath), this);
    groupLabel->setStyleSheet("font-weight: 600; margin: 6px 0;");
    m_outer->addWidget(groupLabel);

    QWidget* area = new QWidget(this);
    auto* grid = new QGridLayout(area);
    int row = 0;

    YAML::Node hw = (*m_root)["hardware"];
    for (const auto& key : capKeys()) {
        CapabilityInfo ci; ci.key = key;
        const std::string ks = key.toStdString();
        if (hw && hw[ks]) {
            ci.path = QString::fromStdString(hw[ks]["path"].as<std::string>());
            if (hw[ks]["modes"] && hw[ks]["modes"].IsSequence()) {
                for (auto m : hw[ks]["modes"]) {
                    ci.modes << QString::fromStdString(m.as<std::string>());
                }
            }
        }
        ci.exists = !ci.path.isEmpty() && QFileInfo::exists(ci.path);

        m_caps.insert(key, ci);

        auto* name = new QLabel(QString("• %1").arg(key), area);
        auto* path = new QLabel(QString("path: %1  %2")
                                .arg(ci.path.isEmpty() ? "<missing>" : ci.path,
                                     ci.exists ? "✓" : "✗"), area);
        auto* modes = new QLabel(QString("modes: [%1]").arg(ci.modes.join(", ")), area);

        grid->addWidget(name,  row, 0);
        grid->addWidget(path,  row, 1);
        grid->addWidget(modes, row, 2);
        row++;
    }
    m_outer->addWidget(area);
}

void ProfileConfigDialog::buildProfilesUI() {
    auto* label = new QLabel("Profiles", this);
    label->setStyleSheet("font-weight: 600; margin-top: 12px;");
    m_outer->addWidget(label);

    QWidget* area = new QWidget(this);
    m_profilesGrid = new QGridLayout(area);

    // Header
    m_profilesGrid->addWidget(new QLabel("Profile"), 0, 0);
    int col = 1;
    for (const auto& key : capKeys()) {
        m_profilesGrid->addWidget(new QLabel(key), 0, col++);
    }

    // Load initial selections from YAML, fallback to first mode
    YAML::Node profs = (*m_root)["profiles"];
    int row = 1;
    for (const auto& profile : m_profiles) {
        m_profilesGrid->addWidget(new QLabel(profile), row, 0);

        QMap<QString, QToolButton*> btnMap;
        for (const auto& capKey : capKeys()) {
            QString current;
            if (profs && profs[profile.toStdString()] && profs[profile.toStdString()][capKey.toStdString()]) {
                current = QString::fromStdString(profs[profile.toStdString()][capKey.toStdString()].as<std::string>());
            } else if (m_caps.value(capKey).modes.size() > 0) {
                current = m_caps.value(capKey).modes.first();
            }

            m_selection[profile][capKey] = current;

            auto* btn = new QToolButton(area);
            btn->setText(current.isEmpty() ? "<unset>" : current);
            btn->setPopupMode(QToolButton::InstantPopup);
            updateButtonMenu(profile, capKey, btn);

            m_profilesGrid->addWidget(btn, row, m_profilesGrid->columnCount() - (int)capKeys().size() + capKeys().indexOf(capKey));
            btnMap.insert(capKey, btn);
        }
        m_buttons.insert(profile, btnMap);
        row++;
    }

    m_outer->addWidget(area);
}

void ProfileConfigDialog::updateButtonMenu(const QString& profile, const QString& capKey, QToolButton* btn) {
    auto* menu = new QMenu(btn);
    const auto& cap = m_caps.value(capKey);
    const auto modes = cap.modes;

    for (const auto& m : modes) {
        QAction* act = menu->addAction(m);
        connect(act, &QAction::triggered, this, [this, profile, capKey, m, btn]() {
            m_selection[profile][capKey] = m;
            btn->setText(m);
        });
    }
    btn->setMenu(menu);
}

void ProfileConfigDialog::onSave() {
    QByteArray data = emitUpdatedYaml();
    if (data.isEmpty()) {
        showError("Failed to serialize configuration.");
        return;
    }
    if (!writeSystemFileWithPkexec(data)) return;
    showInfo("Saved. The daemon will auto-reload.");
    accept();
}

void ProfileConfigDialog::onCancel() { reject(); }

QByteArray ProfileConfigDialog::emitUpdatedYaml() const {
    try {
        YAML::Node root = *m_root;

        // Update profiles section with current selections
        YAML::Node profs = root["profiles"];
        if (!profs || !profs.IsMap()) profs = YAML::Node(YAML::NodeType::Map);

        for (const auto& profile : m_profiles) {
            YAML::Node p = profs[profile.toStdString()];
            for (const auto& capKey : capKeys()) {
                const QString sel = m_selection.value(profile).value(capKey);
                if (!sel.isEmpty()) {
                    p[capKey.toStdString()] = sel.toStdString();
                }
            }
            profs[profile.toStdString()] = p;
        }
        root["profiles"] = profs;

        YAML::Emitter out;
        out << root;
        if (!out.good()) return {};
        return QByteArray(out.c_str());
    } catch (...) {
        return {};
    }
}

bool ProfileConfigDialog::writeSystemFileWithPkexec(const QByteArray& data) {
    // Escalate only for the write using: pkexec tee /etc/dynamic_power.yaml
    QProcess proc;
    QString program = "pkexec";
    QStringList args; args << "tee" << m_configPath;
    proc.start(program, args);
    if (!proc.waitForStarted(10000)) {
        showError("Failed to start pkexec. Is polkit installed?");
        return false;
    }
    proc.write(data);
    proc.closeWriteChannel();
    proc.waitForFinished(-1);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        showError(QString("Write failed (exit %1).").arg(proc.exitCode()));
        return false;
    }
    return true;
}

void ProfileConfigDialog::showError(const QString& msg) {
    QMessageBox::critical(this, "Profile Configuration", msg);
}

void ProfileConfigDialog::showInfo(const QString& msg) {
    QMessageBox::information(this, "Profile Configuration", msg);
}

void ProfileConfigDialog::closeEvent(QCloseEvent* ev) {
    QDialog::closeEvent(ev);
}
