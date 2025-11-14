
#include "MainWindow.h"
#include "DbusClient.h"
#include "Config.h"
#include "LoadGraphWidget.h"
#include "ProcessRuleEditor.h"
#include "features/RootCompositeFeature.h"
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
#include "UserFeatures.h"

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
        setWindowTitle("Power Saving Features");
        resize(760, 560);

        auto* outer = new QVBoxLayout(this);
        // ─────────────────────────────────────────────────────────────────────
        // User Power Saving Features (own section, own Save button) — TOP
        // ─────────────────────────────────────────────────────────────────────
        auto* userGroup = new QGroupBox("User Power Saving Features", this);
        // Center the group title and enlarge it (no per-row headers needed)
        userGroup->setStyleSheet(
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top center;"
            "  font-size: 15pt;"
            "  font-weight: 600;"
            "  padding: 6px 8px;"
            "}"
        );
        auto* userLay = new QVBoxLayout(userGroup);

        // NEW: header row for AC/BAT columns
        auto* headerRow = new QWidget(userGroup);
        auto* hh = new QHBoxLayout(headerRow);
        hh->setContentsMargins(0,0,0,0);

        // Spacer over the checkbox column
        hh->addSpacing(24);

        // Spacer over the description/status column
        hh->addStretch(1);

        auto* acHeader  = new QLabel("On AC        ", headerRow);
        auto* batHeader = new QLabel("On Battery   ", headerRow);
        acHeader->setAlignment(Qt::AlignCenter);
        batHeader->setAlignment(Qt::AlignCenter);
        hh->addWidget(acHeader);
        hh->addWidget(batHeader);

        userLay->addWidget(headerRow);

        m_userWidget = new UserFeaturesWidget(userGroup);
        userLay->addWidget(m_userWidget);

        // Save (User)
        m_userSaveBtn = new QPushButton("Save (User)", userGroup);
        auto* btnRowUser = new QHBoxLayout();
        btnRowUser->addStretch(1);
        btnRowUser->addWidget(m_userSaveBtn);
        userLay->addLayout(btnRowUser);

        connect(m_userSaveBtn, &QPushButton::clicked, this, [this]{
            if (m_userWidget->save()) {
                QMessageBox::information(this, "Saved", "User features saved.");
            } else {
                QMessageBox::critical(this, "Error", "Unable to write user config file.");
            }
        });

        outer->addWidget(userGroup);

        // Populate values and detect current screen refresh per monitor
        m_userWidget->load();
        m_userWidget->refreshLiveStatus();


        // Disclaimer strip
        auto* disclaimBox = new QHBoxLayout();
        m_confirmBtn = new QPushButton(this);
        m_confirmBtn->setText("Confirm ❌");
        m_confirmBtn->setToolTip("Read and accept the disclaimer to enable saving.");
        disclaimBox->addWidget(m_confirmBtn, 0);
        m_confirmNote = new QLabel("Saving is blocked until you accept the disclaimer.", this);
        disclaimBox->addWidget(m_confirmNote, 1);

        connect(m_confirmBtn, &QPushButton::clicked, this, [this]{ showDisclaimer(); });

        // Rows area inside a group + scroll
        auto* group = new QGroupBox("Root-required features (writes to /etc/dynamic_power.yaml)", this);
        auto* groupLay = new QVBoxLayout(group);
        groupLay->addLayout(disclaimBox);

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
        hgrid->setColumnStretch(2, 8); // [→] Path (wide)
        hgrid->setColumnStretch(3, 1); // [→] AC value
        hgrid->setColumnStretch(4, 1); // [→] Battery value
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
        auto* lblBat = new QLabel("BAT value", header);
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

    // User section
    class UserFeaturesWidget* m_userWidget{};
    QPushButton* m_userSaveBtn{};

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
        grid->setColumnStretch(2, 8); // Path
        grid->setColumnStretch(3, 1); // AC value
        grid->setColumnStretch(4, 1); // Battery value
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
        dp::features::RootCompositeFeature feat(m_etcPath);
        auto st = feat.read();
        m_disclaimerAccepted = st.disclaimerAccepted;
        m_disclaimerAcceptedAt = st.acceptedAt;
        clearRows();
        for (const auto& it : st.items) {
            addRow(it.enabled, it.absPath, it.ac, it.battery);
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
        dp::features::RootCompositeFeature feat(m_etcPath);
        dp::features::RootCompositeFeature::State st;
        st.disclaimerAccepted = m_disclaimerAccepted;
        st.acceptedAt = m_disclaimerAcceptedAt;
        for (const auto& r : m_rows) {
            QString rel = r.pathRel->text().trimmed();
            if (rel.startsWith("/")) rel = rel.mid(1);
            const QString prefix = r.rootBtn->text().trimmed();   // "/sys" or "/proc"
            const QString abs = QString("%1/%2").arg(prefix, rel);
            dp::features::RootCompositeFeature::Item it;
            it.enabled = r.enabled->isChecked();
            it.absPath = abs;
            it.ac = r.ac->text().trimmed();
            it.battery = r.bat->text().trimmed();
            st.items.push_back(it);
        }
        QByteArray data = feat.serialize(st);
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
        if (m_rootDialog) { m_rootDialog->raise(); m_rootDialog->activateWindow(); return; }
        m_rootDialog = new RootFeaturesDialog(this, "/etc/dynamic_power.yaml");
        m_rootDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(m_rootDialog, &QObject::destroyed, this, [this]{ m_rootDialog = nullptr; });
        m_rootDialog->show();
    });

    // Power info label
    m_powerLabel = new QLabel(this);
    m_powerLabel->setText("Power: …");
    // Use theme default color; just make it bold
    QFont f = m_powerLabel->font();
    f.setBold(true);
    m_powerLabel->setFont(f);
    m_powerLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_powerLabel);

    // --- Process matches section ---
    auto* sectionLabel = new QLabel("Process Matches", this);
    sectionLabel->setAlignment(Qt::AlignHCenter);
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
    resize(600, 600);
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
    m_overrideBtn->setText(QString("User Override: %1 - Current power profile: %2")
                           .arg(m_userMode, m_activeProfile));
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

    // One button per config rule: show friendly name; fall back to process_name
    const auto& rules = m_config->processRules();
    for (int i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];

        const QString label = r.name.trimmed().isEmpty() ? r.process_name : r.name;
        auto* btn = new QPushButton(label, m_rulesPanel);
        btn->setToolTip(QString("Process “%1” · Priority %2 · Mode %3")
                            .arg(r.process_name)
                            .arg(r.priority)
                            .arg(r.active_profile));

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
    m_powerLabel->setText(text);
}

void MainWindow::closeFeaturesDialogIfOpen() {
    if (m_rootDialog) m_rootDialog->close();
}