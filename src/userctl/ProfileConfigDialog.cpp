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
#include <QRadioButton>
#include <QButtonGroup>
#include <QMessageBox>
#include "DbusClient.h"
#include <QLineEdit>
#include <QRegularExpression>
#include <QFontMetrics>

#include <yaml-cpp/yaml.h>

static QStringList capKeys() {
    return {"cpu_governor","epp_profile", "acpi_platform_profile","aspm"};
}

// Human-friendly labels for capability keys
static QString capDisplayName(const QString& key) {
    if (key == "cpu_governor")         return "CPU governor";
    if (key == "epp_profile")          return "CPU EPP policy";
    if (key == "acpi_platform_profile")return "ACPI profile";
    if (key == "aspm")                 return "PCIe ASPM";
    return key;
}

ProfileConfigDialog::ProfileConfigDialog(QWidget* parent, const QString& configPath,
                                         DbusClient* dbus)
    : QDialog(parent), m_configPath(configPath), m_dbus(dbus)
{
    setWindowTitle("Profile Configuration");
    resize(720, 560);

    m_outer = new QVBoxLayout(this);

    loadYaml();            // fills m_root
    buildCapabilitiesUI(); // fills m_caps and builds m_capsContainer (but not added)
    buildProfilesUI();     // builds grid + dropdowns (goes on top)

    // Now add the capabilities block underneath
    if (m_capsContainer) {
        m_outer->addWidget(m_capsContainer);
    }

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
    // Container so we can position this block as a unit
    m_capsContainer = new QWidget(this);
    auto* v = new QVBoxLayout(m_capsContainer);

    auto* groupLabel = new QLabel(QString("Detected Capabilities — %1").arg(m_configPath), m_capsContainer);
    groupLabel->setStyleSheet("font-weight: 600; margin-top: 12px;");
    v->addWidget(groupLabel);

    QWidget* area = new QWidget(m_capsContainer);
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
        // The machine's current list is read for display only. It is deliberately not
        // copied into ci.modes: that is the declared intent, and it is what save writes
        // back. Replacing it here is what narrowed the config's EPP list down to whatever
        // governor happened to be running when the dialog was last opened.
        if (hw && hw[ks] && hw[ks]["options_path"]) {
            const QString optPath = QString::fromStdString(hw[ks]["options_path"].as<std::string>());
            if (!optPath.isEmpty() && QFileInfo::exists(optPath)) {
                ci.accepted = readModesFromFile(optPath);
            }
        }
        ci.exists = !ci.path.isEmpty() && QFileInfo::exists(ci.path);
        if (!ci.modes.contains("disabled"))     // Insert the string "disabled" to prevent a write
            ci.modes << "disabled";        
        m_caps.insert(key, ci);

        // Section header
        auto* sect = new QLabel(QString("• %1").arg(capDisplayName(key)), area);
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
        m_optEdits.insert(key, optEdit);        // so detection can be re-run on selection
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
            modesLbl->setText(modesLabelText(key));
        });
        connect(checkBtn, &QPushButton::clicked, this, [this, key, modesLbl]() {
            validateAndReload(key);
            modesLbl->setText(modesLabelText(key));
        });

        auto checkOptions = [this, key, optEdit, modesLbl]() {
            const QString op = optEdit->text().trimmed();
            if (!op.isEmpty() && QFileInfo::exists(op)) {
                const QStringList modes = readModesFromFile(op);
                if (!modes.isEmpty()) {
                    m_caps[key].accepted = modes;   // live list: display only, never persisted
                    modesLbl->setText(modesLabelText(key));
                    refreshMenusForCap(key);
                }
            }
        };
        connect(optEdit,  &QLineEdit::editingFinished, this, checkOptions);
        connect(optCheck, &QPushButton::clicked,       this, checkOptions);
    }

    // Attach the capabilities grid into the container, not directly to m_outer
    v->addWidget(area);

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
                m_caps[key].accepted = modes;   // live list: display only, never persisted
                if (modesLbl) modesLbl->setText(modesLabelText(key));
                refreshMenusForCap(key);
            }
        } else if (modesLbl) {
            modesLbl->setText(modesLabelText(key));
        }
    }
}



