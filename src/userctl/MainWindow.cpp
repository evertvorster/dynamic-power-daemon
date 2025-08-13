
#include "MainWindow.h"
#include "DbusClient.h"
#include "Config.h"
#include "LoadGraphWidget.h"
#include "ProcessRuleEditor.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QCursor>
#include <QLabel>
#include <QScrollArea>
#include <QSpacerItem>
#include <QLabel>
#include <QSet>
#include "ProfileConfigDialog.h"

// ================= RootFeaturesDialog (inline, no Q_OBJECT) ==================
#include <QDialog>
#include <QGroupBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QDateTime>
#include <QMessageBox>
#include <QProcess>
#include <QScrollArea>
#include <QMap>
#include <QVector>
#include <QFrame>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QFile>
#include <yaml-cpp/yaml.h>

namespace {
struct RootRow {
    QWidget*    rowWidget{};
    QCheckBox*  enabled{};
    QPushButton* rootBtn{};   // "/sys" or "/proc"
    QLineEdit*  pathRel{};    // relative to rootBtn
    QLineEdit*  ac{};
    QLineEdit*  bat{};
    QPushButton* delBtn{};
};

class RootFeaturesDialog : public QDialog {
public:
    explicit RootFeaturesDialog(QWidget* parent, const QString& etcPath = "/etc/dynamic_power.yaml")
        : QDialog(parent), m_etcPath(etcPath) {
        setWindowTitle("Power-saving Features (Root)");
        resize(760, 560);

        auto* outer = new QVBoxLayout(this);

        // Disclaimer strip
        auto* disclaimBox = new QHBoxLayout();
        m_confirmBtn = new QPushButton(this);
        m_confirmBtn->setText("Confirm ❌");
        m_confirmBtn->setToolTip("Read and accept the disclaimer to enable saving.");
        disclaimBox->addWidget(m_confirmBtn, 0);
        m_confirmNote = new QLabel("Saving is blocked until you accept the disclaimer.", this);
        disclaimBox->addWidget(m_confirmNote, 1);
        outer->addLayout(disclaimBox);

        connect(m_confirmBtn, &QPushButton::clicked, this, [this]{ showDisclaimer(); });

        // Rows area inside a group + scroll
        auto* group = new QGroupBox("Root-required features (writes to /etc/dynamic_power.yaml)", this);
        auto* groupLay = new QVBoxLayout(group);

        m_rowsContainer = new QWidget(group);
        m_rowsLayout = new QVBoxLayout(m_rowsContainer);
        m_rowsLayout->setSpacing(2);
        m_rowsLayout->setContentsMargins(0,0,0,0);
        m_rowsLayout->setAlignment(Qt::AlignTop);

        // Header row        
        auto* header = new QWidget(m_rowsContainer);
        header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* hgrid = new QGridLayout(header);
        hgrid->setContentsMargins(0,0,0,0);
        hgrid->setHorizontalSpacing(8);
        hgrid->setVerticalSpacing(0);
        // Make header columns match data-row columns
        hgrid->setColumnStretch(0, 0); // [✓] checkbox
        hgrid->setColumnStretch(1, 0); // [✓] root button
        hgrid->setColumnStretch(2, 3); // [→] Path (wide)
        hgrid->setColumnStretch(3, 2); // [→] AC value
        hgrid->setColumnStretch(4, 2); // [→] Battery value
        hgrid->setColumnStretch(5, 0); // [✓] delete button
        // Ensure header's first two columns match the row widgets
        QCheckBox chkProbe;                           // checkbox width probe
        QPushButton rootProbe("/proc");               // button width probe ("/proc" is widest)
        hgrid->setColumnMinimumWidth(0, chkProbe.sizeHint().width());
        hgrid->setColumnMinimumWidth(1, rootProbe.sizeHint().width());
        QPushButton delProbe("Delete");
        hgrid->setColumnMinimumWidth(5, delProbe.sizeHint().width());
        int c = 0;
        hgrid->addWidget(new QLabel("", header), 0, c++); // (checkbox column)
        hgrid->addWidget(new QLabel("Root", header), 0, c++); // (Root button column; leave header blank)
        auto* lblPath = new QLabel("Path", header);
        lblPath->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lblPath->setIndent(0); lblPath->setMargin(0); lblPath->setContentsMargins(0,0,0,0);
        lblPath->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        hgrid->addWidget(lblPath, 0, c++);
        auto* lblAc = new QLabel("AC value", header);
        lblAc->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lblAc->setIndent(0); lblAc->setMargin(0); lblAc->setContentsMargins(0,0,0,0);
        lblAc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        hgrid->addWidget(lblAc, 0, c++);
        auto* lblBat = new QLabel("Battery value", header);
        lblBat->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lblBat->setIndent(0); lblBat->setMargin(0); lblBat->setContentsMargins(0,0,0,0);
        lblBat->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        hgrid->addWidget(lblBat, 0, c++);
        hgrid->addWidget(new QLabel("", header), 0, c++); // (delete button column)
        m_rowsLayout->addWidget(header);

        // Scroll area
        auto* scroll = new QScrollArea(group);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::StyledPanel);
        scroll->setWidget(m_rowsContainer);
        groupLay->addWidget(scroll, 1);

