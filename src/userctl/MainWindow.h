
#pragma once
#include <QMainWindow>
#include <QString>
#include <QShowEvent>
#include <QHideEvent>
#include <QCloseEvent>

class DbusClient;
class Config;
class LoadGraphWidget;
class QPushButton;
class QWidget;
class QVBoxLayout;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(DbusClient* dbus, Config* config, QWidget* parent = nullptr);
    void setThresholds(double low, double high);
    void setActiveProfile(const QString& profile);
    QString currentUserMode() const { return m_userMode; }
    void refreshProcessButtons();
    void setPowerInfo(const QString& text);

signals:
    void userOverrideSelected(const QString& mode, bool boss);
    void thresholdsAdjusted(double low, double high);
    void visibilityChanged(bool visible);

protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onOverrideButtonClicked();
    void onGraphThresholdChanged(double low, double high);

private:
    DbusClient* m_dbus;
    Config* m_config;
    LoadGraphWidget* m_graph;
    QPushButton* m_overrideBtn;
    QString m_activeProfile = "balanced";
    QString m_userMode = "Dynamic"; // default
    void refreshOverrideButton();
    QWidget* m_rulesPanel = nullptr;
    QVBoxLayout* m_rulesLayout = nullptr;
    QLabel* m_powerLabel = nullptr;
};
