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
#include <QLineEdit>
#include <QRegularExpression>

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
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);
    grid->setColumnStretch(1, 1); // make path edits expand

    int row = 0;
    YAML::Node hw = (*m_root)["hardware"];

    for (const auto& key : capKeys()) {
        CapabilityInfo ci; ci.key = key;
        const std::string ks = key.toStdString();
        if (hw && hw[ks]) {
            if (hw[ks]["path"]) {
                ci.path = QString::fromStdString(hw[ks]["path"].as<std::string>());
            }
            if (hw[ks]["modes"] && hw[ks]["modes"].IsSequence()) {
                for (auto m : hw[ks]["modes"]) {
                    ci.modes << QString::fromStdString(m.as<std::string>());
                }
            }
        }
        // If options_path present and readable, prefer modes from there
        if (hw && hw[ks] && hw[ks]["options_path"]) {
            const QString optPath = QString::fromStdString(hw[ks]["options_path"].as<std::string>());
            if (!optPath.isEmpty() && QFileInfo::exists(optPath)) {
                ci.modes = readModesFromFile(optPath);
            }
        }
        ci.exists = !ci.path.isEmpty() && QFileInfo::exists(ci.path);
        if (!ci.modes.contains("disabled"))     // Insert the string "disabled" to prevent a write
            ci.modes << "disabled";        
        m_caps.insert(key, ci);

        // Section header
        auto* sect = new QLabel(QString("• %1").arg(key), area);
        sect->setStyleSheet("font-weight: 600;");
        grid->addWidget(sect, row++, 0, 1, 4);

        // Set Path row
        grid->addWidget(new QLabel("Set Path:", area), row, 0);
        auto* edit = new QLineEdit(ci.path, area);
        edit->setPlaceholderText("/sys/.../set_file");
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        grid->addWidget(edit, row, 1);
        m_pathEdits.insert(key, edit);

        auto* checkBtn = new QPushButton("Check", area);
        grid->addWidget(checkBtn, row, 2);

        auto* status = new QLabel(ci.exists ? "✓ path ok" : "✗ not found", area);
        grid->addWidget(status, row, 3);
        m_statusLabels.insert(key, status);
        row++;

        // Options Path row
        grid->addWidget(new QLabel("Options Path:", area), row, 0);
        auto* optEdit = new QLineEdit(area);
        if (hw && hw[ks] && hw[ks]["options_path"]) {
            optEdit->setText(QString::fromStdString(hw[ks]["options_path"].as<std::string>()));
        }
        optEdit->setPlaceholderText("/sys/.../options_file");
        optEdit->setObjectName(QString("opt_%1").arg(key));
        optEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        grid->addWidget(optEdit, row, 1);

        auto* optCheck = new QPushButton("Check", area); // checks file and loads options
        grid->addWidget(optCheck, row, 2);
        row++;

        // Options (discovered) row
        grid->addWidget(new QLabel("Options:", area), row, 0);
        auto* modesLbl = new QLabel(QString("[%1]").arg(ci.modes.join(", ")), area);
        modesLbl->setObjectName(QString("modes_%1").arg(key));
        modesLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(modesLbl, row, 1, 1, 3);
        row++;

        // Wiring
        connect(edit, &QLineEdit::editingFinished, this, [this, key, modesLbl]() {
            validateAndReload(key); // only validates Set Path
            modesLbl->setText(QString("[%1]").arg(m_caps.value(key).modes.join(", ")));
        });
        connect(checkBtn, &QPushButton::clicked, this, [this, key, modesLbl]() {
            validateAndReload(key);
            modesLbl->setText(QString("[%1]").arg(m_caps.value(key).modes.join(", ")));
        });

        auto checkOptions = [this, key, optEdit, modesLbl]() {
            const QString op = optEdit->text().trimmed();
            if (!op.isEmpty() && QFileInfo::exists(op)) {
                const QStringList modes = readModesFromFile(op);
                if (!modes.isEmpty()) {
                    m_caps[key].modes = modes;
                    modesLbl->setText(QString("[%1]").arg(m_caps.value(key).modes.join(", ")));
                    refreshMenusForCap(key);
                }
            }
        };
        connect(optEdit,  &QLineEdit::editingFinished, this, checkOptions);
        connect(optCheck, &QPushButton::clicked,       this, checkOptions);
    }

    m_outer->addWidget(area);

    // Initial validation + options load
    for (const auto& key : capKeys()) {
        if (m_pathEdits.contains(key)) {
            validateAndReload(key); // Set Path exists?
        }
        auto modesLbl = area->findChild<QLabel*>(QString("modes_%1").arg(key));
        auto optEdit  = area->findChild<QLineEdit*>(QString("opt_%1").arg(key));
        if (optEdit && QFileInfo::exists(optEdit->text().trimmed())) {
            const QStringList modes = readModesFromFile(optEdit->text().trimmed());
            if (!modes.isEmpty()) {
                m_caps[key].modes = modes;
                if (modesLbl) modesLbl->setText(QString("[%1]").arg(m_caps.value(key).modes.join(", ")));
                refreshMenusForCap(key);
            }
        } else if (modesLbl) {
            modesLbl->setText(QString("[%1]").arg(m_caps.value(key).modes.join(", ")));
        }
    }
}