        // Add row + Save buttons
        auto* btnRow = new QHBoxLayout();
        m_addBtn = new QPushButton("Add Feature", group);
        m_saveBtn = new QPushButton("Save (Root)", group);
        btnRow->addWidget(m_addBtn);
        btnRow->addStretch(1);
        btnRow->addWidget(m_saveBtn);
        groupLay->addLayout(btnRow);

        outer->addWidget(group);

        connect(m_addBtn, &QPushButton::clicked, this, [this]{ addRow(); });
        connect(m_saveBtn, &QPushButton::clicked, this, [this]{ onSave(); });

        // Load from YAML
        loadYaml();
        updateConfirmUI();
    }

private:
    QString m_etcPath;
    bool m_disclaimerAccepted = false;
    QString m_disclaimerAcceptedAt;

    QWidget* m_rowsContainer{};
    QVBoxLayout* m_rowsLayout{};
    QPushButton* m_addBtn{};
    QPushButton* m_saveBtn{};
    QPushButton* m_confirmBtn{};
    QLabel* m_confirmNote{};

    QVector<RootRow> m_rows;

    void updateConfirmUI() {
        m_confirmBtn->setText(m_disclaimerAccepted ? "Confirm ✅" : "Confirm ❌");
        m_confirmNote->setVisible(!m_disclaimerAccepted);
    }

    void showDisclaimer() {
        QMessageBox box(this);
        box.setWindowTitle("Disclaimer");
        box.setText("The settings here write variables YOU specify into files YOU specify as ROOT, automatically.\n\n"
                    "With this great power comes great responsibilty. YOUR responsiblity.\n\n"
                    "The authors accept NO responsiblity for anything bad happening to your system when you set these."
                );
        box.setInformativeText("Do you agree?");
        auto* agree = box.addButton("Agree", QMessageBox::AcceptRole);
        auto* disagree = box.addButton("Disagree", QMessageBox::RejectRole);
        box.setIcon(QMessageBox::Warning);
        box.exec();
        if (box.clickedButton() == agree) {
            m_disclaimerAccepted = true;
            m_disclaimerAcceptedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            updateConfirmUI();
        } else if (box.clickedButton() == disagree) {
            m_disclaimerAccepted = false;
            updateConfirmUI();
        }
    }

    void clearRows() {
        for (auto& r : m_rows) if (r.rowWidget) r.rowWidget->deleteLater();
        m_rows.clear();
    }

