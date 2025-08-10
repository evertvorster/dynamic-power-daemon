#include "ProcessMonitor.h"
#include "Config.h"
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QLoggingCategory>
#include <sys/types.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(dpProc, "dynamic_power.userctl.process", QtInfoMsg)

ProcessMonitor::ProcessMonitor(QObject* parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::tick);
}

void ProcessMonitor::setRules(const QVector<ProcessRule>& rules) { m_rules = rules; }
void ProcessMonitor::start(int intervalMs) { m_timer->start(intervalMs); }
void ProcessMonitor::stop() { m_timer->stop(); }

QSet<QString> ProcessMonitor::currentProcesses() const {
    QSet<QString> names;
    const uid_t myuid = getuid();

    QDir proc("/proc");
    for (const auto& entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (!ok) continue;

        // Filter by UID using /proc/<pid> ownership (robust)
        QFileInfo pinfo(QString("/proc/%1").arg(pid));
        if (!pinfo.exists() || pinfo.ownerId() != myuid) continue;

        // Prefer basename of /proc/<pid>/exe
        QString name;
        QFileInfo exe(QString("/proc/%1/exe").arg(pid));
        if (exe.exists()) {
            const QString target = exe.symLinkTarget();
            if (!target.isEmpty()) name = QFileInfo(target).fileName();
        }
        // Fallback to /proc/<pid>/comm
        if (name.isEmpty()) {
            QFile com(QString("/proc/%1/comm").arg(pid));
            if (com.open(QIODevice::ReadOnly | QIODevice::Text)) {
                name = QString::fromUtf8(com.readLine()).trimmed();
            }
        }

        if (!name.isEmpty()) {
            const QString lname = name.toLower();
            names.insert(lname);
            qCDebug(dpProc) << "pid" << pid << "name" << lname;
        }
    }

    // qCDebug(dpProc) << "user-procs total:" << names.size();  // Prints all processes, to noisy
    return names;
}


void ProcessMonitor::tick() {
    const auto procs = currentProcesses();
    int bestPrio = -1;
    QString bestProfile;
    QString bestProc;                 // lowercased name of winning rule
    QSet<QString> matches;            // all matched lowercased names

    for (const auto& r : m_rules) {
        const QString match = r.process_name.toLower();
        if (procs.contains(match)) {
            matches.insert(match);
            if (r.priority > bestPrio) {
                bestPrio = r.priority;
                bestProfile = r.active_profile;
                bestProc = match;
            }
        }
    }
    qCDebug(dpProc) << "process matches:" << matches.values();
    emit matchesUpdated(matches, (bestPrio >= 0 ? bestProc : QString()));
    if (bestPrio >= 0) {
        if (!m_hasMatch) qCDebug(dpProc) << "matched →" << bestProfile << "prio" << bestPrio;
        m_hasMatch = true;
        emit matchedProfile(bestProfile);
    } else {
        if (m_hasMatch) {
            qCDebug(dpProc) << "no match";
            m_hasMatch = false;
            emit noMatch();                     // <— notify clear
        } else {
            m_hasMatch = false;                 // still none; nothing to do
        }
    }
}
