
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

    // Power info label
    m_powerLabel = new QLabel(this);
    m_powerLabel->setText("Power: …");
    m_powerLabel->setStyleSheet("color: black;");
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
