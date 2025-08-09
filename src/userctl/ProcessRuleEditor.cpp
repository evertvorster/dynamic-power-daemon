
#include "ProcessRuleEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QMenu>

ProcessRuleEditor::ProcessRuleEditor(QWidget* parent) : QDialog(parent) {
    auto* v = new QVBoxLayout(this);
    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Name:"));
    m_nameEdit = new QLineEdit;
    nameRow->addWidget(m_nameEdit);
    v->addLayout(nameRow);

    auto* procRow = new QHBoxLayout();
    procRow->addWidget(new QLabel("Process:"));
    m_procEdit = new QLineEdit;
    procRow->addWidget(m_procEdit);
    v->addLayout(procRow);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Power Mode:"));
    m_modeBtn = new QPushButton("Dynamic");
    connect(m_modeBtn, &QPushButton::clicked, this, &ProcessRuleEditor::chooseMode);
    modeRow->addWidget(m_modeBtn);
    v->addLayout(modeRow);

    auto* prioRow = new QHBoxLayout();
    prioRow->addWidget(new QLabel("Priority:"));
    m_prioSlider = new QSlider(Qt::Horizontal);
    m_prioSlider->setRange(0, 100);
    prioRow->addWidget(m_prioSlider);
    v->addLayout(prioRow);

    auto* buttons = new QHBoxLayout();
    auto* delBtn = new QPushButton("Delete");
    auto* applyBtn = new QPushButton("Apply");
    auto* closeBtn = new QPushButton("Close");
    connect(delBtn, &QPushButton::clicked, this, [this]() { emit deleteRequested(); accept(); });
    connect(applyBtn, &QPushButton::clicked, this, [this]() { accept(); });
    connect(closeBtn, &QPushButton::clicked, this, [this]() { reject(); });
    buttons->addWidget(delBtn);
    buttons->addWidget(applyBtn);
    buttons->addWidget(closeBtn);
    v->addLayout(buttons);

    setWindowTitle("Edit Process Rule");
    resize(360, 200);
}

void ProcessRuleEditor::setRule(const ProcessRule& r) {
    m_nameEdit->setText(r.name);
    m_procEdit->setText(r.process_name);
    m_mode = r.active_profile;
    if (m_mode.isEmpty()) m_mode = "Dynamic";
    m_modeBtn->setText(m_mode);
    m_prioSlider->setValue(r.priority);
}

ProcessRule ProcessRuleEditor::rule() const {
    ProcessRule r;
    r.name = m_nameEdit->text();
    r.process_name = m_procEdit->text();
    r.active_profile = m_mode;
    r.priority = m_prioSlider->value();
    return r;
}

void ProcessRuleEditor::chooseMode() {
    QMenu menu(this);
    QStringList modes = {"Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"};
    for (const auto& m : modes) {
        QAction* act = menu.addAction(m);
        connect(act, &QAction::triggered, this, [this, m]() {
            m_mode = m;
            m_modeBtn->setText(m);
        });
    }
    menu.exec(QCursor::pos());
}
