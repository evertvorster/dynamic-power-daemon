
#pragma once
#include <QObject>
#include <QVector>

struct ProcessRule;

class QTimer;

class ProcessMonitor : public QObject {
    Q_OBJECT
public:
    explicit ProcessMonitor(QObject* parent = nullptr);
    void setRules(const QVector<ProcessRule>& rules);
    void start(int intervalMs = 5000);
    void stop();
    bool hasActiveMatch() const { return m_hasMatch; }

signals:
    void matchedProfile(const QString& profile);

private slots:
    void tick();

private:
    QVector<ProcessRule> m_rules;
    QTimer* m_timer;
    bool m_hasMatch = false;
    QStringList currentProcesses() const;
};
