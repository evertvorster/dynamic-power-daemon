
#pragma once
#include <QDialog>
#include "Config.h"

class QLineEdit;
class QPushButton;
class QSlider;

class ProcessRuleEditor : public QDialog {
    Q_OBJECT
public:
    explicit ProcessRuleEditor(QWidget* parent = nullptr);
    void setRule(const ProcessRule& r);
    ProcessRule rule() const;

signals:
    void deleteRequested();

private slots:
    void chooseMode();

private:
    QLineEdit* m_nameEdit;
    QLineEdit* m_procEdit;
    QPushButton* m_modeBtn;
    QSlider* m_prioSlider;
    QString m_mode;
};
