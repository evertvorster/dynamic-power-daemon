#include "ppd_compat.h"
#include "daemon.h"
#include "log.h"
#include <QVariantMap>
#include <QList>

PpdCompatAdaptor::PpdCompatAdaptor(Daemon* daemon)
    : QDBusAbstractAdaptor(daemon), m_daemon(daemon)
{
    setAutoRelaySignals(true);
    QObject::connect(m_daemon, SIGNAL(ProfileChanged(QString)),
                     this, SLOT(onProfileChanged(QString)));
}

QString PpdCompatAdaptor::activeProfile() const {
    // Expose current internal profile using PPD's external spelling
    return toExternal(m_daemon->getActiveProfile());
}

void PpdCompatAdaptor::setActiveProfile(const QString& externalName) {
    const QString internal = toInternal(externalName);
    log_info(QString("PPD shim Set ActiveProfile → '%1'").arg(internal).toUtf8().constData());
    // Treat UI requests as “boss override”
    m_daemon->setRequestedProfile(internal, /*userRequested*/true);
    // Optional: apply immediately (uncomment if you want zero-latency switching)
    // m_daemon->setProfile(internal);
    // Emit a change so UIs reflect the new requested state promptly
    emit ActiveProfileChanged(externalName);
}

QVariantList PpdCompatAdaptor::profiles() const {
    // Build aa{sv}: each entry is a dict {"Profile": s, "Driver": s}
    QVariantList out;
    for (const auto& kv : ::profiles) {
        const QString internal = QString::fromStdString(kv.first);
        QVariantMap entry;
        entry.insert("Profile", toExternal(internal));
        entry.insert("Driver", "dynamic-power");
        out.push_back(entry);
    }
    return out;
}

void PpdCompatAdaptor::onProfileChanged(const QString& internalName) {
    emit ActiveProfileChanged(toExternal(internalName));
}
