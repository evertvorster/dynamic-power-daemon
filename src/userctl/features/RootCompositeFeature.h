// File: src/userctl/features/RootCompositeFeature.h
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace dp::features {

class RootCompositeFeature {
public:
    struct Node {
        QString id;
        QString parentId;
        QString absPath;
        QString label;
        QString nodeClass;
        bool enabled = false;
        QString acValue;
        QString batteryValue;
        QString policyScope = QStringLiteral("override");
        bool supported = true;
        bool detected = false;
        bool legacy = false;
        bool isGroup = false;
        QString currentValue;
        QStringList allowedValues;
    };
    struct State {
        bool disclaimerAccepted = false;
        QString acceptedAt;
        QVector<Node> nodes;
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
