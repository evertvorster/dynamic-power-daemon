#include "MainWindow.h"
#include "DbusClient.h"
#include "Config.h"
#include "LoadGraphWidget.h"
#include "ProcessRuleEditor.h"
#include "ProfileConfigDialog.h"
#include "RootFeaturesDialog.h"

#include <QCursor>
#include <QDialog>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>
#include "common/config_paths.h"

MainWindow::MainWindow(DbusClient* dbus, Config* config, QWidget* parent)
    : QMainWindow(parent), m_dbus(dbus), m_config(config)
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_graph = new LoadGraphWidget(this);
    layout->addWidget(m_graph);
    connect(m_graph, &LoadGraphWidget::thresholdsPreview, this, [this](double low, double high) {
        Q_UNUSED(low);
        Q_UNUSED(high);
    });
    connect(m_graph, &LoadGraphWidget::thresholdsCommitted, this, &MainWindow::onGraphThresholdChanged);

    m_overrideBtn = new QPushButton(this);
    layout->addWidget(m_overrideBtn);
    connect(m_overrideBtn, &QPushButton::clicked, this, &MainWindow::onOverrideButtonClicked);

    auto* profileBtn = new QPushButton("Profile Configuration", this);
    layout->addWidget(profileBtn);
    connect(profileBtn, &QPushButton::clicked, this, [this] {
        ProfileConfigDialog dlg(this, DEFAULT_CONFIG_PATH);
        dlg.exec();
    });

    auto* rootFeatBtn = new QPushButton("Power-saving Features…", this);
    layout->addWidget(rootFeatBtn);
    connect(rootFeatBtn, &QPushButton::clicked, this, [this] {
        if (m_rootDialog) {
            m_rootDialog->raise();
            m_rootDialog->activateWindow();
            return;
        }
        m_rootDialog = new RootFeaturesDialog(this, DEFAULT_CONFIG_PATH);
        m_rootDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(m_rootDialog, &QObject::destroyed, this, [this] { m_rootDialog = nullptr; });
        m_rootDialog->show();
    });

    m_powerLabel = new QLabel(this);
    m_powerLabel->setText("Power: …");
    QFont f = m_powerLabel->font();
    f.setBold(true);
    m_powerLabel->setFont(f);
    m_powerLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_powerLabel);

    auto* sectionLabel = new QLabel("Process Matches", this);
    sectionLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(sectionLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_rulesPanel = new QWidget(scroll);
    m_rulesLayout = new QVBoxLayout(m_rulesPanel);
    m_rulesLayout->setContentsMargins(6, 6, 6, 6);
    scroll->setWidget(m_rulesPanel);
    layout->addWidget(scroll);

    layout->setStretch(0, 3);
    layout->setStretch(2, 1);

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

void MainWindow::closeEvent(QCloseEvent* e) {
    // Closing hides the window rather than quitting; the app lives in the tray.
    e->ignore();
    hide();
}

void MainWindow::onOverrideButtonClicked() {
    QMenu menu(this);
    const QStringList modes = {"Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"};
    for (const auto& m : modes) {
        QAction* act = menu.addAction(m);
        connect(act, &QAction::triggered, this, [this, m]() {
            m_userMode = m;
            const bool boss = (m != QStringLiteral("Dynamic"));
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
    if (!m_rulesLayout) return;
    QLayoutItem* child;
    while ((child = m_rulesLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }

    const auto& rules = m_config->processRules();
    for (int i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];
        const QString label = r.name.trimmed().isEmpty() ? r.process_name : r.name;
        auto* btn = new QPushButton(label, m_rulesPanel);
        btn->setToolTip(QString("Process “%1” · Priority %2 · Mode %3")
                            .arg(r.process_name)
                            .arg(r.priority)
                            .arg(r.active_profile));

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            auto current = m_config->processRules();
            if (i < 0 || i >= current.size()) return;

            ProcessRuleEditor dlg(this);
            dlg.setRule(current[i]);

            bool didDelete = false;
            connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&] { didDelete = true; });

            if (dlg.exec() == QDialog::Accepted) {
                if (didDelete) current.removeAt(i);
                else current[i] = dlg.rule();
                m_config->setProcessRules(current);
                m_config->save();
                refreshProcessButtons();
            }
        });

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

    auto* addBtn = new QPushButton("Add process to match", m_rulesPanel);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        ProcessRuleEditor dlg(this);
        ProcessRule empty;
        dlg.setRule(empty);

        bool didDelete = false;
        connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&] { didDelete = true; });

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
    m_rulesLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void MainWindow::setPowerInfo(const QString& text) {
    m_powerLabel->setText(text);
}