    void addRow(bool enabled=false, const QString& absPath="/sys/", const QString& ac="", const QString& bat="") {
        auto* row = new QWidget(m_rowsContainer);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* grid = new QGridLayout(row);
        grid->setContentsMargins(0,0,0,0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(0);
        // Match header column model exactly
        grid->setColumnStretch(0, 0); // checkbox
        grid->setColumnStretch(1, 0); // root button
        grid->setColumnStretch(2, 3); // Path
        grid->setColumnStretch(3, 2); // AC value
        grid->setColumnStretch(4, 2); // Battery value
        grid->setColumnStretch(5, 0); // delete button
        int col = 0;

        auto* chk = new QCheckBox(row);
        chk->setChecked(enabled);
        grid->addWidget(chk, 0, col++);

        // Root selector button with tiny popup menu
        auto* rootBtn = new QPushButton(row);
        rootBtn->setText(absPath.startsWith("/proc") ? "/proc" : "/sys");
        auto* rootMenu = new QMenu(rootBtn);
        rootMenu->addAction("/sys",  [rootBtn]{ rootBtn->setText("/sys");  });
        rootMenu->addAction("/proc", [rootBtn]{ rootBtn->setText("/proc"); });
        connect(rootBtn, &QPushButton::clicked, this, [rootBtn, rootMenu]{
            rootMenu->exec(QCursor::pos());
        });
        grid->addWidget(rootBtn, 0, col++);

        auto* pathEdit = new QLineEdit(row);
        QString rel = absPath;
        if (absPath.startsWith("/sys"))  rel = absPath.mid(4);
        else if (absPath.startsWith("/proc")) rel = absPath.mid(5);
        pathEdit->setPlaceholderText("/class/.../file");
        pathEdit->setText(rel);
        pathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid->addWidget(pathEdit, 0, col++);

        auto* acEdit = new QLineEdit(ac, row);
        acEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid->addWidget(acEdit, 0, col++);

        auto* batEdit = new QLineEdit(bat, row);
        batEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        grid->addWidget(batEdit, 0, col++);

        auto* del = new QPushButton("Delete", row);
        del->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        grid->addWidget(del, 0, col++);

        RootRow rr{row, chk, rootBtn, pathEdit, acEdit, batEdit, del};
        m_rows.push_back(rr);

        connect(del, &QPushButton::clicked, this, [this, row]() {
            int idx = -1;
            for (int i=0;i<m_rows.size();++i) if (m_rows[i].rowWidget == row) { idx = i; break; }
            if (idx >= 0) {
                auto r = m_rows.takeAt(idx);
                r.rowWidget->deleteLater();
            }
        });

        m_rowsLayout->addWidget(row);
    }

    void loadYaml() {
        YAML::Node root;
        try { root = YAML::LoadFile(m_etcPath.toStdString()); } catch (...) {
            root = YAML::Node(YAML::NodeType::Map);
        }
        YAML::Node fr = root["features"]["root"];
        if (fr && fr["disclaimer"]) {
            try { m_disclaimerAccepted = fr["disclaimer"]["accepted"].as<bool>(); } catch (...) {}
            try { m_disclaimerAcceptedAt = QString::fromStdString(fr["disclaimer"]["accepted_at"].as<std::string>()); } catch (...) {}
        }
        clearRows();
        if (fr && fr["features"] && fr["features"].IsSequence()) {
            for (const auto& n : fr["features"]) {
                const QString path = n["path"] ? QString::fromStdString(n["path"].as<std::string>()) : "/sys/";
                const bool en = n["enabled"] ? n["enabled"].as<bool>() : false;
                const QString ac = n["ac_value"] ? QString::fromStdString(n["ac_value"].as<std::string>()) : "";
                const QString bat = n["battery_value"] ? QString::fromStdString(n["battery_value"].as<std::string>()) : "";
                addRow(en, path, ac, bat);
            }
        } else if (fr && fr.IsMap()) {
            for (auto it = fr.begin(); it != fr.end(); ++it) {
                const std::string key = it->first.as<std::string>();
                if (key == "disclaimer" || key == "features") continue;
                YAML::Node n = it->second;
                const QString path = n["path"] ? QString::fromStdString(n["path"].as<std::string>()) : "/sys/";
                const bool en = n["enabled"] ? n["enabled"].as<bool>() : false;
                const QString ac = n["ac"] ? QString::fromStdString(n["ac"].as<std::string>()) : "";
                const QString bat = n["battery"] ? QString::fromStdString(n["battery"].as<std::string>()) : "";
                addRow(en, path, ac, bat);
            }
        }
        if (m_rows.isEmpty()) addRow(); // seed
    }

    bool pkexecWrite(const QByteArray& data, const QString& path) {
        QProcess proc;
        proc.start("pkexec", {"tee", path});
        if (!proc.waitForStarted(10000)) {
            QMessageBox::critical(this, "Error", "Failed to start pkexec. Is polkit installed?");
            return false;
        }
        proc.write(data);
        proc.closeWriteChannel();
        proc.waitForFinished(-1);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            QMessageBox::critical(this, "Error", QString("Write failed (exit %1).").arg(proc.exitCode()));
            return false;
        }
        return true;
    }