void ProfileConfigDialog::buildProfilesUI() {
    auto* label = new QLabel("Profiles", this);
    label->setStyleSheet("font-weight: 600; margin-top: 12px;");

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

    // Center the profiles block horizontally
    auto* rowHBox = new QHBoxLayout();
    rowHBox->addStretch(1);
    auto* container = new QWidget(this);
    auto* v = new QVBoxLayout(container);
    v->addWidget(label);
    v->addWidget(area);
    rowHBox->addWidget(container);
    rowHBox->addStretch(1);

    m_outer->addLayout(rowHBox);
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

        // Persist hardware paths and current modes (preserve options_path if present)
        YAML::Node hw = root["hardware"];
        if (!hw || !hw.IsMap()) hw = YAML::Node(YAML::NodeType::Map);

        for (const auto& key : capKeys()) {
            const auto& cap = m_caps.value(key);
            const std::string ks = key.toStdString();

            YAML::Node h = hw[ks];
            h["path"] = cap.path.toStdString();

            YAML::Node mm(YAML::NodeType::Sequence);
            for (const auto& m : cap.modes) mm.push_back(m.toStdString());
            h["modes"] = mm;

            // write options_path from UI if present; else preserve existing
            if (auto optEditWidget = this->findChild<QLineEdit*>(QString("opt_%1").arg(key))) {
                const QString op = optEditWidget->text().trimmed();
                if (!op.isEmpty()) {
                    h["options_path"] = op.toStdString();
                }
            } else if ((*m_root)["hardware"] &&
                       (*m_root)["hardware"][ks] &&
                       (*m_root)["hardware"][ks]["options_path"]) {
                h["options_path"] = (*m_root)["hardware"][ks]["options_path"];
            }

            hw[ks] = h;
        }
        root["hardware"] = hw;

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

void ProfileConfigDialog::validateAndReload(const QString& capKey) {
    auto* edit = m_pathEdits.value(capKey, nullptr);
    auto* status = m_statusLabels.value(capKey, nullptr);
    if (!edit || !status) return;

    const QString path = edit->text().trimmed();
    m_caps[capKey].path = path;
    const bool ok = !path.isEmpty() && QFileInfo::exists(path);
    m_caps[capKey].exists = ok;

    status->setText(ok ? "✓ path ok" : "✗ not found");

    // Do NOT read options here; Options Path has its own Check and loader
    refreshMenusForCap(capKey);
}

QStringList ProfileConfigDialog::readModesFromFile(const QString& path) const {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    const QString text = QString::fromUtf8(f.readAll());
    f.close();

    QStringList tokens = text.split(QRegularExpression("[\\s,]+"),
                                    Qt::SkipEmptyParts);

    // Remove square brackets from active ASPM value like "[powersave]"
    QStringList cleaned;
    cleaned.reserve(tokens.size());
    for (QString s : tokens) {
        s.remove('[').remove(']');
        s = s.trimmed();
        if (!s.isEmpty()) cleaned << s;
    }
    cleaned.removeDuplicates();
    if (!cleaned.contains("disabled")) // Add in disabled variable that prevents a write
        cleaned << "disabled";
    return cleaned;
}

void ProfileConfigDialog::refreshMenusForCap(const QString& capKey) {
    const auto modes = m_caps.value(capKey).modes;

    for (const auto& profile : m_profiles) {
        auto* btn = m_buttons.value(profile).value(capKey, nullptr);
        if (!btn) continue;

        // Rebuild menu
        auto* menu = new QMenu(btn);
        for (const auto& m : modes) {
            QAction* act = menu->addAction(m);
            connect(act, &QAction::triggered, this, [this, profile, capKey, m, btn]() {
                m_selection[profile][capKey] = m;
                btn->setText(m);
            });
        }
        btn->setMenu(menu);

        // Ensure current selection is valid
        QString sel = m_selection.value(profile).value(capKey);
        if (!modes.contains(sel)) {
            sel = modes.isEmpty() ? QString() : modes.first();
            m_selection[profile][capKey] = sel;
            btn->setText(sel.isEmpty() ? "<unset>" : sel);
        }
    }
}
