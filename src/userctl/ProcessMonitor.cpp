
#include "ProcessMonitor.h"
#include "Config.h"
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <unistd.h>

ProcessMonitor::ProcessMonitor(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::tick);
}

void ProcessMonitor::setRules(const QVector<ProcessRule>& rules) {
    m_rules = rules;
}

void ProcessMonitor::start(int intervalMs) {
    m_timer->start(intervalMs);
}

void ProcessMonitor::stop() {
    m_timer->stop();
}

QStringList ProcessMonitor::currentProcesses() const {
    QStringList names;
    QDir proc("/proc");
    for (const auto& entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok=false;
        int pid = entry.toInt(&ok);
        if (!ok) continue;
        QString commPath = QString("/proc/%1/comm").arg(pid);
        QFile f(commPath);
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) {
            QTextStream in(&f);
            QString name = in.readLine().trimmed();
            if (!name.isEmpty()) names << name;
        }
    }
    names.removeDuplicates();
    return names;
}

void ProcessMonitor::tick() {
    auto procs = currentProcesses();
    int bestPrio = -1;
    QString bestProfile;
    for (const auto& r : m_rules) {
        if (procs.contains(r.process_name)) {
            if (r.priority > bestPrio) {
                bestPrio = r.priority;
                bestProfile = r.active_profile;
            }
        }
    }
    if (bestPrio >= 0) {
        m_hasMatch = true;
        emit matchedProfile(bestProfile);
    } else {
        m_hasMatch = false;
    }
}
