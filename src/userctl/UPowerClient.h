#pragma once
#include <QObject>
#include <QDBusInterface>
#include <QVariant>

class UPowerClient : public QObject {
    Q_OBJECT
public:
    explicit UPowerClient(QObject* parent = nullptr);
    bool onBattery() const { return m_onBattery; }
    double percentage() const { return m_percentage; }    // 0..100
    QString stateText() const;                             // Charging/Discharging/…
    QString summaryText() const;                           // e.g. "AC • 67% (Discharging)"

signals:
    void powerInfoChanged();

private slots:
    void onUPowerPropsChanged(QString iface, QVariantMap changed, QStringList invalidated);

private:
    void refresh();
    bool getBool(const QString& path, const QString& iface, const QString& prop, bool def=false) const;
    double getDouble(const QString& path, const QString& iface, const QString& prop, double def=0.0) const;
    uint getUint(const QString& path, const QString& iface, const QString& prop, uint def=0) const;
    QString displayDevicePath() const;

    bool m_onBattery = false;
    double m_percentage = 0.0;
    uint m_state = 0; // UPower.Device.State enum
};
