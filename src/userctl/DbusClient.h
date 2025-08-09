
#pragma once
#include <QObject>
#include <QVariantMap>

class QDBusInterface;

class DbusClient : public QObject {
    Q_OBJECT
public:
    explicit DbusClient(QObject* parent = nullptr);
    QVariantMap getDaemonState() const;
    bool setProfile(const QString& profile, bool boss);
    bool setLoadThresholds(double low, double high);
    bool setPollInterval(unsigned int ms);

signals:
    void powerStateChanged();

private:
    QDBusInterface* m_iface;
    void connectSignals();
};
