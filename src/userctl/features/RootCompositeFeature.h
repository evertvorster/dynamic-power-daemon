// File: src/userctl/features/RootCompositeFeature.h
#pragma once
#include <QString>
#include <QVector>

namespace dp::features {

class RootCompositeFeature {
public:
    struct Item {
        bool enabled = false;
        QString absPath;   // absolute path like "/sys/...." or "/proc/..."
        QString ac;        // value to write on AC
        QString battery;   // value to write on Battery
    };
    struct State {
        bool disclaimerAccepted = false;
        QString acceptedAt;        // ISO string if present
        QVector<Item> items;       // ordered as in file
    };

    explicit RootCompositeFeature(const QString& etcPath = QStringLiteral("/etc/dynamic_power.yaml"));
    const QString& etcPath() const { return m_etcPath; }

    State read() const;
    bool write(const State& s) const;
    QByteArray serialize(const State& s) const;

private:
    QString m_etcPath;
};

} // namespace dp::features
