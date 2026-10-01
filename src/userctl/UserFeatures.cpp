// File: src/userctl/UserFeatures.cpp
#include "UserFeatures.h"
#include "features/ScreenRefreshFeature.h"
#include "features/PanelAutohideFeature.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(dpUser, "dp.userfeatures")

UserFeaturesWidget::UserFeaturesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(6);

    // Row: [checkbox] "Screen Refresh Rate — Current: ..." [AC] [BAT]
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0,0,0,0);

    m_screenEnabled = new QCheckBox(row);
    h->addWidget(m_screenEnabled);

    m_status = new QLabel("Screen Refresh Rate — Current: (detecting…)", row);
    h->addWidget(m_status, 1);

    m_acBtn  = new QPushButton("Unchanged", row);
    m_batBtn = new QPushButton("Unchanged", row);
    h->addWidget(m_acBtn);
    h->addWidget(m_batBtn);

    connect(m_acBtn,  &QPushButton::clicked, this, [this]{ m_acBtn->setText(cycle3(m_acBtn->text())); });
    connect(m_batBtn, &QPushButton::clicked, this, [this]{ m_batBtn->setText(cycle3(m_batBtn->text())); });

    outer->addWidget(row);
    
    // Row: [checkbox] "KDE Panel Autohide" [AC] [BAT]
    auto* row2 = new QWidget(this);
    auto* h2 = new QHBoxLayout(row2);
    h2->setContentsMargins(0,0,0,0);

    m_panelEnabled = new QCheckBox(row2);
    h2->addWidget(m_panelEnabled);

    m_panelStatus = new QLabel("KDE Panel Autohide — (detecting…)", row2);
    h2->addWidget(m_panelStatus, 1);

    m_panelAcBtn  = new QPushButton("Unchanged", row2);
    m_panelBatBtn = new QPushButton("Unchanged", row2);
    h2->addWidget(m_panelAcBtn);
    h2->addWidget(m_panelBatBtn);

    connect(m_panelAcBtn,  &QPushButton::clicked, this, [this]{ m_panelAcBtn->setText(cyclePanelMode(m_panelAcBtn->text())); });
    connect(m_panelBatBtn, &QPushButton::clicked, this, [this]{ m_panelBatBtn->setText(cyclePanelMode(m_panelBatBtn->text())); });

    outer->addWidget(row2);

}

QString UserFeaturesWidget::configPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + "/dynamic_power/config.yaml";
}

QString UserFeaturesWidget::cycle3(const QString& cur) {
    if (cur.compare("Unchanged", Qt::CaseInsensitive) == 0) return "Min";
    if (cur.compare("Min", Qt::CaseInsensitive) == 0)       return "Max";
    return "Unchanged";
}

QString UserFeaturesWidget::cycleOnOff(const QString& cur) {
    if (cur.compare("Unchanged", Qt::CaseInsensitive) == 0) return "On";
    if (cur.compare("On", Qt::CaseInsensitive) == 0)        return "Off";
    return "Unchanged";
}

QString UserFeaturesWidget::cyclePanelMode(const QString& cur) {
    // UI shows Title Case; cycle through in this order:
    // Unchanged -> None -> Autohide -> DodgeWindows -> WindowsGoBelow -> Unchanged
    if (cur.compare("Unchanged", Qt::CaseInsensitive) == 0) return "None";
    if (cur.compare("None", Qt::CaseInsensitive) == 0) return "Autohide";
    if (cur.compare("Autohide", Qt::CaseInsensitive) == 0) return "DodgeWindows";
    if (cur.compare("DodgeWindows", Qt::CaseInsensitive) == 0) return "WindowsGoBelow";
    return "Unchanged";
}

QString UserFeaturesWidget::normalizePolicy(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "min" || t == "max") return t;
    return "unchanged";
}

void UserFeaturesWidget::load() {
    // Screen Refresh
    dp::features::ScreenRefreshFeature srf;
    auto sr = srf.readState();
    m_screenEnabled->setChecked(sr.enabled);
    auto toTitleHz = [](QString v){ v = v.toLower(); if (v=="min") return QString("Min"); if (v=="max") return QString("Max"); return QString("Unchanged"); };
    m_acBtn->setText(toTitleHz(sr.ac));
    m_batBtn->setText(toTitleHz(sr.battery));

    // Panel Autohide
    dp::features::PanelAutohideFeature paf;
    auto pa = paf.readState();
    m_panelEnabled->setChecked(pa.enabled);
    auto toTitlePanel = [](QString v){
        v = v.toLower();
        if (v == "none"           || v == "off" || v == "normal" || v == "always") return QString("None");
        if (v == "autohide"       || v == "on") return QString("Autohide");
        if (v == "dodgewindows"   || v == "dodge") return QString("DodgeWindows");
        if (v == "windowsgobelow" || v == "windowsbelow" || v == "below") return QString("WindowsGoBelow");
        return QString("Unchanged");
    };
    m_panelAcBtn->setText(toTitlePanel(pa.ac));
    m_panelBatBtn->setText(toTitlePanel(pa.battery));
}


bool UserFeaturesWidget::save() {
    // Screen Refresh
    dp::features::ScreenRefreshFeature srf;
    dp::features::ScreenRefreshFeature::State sr;
    sr.enabled = m_screenEnabled->isChecked();
    sr.ac      = m_acBtn->text();
    sr.battery = m_batBtn->text();
    bool ok1 = srf.writeState(sr);

    // Panel Autohide
    dp::features::PanelAutohideFeature paf;
    dp::features::PanelAutohideFeature::State pa;
    pa.enabled = m_panelEnabled->isChecked();
    pa.ac      = m_panelAcBtn->text();
    pa.battery = m_panelBatBtn->text();
    bool ok2 = paf.writeState(pa);

    return ok1 && ok2;
}


void UserFeaturesWidget::refreshLiveStatus() {
    dp::features::ScreenRefreshFeature srf;
    m_status->setText(srf.statusText());

    dp::features::PanelAutohideFeature paf;
    if (m_panelStatus) m_panelStatus->setText(paf.statusText());
}