    void onSave() {
        if (!m_disclaimerAccepted) {
            QMessageBox::warning(this, "Confirmation required",
                                 "Saving is disabled until you click Confirm and agree to the disclaimer.");
            return;
        }

        YAML::Node root;
        try { root = YAML::LoadFile(m_etcPath.toStdString()); } catch (...) { root = YAML::Node(YAML::NodeType::Map); }
        YAML::Node features = root["features"];
        if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);
        YAML::Node fr = features["root"];
        if (!fr || !fr.IsMap()) fr = YAML::Node(YAML::NodeType::Map);

        YAML::Node disc = fr["disclaimer"];
        if (!disc || !disc.IsMap()) disc = YAML::Node(YAML::NodeType::Map);
        disc["accepted"] = m_disclaimerAccepted;
        if (!m_disclaimerAcceptedAt.isEmpty())
            disc["accepted_at"] = m_disclaimerAcceptedAt.toStdString();
        fr["disclaimer"] = disc;

        YAML::Node arr(YAML::NodeType::Sequence);
        for (const auto& r : m_rows) {
            QString rel = r.pathRel->text().trimmed();
            if (rel.startsWith("/")) rel = rel.mid(1);
            const QString prefix = r.rootBtn->text().trimmed();   // "/sys" or "/proc"
            const QString abs = QString("%1/%2").arg(prefix, rel);

            YAML::Node n;                           // ← add this line
            n["enabled"]       = r.enabled->isChecked();
            n["path"]          = abs.toStdString();
            n["ac_value"]      = r.ac->text().trimmed().toStdString();
            n["battery_value"] = r.bat->text().trimmed().toStdString();
            arr.push_back(n);
        }
        fr["features"] = arr;
        features["root"] = fr;
        root["features"] = features;

        YAML::Emitter out;
        out << root;
        QByteArray data(out.c_str());
        if (pkexecWrite(data, m_etcPath)) {
            QMessageBox::information(this, "Saved", "Root features saved. The daemon will auto-reload.");
            accept();
        }
    }
};
} // namespace

MainWindow::MainWindow(DbusClient* dbus, Config* config, QWidget* parent)
    : QMainWindow(parent), m_dbus(dbus), m_config(config) {

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_graph = new LoadGraphWidget(this);
    layout->addWidget(m_graph);
    connect(m_graph, &LoadGraphWidget::thresholdsPreview, this, [this](double low, double high){
        // UI already reflects it; no I/O here.
        Q_UNUSED(low); Q_UNUSED(high);
    });
    connect(m_graph, &LoadGraphWidget::thresholdsCommitted, this, &MainWindow::onGraphThresholdChanged);

    m_overrideBtn = new QPushButton(this);
    layout->addWidget(m_overrideBtn);
    connect(m_overrideBtn, &QPushButton::clicked, this, &MainWindow::onOverrideButtonClicked);

    // Profile Configuration button
    auto* profileBtn = new QPushButton("Profile Configuration", this);
    layout->addWidget(profileBtn);
    connect(profileBtn, &QPushButton::clicked, this, [this]{
        ProfileConfigDialog dlg(this, "/etc/dynamic_power.yaml");
        dlg.exec();
    });

    // Power-saving Features (Root) button
    auto* rootFeatBtn = new QPushButton("Power-saving Features…", this);
    layout->addWidget(rootFeatBtn);
    connect(rootFeatBtn, &QPushButton::clicked, this, [this]{
        RootFeaturesDialog dlg(this, "/etc/dynamic_power.yaml");
        dlg.exec();
    });

    // Power info label
    m_powerLabel = new QLabel(this);
    m_powerLabel->setText("Power: …");
    // Use theme default color; just make it bold
    QFont f = m_powerLabel->font();
    f.setBold(true);
    m_powerLabel->setFont(f);
    layout->addWidget(m_powerLabel);

    // --- Process matches section ---
    auto* sectionLabel = new QLabel("Process Matches", this);
    layout->addWidget(sectionLabel);

    // Scrollable panel for buttons
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_rulesPanel = new QWidget(scroll);
    m_rulesLayout = new QVBoxLayout(m_rulesPanel);
    m_rulesLayout->setContentsMargins(6,6,6,6);
    scroll->setWidget(m_rulesPanel);
    layout->addWidget(scroll);

    // layout order: [0]=graph, [1]=overrideBtn, [2]=scroll(process list)
    layout->setStretch(0, 3);   // graph gets the stretch
    layout->setStretch(2, 1);   // list can grow, but less    

    // Initial population
    refreshProcessButtons();

    setCentralWidget(central);
    setWindowTitle("Dynamic Power Control");
    resize(800, 480);
    refreshOverrideButton();
}

