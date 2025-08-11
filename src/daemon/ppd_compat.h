#pragma once
#include <QDBusAbstractAdaptor>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include "config/config.h"   // for global `profiles`
class Daemon;

class PpdCompatAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.hadess.PowerProfiles")
    Q_PROPERTY(QString ActiveProfile READ activeProfile WRITE setActiveProfile NOTIFY ActiveProfileChanged)
    Q_PROPERTY(QVariantList Profiles READ profiles)
    Q_PROPERTY(QString PerformanceInhibited READ performanceInhibited)
    Q_PROPERTY(QStringList Actions READ actions)

public:
    explicit PpdCompatAdaptor(Daemon* daemon);

    QString activeProfile() const;
    void setActiveProfile(const QString& externalName);

    QVariantList profiles() const;              // aa{sv}
    QString performanceInhibited() const { return QString(); }
    QStringList actions() const { return {}; }

Q_SIGNALS:
    void ActiveProfileChanged(const QString& newExternal);

private Q_SLOTS:
    void onProfileChanged(const QString& internalName);

private:
    Daemon* m_daemon;
    static QString toExternal(const QString& internal) {
        return internal == "powersave" ? "power-saver" : internal;
    }
    static QString toInternal(const QString& external) {
        return external == "power-saver" ? "powersave" : external;
    }
};
