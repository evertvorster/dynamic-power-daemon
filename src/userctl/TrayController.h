
#pragma once
#include <QObject>
#include <QSystemTrayIcon>

class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject* parent = nullptr);
    void setIconByKey(const QString& key);
    void setTooltip(const QString& tip);

signals:
    void showMainRequested();

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    QSystemTrayIcon m_tray;
    QIcon iconForKey(const QString& key);
};