void MainWindow::setThresholds(double low, double high) {
    m_graph->setThresholds(low, high);
    refreshOverrideButton();
}

void MainWindow::setActiveProfile(const QString& profile) {
    m_activeProfile = profile;
    refreshOverrideButton();
}

void MainWindow::setProcessMatchState(const QSet<QString>& matches, const QString& winnerLower) {
    m_matchedProcs = matches;
    m_winnerProc = winnerLower;
    refreshProcessButtons();
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    emit visibilityChanged(true);
}

void MainWindow::hideEvent(QHideEvent* e) {
    QMainWindow::hideEvent(e);
    emit visibilityChanged(false);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    e->ignore();
    this->hide();
    emit visibilityChanged(false);
}

void MainWindow::onOverrideButtonClicked() {
    QMenu menu(this);
    QStringList modes = {"Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"};
    for (const auto& m : modes) {
        QAction* act = menu.addAction(m);
        connect(act, &QAction::triggered, this, [this, m]() {
            m_userMode = m;
            bool boss = (m != QStringLiteral("Dynamic"));
            emit userOverrideSelected(m, boss);
            refreshOverrideButton();
        });
    }
    menu.exec(QCursor::pos());
}

void MainWindow::onGraphThresholdChanged(double low, double high) {
    emit thresholdsAdjusted(low, high);
}

void MainWindow::refreshOverrideButton() {
    m_overrideBtn->setText(QString("%1 – %2").arg(m_userMode, m_activeProfile));
    m_overrideBtn->setStyleSheet(m_userMode != QStringLiteral("Dynamic")
        ? "background: palette(highlight); color: palette(highlighted-text);"
        : "");
}

void MainWindow::refreshProcessButtons() {
    // Clear old
    if (!m_rulesLayout) return;
    QLayoutItem* child;
    while ((child = m_rulesLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }

    // One button per config rule (display the match string = process_name)
    const auto& rules = m_config->processRules();
    for (int i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];

        auto* btn = new QPushButton(r.process_name, m_rulesPanel);
        btn->setToolTip(QString("Priority %1 · Mode %2").arg(r.priority).arg(r.active_profile));

        // Open editor for this rule
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            auto current = m_config->processRules();
            if (i < 0 || i >= current.size()) return;

            ProcessRuleEditor dlg(this);
            dlg.setRule(current[i]);

            bool didDelete = false;
            connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&]{ didDelete = true; });

            if (dlg.exec() == QDialog::Accepted) {
                if (didDelete) {
                    current.removeAt(i);
                } else {
                    current[i] = dlg.rule();
                }
                m_config->setProcessRules(current);
                m_config->save();
                refreshProcessButtons();
            }
        });

        // existing highlighting
        const QString lname = r.process_name.toLower();
        if (m_userMode == QStringLiteral("Dynamic") && !m_winnerProc.isEmpty() && lname == m_winnerProc) {
            btn->setStyleSheet("background: palette(highlight); color: palette(highlighted-text);");
        } else if (m_matchedProcs.contains(lname)) {
            const QColor c = qApp->palette().color(QPalette::Active, QPalette::Highlight);
            btn->setStyleSheet(QString("border-left: 5px solid rgb(%1,%2,%3); background: rgba(%1,%2,%3,0.16);")
                            .arg(c.red()).arg(c.green()).arg(c.blue()));
        }

        m_rulesLayout->addWidget(btn);
    }


    // "Add process to match" button at bottom
    auto* addBtn = new QPushButton("Add process to match", m_rulesPanel);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        ProcessRuleEditor dlg(this);
        ProcessRule empty;                    // blank fields by default
        dlg.setRule(empty);

        bool didDelete = false;
        connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&]{ didDelete = true; });

        if (dlg.exec() == QDialog::Accepted && !didDelete) {
            auto rules = m_config->processRules();
            auto r = dlg.rule();
            if (!r.process_name.trimmed().isEmpty()) {
                rules.push_back(r);
                m_config->setProcessRules(rules);
                m_config->save();
                refreshProcessButtons();
            }
        }
    });

    m_rulesLayout->addWidget(addBtn);

    // Spacer to push buttons to top
    m_rulesLayout->addItem(new QSpacerItem(0,0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void MainWindow::setPowerInfo(const QString& text) {
    if (m_powerLabel) m_powerLabel->setText("Power: " + text);
}
