// Tests for src/daemon/cpu_targets.cpp.
//
// This decides which sysfs nodes the daemon writes for the governor and the EPP
// field. The config names a single example path; the kernel exposes one knob per
// policy or per CPU; getting it wrong means writing to one CPU out of sixteen and
// reporting success. Both fields used to carry a private copy of this walk, and the
// copies had drifted, so it is worth pinning down.
//
// Everything here is read-only and runs against a temporary directory tree, except
// the last case, which checks the path the shipped config actually names.

#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "daemon/cpu_targets.h"

using dp::daemon::cpuTargetsFor;
using dp::daemon::availableValuesFileName;

class TestCpuTargets : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void policyShapeReturnsEveryPolicy();
    void cpuShapeReturnsEveryCpu();
    void theKernelRootsAreWhereWeThink();
    void onlyNumericSuffixesMatch();
    void anUnrecognisedShapeFallsBackToTheConfiguredPath();
    void anUnrelatedFieldNameFallsBackToTheConfiguredPath();
    void theShippedConfigPathResolvesOnThisMachine();
    void namesTheAttributeThatExplainsARejection();

private:
    // Builds a directory tree and touches every file in `files` (paths relative to
    // the temporary root).
    void plant(const QStringList& files);
    QTemporaryDir m_dir;
    int m_seq = 0;
    QString m_root;
};

void TestCpuTargets::initTestCase()
{
    QVERIFY(m_dir.isValid());
}

void TestCpuTargets::plant(const QStringList& files)
{
    m_root = m_dir.filePath(QStringLiteral("tree-%1").arg(++m_seq));
    for (const QString& rel : files) {
        const QString abs = m_root + QLatin1Char('/') + rel;
        QVERIFY(QDir().mkpath(QFileInfo(abs).absolutePath()));
        QFile f(abs);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("test\n");
    }
}

static QStringList asList(const std::vector<std::string>& v)
{
    QStringList out;
    for (const auto& s : v) out << QString::fromStdString(s);
    return out;
}

void TestCpuTargets::policyShapeReturnsEveryPolicy()
{
    plant({ QStringLiteral("cpu/cpufreq/policy0/scaling_governor"),
            QStringLiteral("cpu/cpufreq/policy1/scaling_governor"),
            QStringLiteral("cpu/cpufreq/policy2/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/cpufreq/policy0/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));

    QCOMPARE(got.size(), 3);
    for (const QString& t : got)
        QVERIFY2(t.endsWith(QStringLiteral("/cpufreq/policy0/scaling_governor")) ||
                 t.endsWith(QStringLiteral("/cpufreq/policy1/scaling_governor")) ||
                 t.endsWith(QStringLiteral("/cpufreq/policy2/scaling_governor")), qPrintable(t));
}

void TestCpuTargets::cpuShapeReturnsEveryCpu()
{
    // The shape the shipped template actually names.
    plant({ QStringLiteral("cpu/cpu0/cpufreq/scaling_governor"),
            QStringLiteral("cpu/cpu1/cpufreq/scaling_governor"),
            QStringLiteral("cpu/cpu10/cpufreq/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/cpu0/cpufreq/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));

    QCOMPARE(got.size(), 3);
    for (const QString& t : got)
        QVERIFY2(t.contains(QStringLiteral("/cpufreq/scaling_governor")), qPrintable(t));
    // cpu10 must be included, which is why the suffix has to be all digits.
    QVERIFY(got.filter(QStringLiteral("/cpu10/")).size() == 1);
}

void TestCpuTargets::theKernelRootsAreWhereWeThink()
{
    // The walk goes up two levels from the configured path, then down through
    // policyN or cpuN. Pinning that here means a wrong level is caught by the test
    // rather than by a daemon that silently writes only cpu0.
    plant({ QStringLiteral("cpu/cpu0/cpufreq/scaling_governor"),
            QStringLiteral("cpu/cpu1/cpufreq/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/cpu0/cpufreq/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));

    QCOMPARE(got.size(), 2);
    QVERIFY(got[0].startsWith(m_root + QStringLiteral("/cpu/cpu")));
}

void TestCpuTargets::onlyNumericSuffixesMatch()
{
    // "cpu" and "policy" are prefixes, not the whole name: a directory called
    // cpufreq or policyX must not be treated as a node.
    plant({ QStringLiteral("cpu/cpu0/cpufreq/scaling_governor"),
            QStringLiteral("cpu/cpuX/cpufreq/scaling_governor"),
            QStringLiteral("cpu/cpufreq/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/cpu0/cpufreq/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));

    QCOMPARE(got.size(), 1);
    QVERIFY(got[0].contains(QStringLiteral("/cpu0/")));
}

void TestCpuTargets::anUnrecognisedShapeFallsBackToTheConfiguredPath()
{
    // Neither shape present: the user still gets the single write they asked for.
    plant({ QStringLiteral("cpu/somewhere/else/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/somewhere/else/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));

    QCOMPARE(got.size(), 1);
    QCOMPARE(got[0], configured);
}

void TestCpuTargets::anUnrelatedFieldNameFallsBackToTheConfiguredPath()
{
    plant({ QStringLiteral("cpu/cpu0/cpufreq/scaling_governor") });

    const QString configured = m_root + QStringLiteral("/cpu/cpu0/cpufreq/scaling_governor");
    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "energy_performance_preference"));

    QCOMPARE(got.size(), 1);
    QCOMPARE(got[0], configured);
}

void TestCpuTargets::theShippedConfigPathResolvesOnThisMachine()
{
    // The template names /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor. On a
    // machine that exposes cpufreq, that must resolve to a real set of nodes rather
    // than just itself.
    const QString configured = QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (!QFile::exists(configured))
        QSKIP("no cpufreq tree on this machine");

    const QStringList got = asList(cpuTargetsFor(configured.toStdString(), "scaling_governor"));
    QVERIFY2(got.size() > 1, "expected one target per CPU, not just the configured path");
    for (const QString& t : got) {
        QVERIFY2(t.endsWith(QStringLiteral("/cpufreq/scaling_governor")), qPrintable(t));
        QVERIFY2(QFile::exists(t), qPrintable(t));
    }
}

void TestCpuTargets::namesTheAttributeThatExplainsARejection()
{
    QCOMPARE(availableValuesFileName("scaling_governor"),
             QStringLiteral("scaling_available_governors"));
    QCOMPARE(availableValuesFileName("energy_performance_preference"),
             QStringLiteral("energy_performance_available_preferences"));
    QVERIFY(availableValuesFileName("something_else").empty());
}

QTEST_MAIN(TestCpuTargets)
#include "tst_cpu_targets.moc"
