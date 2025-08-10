
#include "MainWindow.h"
#include "DbusClient.h"
#include "Config.h"
#include "LoadGraphWidget.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QCursor>
#include <QLabel>
#include <QScrollArea>
#include <QSpacerItem>
#include <QLabel>

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
    for (const auto& r : rules) {
        auto* btn = new QPushButton(r.process_name, m_rulesPanel);
        btn->setToolTip(QString("Priority %1 · Mode %2").arg(r.priority).arg(r.active_profile));
        // For now: stub handler (editor comes later)
        connect(btn, &QPushButton::clicked, this, [name=r.name]() {
            // placeholder – will open editor next step
            qInfo("Clicked rule: %s", name.toUtf8().constData());
        });
        m_rulesLayout->addWidget(btn);
    }

    // "Add process to match" button at bottom
    auto* addBtn = new QPushButton("Add process to match", m_rulesPanel);
    connect(addBtn, &QPushButton::clicked, this, []() {
        // placeholder – will open editor next step
        qInfo("Add process clicked");
    });
    m_rulesLayout->addWidget(addBtn);

    // Spacer to push buttons to top
    m_rulesLayout->addItem(new QSpacerItem(0,0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void MainWindow::setPowerInfo(const QString& text) {
    if (m_powerLabel) m_powerLabel->setText("Power: " + text);
}
