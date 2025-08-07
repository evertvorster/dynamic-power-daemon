#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>

class Daemon : public QObject {
    Q_OBJECT

public:
    Daemon(QObject *parent = nullptr);

private Q_SLOTS:
    void handlePropertiesChanged(const QDBusMessage &message);
};