void ProfileConfigDialog::buildProfilesUI() {
    auto* label = new QLabel("Profiles", this);
    label->setStyleSheet("font-weight: 600; margin-top: 12px;");

    QWidget* area = new QWidget(this);
    m_profilesGrid = new QGridLayout(area);

    // Compute minimum button widths per capability column based on modes
    QFontMetrics fm(font());
    QMap<QString, int> columnMinWidth;
    for (const auto& capKey : capKeys()) {
        int w = fm.horizontalAdvance("<unset>");
        const auto& cap = m_caps.value(capKey);
        for (const auto& m : cap.modes) {
            int mw = fm.horizontalAdvance(m);
            if (mw > w) w = mw;
        }
        // some padding for the button frame and arrow
        columnMinWidth.insert(capKey, w + fm.horizontalAdvance("   "));
    }

    // Header
    m_profilesGrid->addWidget(new QLabel("Profile"), 0, 0);
    int col = 1;
    for (const auto& key : capKeys()) {
        m_profilesGrid->addWidget(new QLabel(capDisplayName(key)), 0, col++);
    }

    // Load initial selections from YAML, fallback to first mode
    YAML::Node profs = (*m_root)["profiles"];

    // One exclusive group, so selecting a profile clears the others without bookkeeping.
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    int row = 1;
    for (const auto& profile : m_profiles) {
        // The toggle decides which profile the machine is put into, and that is what makes
        // the option lists truthful: the accepted EPP values depend on the governor that is
        // actually running. Nothing starts selected, which leaves the daemon deciding on
        // its own until a profile is picked for editing.
        auto* toggle = new QRadioButton(profile, area);
        m_profilesGrid->addWidget(toggle, row, 0);
        group->addButton(toggle);
        connect(toggle, &QRadioButton::toggled, this, [this, profile](bool on) {
            if (on) onProfileSelected(profile);
        });

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
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setMinimumWidth(columnMinWidth.value(capKey, btn->sizeHint().width()));
            updateButtonMenu(profile, capKey, btn);

            m_profilesGrid->addWidget(
                btn,
                row,
                m_profilesGrid->columnCount() - (int)capKeys().size() + capKeys().indexOf(capKey)
            );
            btnMap.insert(capKey, btn);
            btn->setEnabled(false);   // until this profile is selected for editing
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

    // Said here rather than in a tooltip: the greyed rows look broken otherwise, and the
    // machine really does change mode while this window is open.
    auto* modeNote = new QLabel("Select a profile to configure it. The machine is switched into "
                               "that profile while it is selected, and your previous mode is "
                               "restored when this window closes.");
    modeNote->setWordWrap(true);
    v->addWidget(modeNote);

    // What the daemon refused on the last apply. Hidden when there is nothing to say, so
    // a clean setup shows no clutter.
    m_daemonNote = new QLabel(this);
    m_daemonNote->setWordWrap(true);
    m_daemonNote->setStyleSheet("color:#c0392b;");
    m_daemonNote->setVisible(false);
    v->addWidget(m_daemonNote);

    v->addWidget(area);
    rowHBox->addWidget(container);
    rowHBox->addStretch(1);

    m_outer->addLayout(rowHBox);
}


void ProfileConfigDialog::updateButtonMenu(const QString& profile, const QString& capKey, QToolButton* btn) {
    auto* menu = new QMenu(btn);
    const CapabilityInfo cap = m_caps.value(capKey);

    auto add = [&](const QString& m) {
        QAction* act = menu->addAction(m);
        connect(act, &QAction::triggered, this, [this, profile, capKey, m, btn]() {
            m_selection[profile][capKey] = m;
            btn->setText(m);
            // The available EPP values follow the governor, and edits are not applied until
            // Save, so the lists follow the *saved* governor. Say so once.
            if (capKey == QStringLiteral("cpu_governor"))
                maybeShowGovernorNote();
            refreshValueMarking();
        });
    };

    // What the machine accepts right now comes first. Anything the config declares but the
    // running mode refuses follows below a separator, so it can still be set for another
    // mode without being mistaken for something that works here. The union matters: a
    // config whose declared list was narrowed by an earlier governor can be missing the
    // very value a profile needs.
    for (const auto& m : cap.accepted)
        add(m);

    QStringList declaredOnly;
    for (const auto& m : cap.modes)
        if (!cap.accepted.contains(m)) declaredOnly << m;
    if (!declaredOnly.isEmpty()) {
        if (!cap.accepted.isEmpty()) menu->addSeparator();
        for (const auto& m : declaredOnly)
            add(m);
    }

    btn->setMenu(menu);
}

// Selecting a profile puts the machine into it and greys every other row. Being in the
// profile is the point: the accepted EPP values depend on the governor that is running,
// so the lists can only be truthful for the mode the machine is actually in.
void ProfileConfigDialog::onProfileSelected(const QString& profile)
{
    for (const auto& p : m_profiles) {
        const bool selected = (p == profile);
        for (QToolButton* btn : m_buttons.value(p)) {
            if (btn) btn->setEnabled(selected);
        }
    }

    m_selectedProfile = profile;

    // The machine has just been switched into this profile, so now is the moment to ask
    // what it accepts - and to re-offer those values, since the menu is built from them.
    detectAccepted();
    for (const auto& capKey : capKeys()) {
        refreshMenusForCap(capKey);
        if (auto* lbl = findChild<QLabel*>(QString("modes_%1").arg(capKey)))
            lbl->setText(modesLabelText(capKey));
    }

    refreshValueMarking();

    emit modeRequested(profile.isEmpty()
                           ? QStringLiteral("Dynamic")
                           : profile.left(1).toUpper() + profile.mid(1));

    // SetProfile applies synchronously, so by the time this returns the daemon has
    // already applied the profile and any refusal is ready to read.
    refreshDaemonWarnings();
}

// Only the profile the machine is in can be judged. The accepted set is read from the
// running mode, so it says nothing about the others - marking them would be guessing.
void ProfileConfigDialog::refreshValueMarking()
{
    for (const auto& capKey : capKeys()) {
        const CapabilityInfo cap = m_caps.value(capKey);
        for (const auto& profile : m_profiles) {
            QToolButton* btn = m_buttons.value(profile).value(capKey);
            if (!btn) continue;

            const QString value = m_selection.value(profile).value(capKey);
            const bool knowable = profile == m_selectedProfile
                                  && !m_selectedProfile.isEmpty()
                                  && !cap.accepted.isEmpty();
            const bool refused = knowable
                                 && value != QStringLiteral("disabled")
                                 && !cap.accepted.contains(value);

            btn->setStyleSheet(refused ? QStringLiteral("color:#c0392b;") : QString());
            btn->setToolTip(refused
                ? QStringLiteral("%1 is not accepted by the running mode. Accepted now: %2")
                      .arg(value, cap.accepted.join(QStringLiteral(", ")))
                : QString());
        }
    }
}

// Re-reads each capability's options file. Called when a profile is selected, because the
// accepted set follows the mode the machine is in, so it only becomes right once the
// machine has been switched into the profile being configured.
void ProfileConfigDialog::detectAccepted()
{
    for (const auto& key : capKeys()) {
        auto* optEdit = m_optEdits.value(key, nullptr);
        if (!optEdit) continue;
        const QString optPath = optEdit->text().trimmed();
        if (optPath.isEmpty() || !QFileInfo::exists(optPath)) continue;
        const QStringList modes = readModesFromFile(optPath);
        if (!modes.isEmpty()) m_caps[key].accepted = modes;
    }
}

void ProfileConfigDialog::maybeShowGovernorNote()
{
    if (m_governorNoteShown) return;
    m_governorNoteShown = true;
    QMessageBox::information(this, QStringLiteral("CPU governor changed"),
        QStringLiteral("After changing CPU governors for a profile, it is recommended to "
                       "save first before setting other options for the profile."));
}

void ProfileConfigDialog::refreshDaemonWarnings()
{
    if (!m_dbus || !m_daemonNote) return;
    const QString skipped = m_dbus->getDaemonState()
                                .value(QStringLiteral("last_skipped")).toString();
    m_daemonNote->setText(skipped.isEmpty()
        ? QString()
        : QStringLiteral("The daemon refused: %1").arg(skipped));
    m_daemonNote->setVisible(!skipped.isEmpty());
}


// The declared list, plus what the machine accepts right now when that differs. Both
// are shown because they answer different questions: the first is what this config may
// be set to, the second is what the running governor will actually take.
QString ProfileConfigDialog::modesLabelText(const QString& key) const
{
    const CapabilityInfo cap = m_caps.value(key);
    QString text = QString("[%1]").arg(cap.modes.join(", "));
    if (!cap.accepted.isEmpty() && cap.accepted != cap.modes)
        text += QString("   accepted now: [%1]").arg(cap.accepted.join(", "));
    return text;
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
    for (const auto& profile : m_profiles) {
        auto* btn = m_buttons.value(profile).value(capKey, nullptr);
        if (!btn) continue;

        updateButtonMenu(profile, capKey, btn);

        // A stored value that is not in any list is shown as it is rather than silently
        // replaced with the first entry. Rewriting a setting because a list changed is how
        // values went missing before.
        const QString sel = m_selection.value(profile).value(capKey);
        btn->setText(sel.isEmpty() ? QStringLiteral("<unset>") : sel);
    }
}
