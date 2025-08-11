
#include "TrayController.h"
#include <QMenu>
#include <QStyle>
#include <QApplication>

TrayController::TrayController(QObject* parent) : QObject(parent) {
    m_tray.setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    m_tray.setToolTip("dynamic_power");
    // Context menu with Quit
    auto *menu = new QMenu;                       // unparented is fine for tray menus
    auto *openAct = menu->addAction("Open Dynamic Power");
    connect(openAct, &QAction::triggered, this, [this]{ emit showMainRequested(); });

    auto *quitAct = menu->addAction("Quit");
    connect(quitAct, &QAction::triggered, qApp, &QCoreApplication::quit);

    m_tray.setContextMenu(menu);    
    connect(&m_tray, &QSystemTrayIcon::activated, this, &TrayController::onActivated);
    m_tray.show();
}

void TrayController::setIconByKey(const QString& key) {
    m_tray.setIcon(iconForKey(key));
}

void TrayController::setTooltip(const QString& tip) {
    m_tray.setToolTip(tip);
}

void TrayController::onActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        emit showMainRequested();
    }
}

QIcon TrayController::iconForKey(const QString& key) {
    const QString name = QStringLiteral("dynamic_power.%1").arg(key); // e.g. default_ac
    QIcon themed = QIcon::fromTheme(name);
    if (!themed.isNull())
        return themed;

    // Fallbacks (keep existing placeholders)
    if (key.contains("override")) return QApplication::style()->standardIcon(QStyle::SP_DialogYesButton);
    if (key.contains("match"))    return QApplication::style()->standardIcon(QStyle::SP_BrowserReload);
    return QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
}
