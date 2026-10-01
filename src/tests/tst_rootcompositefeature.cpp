// Tests for src/userctl/features/RootCompositeFeature.cpp - the read/serialize side
// of the same /etc/dynamic_power.yaml tree the daemon parses.
//
// Two properties are worth pinning down. The first is the round trip: what the
// settings dialog writes must read back as what it wrote. The second is that
// writing touches only features.root - the file also holds the load thresholds,
// the hardware mappings and the profile table, and a save must not disturb them.

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QString>

#include "userctl/features/RootCompositeFeature.h"

using dp::features::RootCompositeFeature;

class TestRootCompositeFeature : public QObject {
    Q_OBJECT

private slots:
    void roundTripsThroughAFile();
    void readsTheLegacyFlatList();
    void writingLeavesTheOtherSectionsAlone();
    void writingReplacesTheLegacyListWithNodes();
    void anEmptyNodeListReadsBackEmpty();

private:
    QString write(const QByteArray& body);
    QTemporaryDir m_dir;
    int m_seq = 0;
};

QString TestRootCompositeFeature::write(const QByteArray& body)
{
    const QString path = m_dir.filePath(QStringLiteral("etc-%1.yaml").arg(++m_seq));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(body);
    f.close();
    return path;
}

static RootCompositeFeature::Node makeNode(const QString& id, const QString& path,
                                           bool enabled, const QString& ac,
                                           const QString& battery)
{
    RootCompositeFeature::Node n;
    n.id = id;
    n.absPath = path;
    n.nodeClass = QStringLiteral("device");
    n.label = path;
    n.enabled = enabled;
    n.acValue = ac;
    n.batteryValue = battery;
    n.policyScope = QStringLiteral("override");
    return n;
}

void TestRootCompositeFeature::roundTripsThroughAFile()
{
    const QString path = write("features:\n  root:\n    nodes: []\n");

    RootCompositeFeature::State out;
    out.disclaimerAccepted = true;
    out.acceptedAt = QStringLiteral("2026-10-01 09:49:17 UTC");
    out.nodes.push_back(makeNode(QStringLiteral("node:/sys/a/power/control"),
                                 QStringLiteral("/sys/a/power/control"), true,
                                 QStringLiteral("on"), QStringLiteral("auto")));
    out.nodes.push_back(makeNode(QStringLiteral("node:/sys/b/power/control"),
                                 QStringLiteral("/sys/b/power/control"), false,
                                 QStringLiteral("enabled"), QStringLiteral("disabled")));

    RootCompositeFeature writer(path);
    QVERIFY(writer.write(out));

    RootCompositeFeature reader(path);
    const auto back = reader.read();

    QVERIFY(back.disclaimerAccepted);
    QCOMPARE(back.acceptedAt, QStringLiteral("2026-10-01 09:49:17 UTC"));
    QCOMPARE(back.nodes.size(), out.nodes.size());
    for (int i = 0; i < out.nodes.size(); ++i) {
        QCOMPARE(back.nodes[i].id, out.nodes[i].id);
        QCOMPARE(back.nodes[i].absPath, out.nodes[i].absPath);
        QCOMPARE(back.nodes[i].enabled, out.nodes[i].enabled);
        QCOMPARE(back.nodes[i].acValue, out.nodes[i].acValue);
        QCOMPARE(back.nodes[i].batteryValue, out.nodes[i].batteryValue);
        QCOMPARE(back.nodes[i].policyScope, out.nodes[i].policyScope);
    }
}

void TestRootCompositeFeature::readsTheLegacyFlatList()
{
    // COMPAT(legacy-root-features): still read so an older config keeps working.
    const QString path = write("features:\n"
                               "  root:\n"
                               "    disclaimer:\n"
                               "      accepted: true\n"
                               "    features:\n"
                               "      - enabled: true\n"
                               "        path: /proc/sys/kernel/nmi_watchdog\n"
                               "        ac_value: 1\n"
                               "        battery_value: 0\n");

    RootCompositeFeature reader(path);
    const auto st = reader.read();

    QVERIFY(st.disclaimerAccepted);
    QCOMPARE(st.nodes.size(), 1);
    QCOMPARE(st.nodes[0].absPath, QStringLiteral("/proc/sys/kernel/nmi_watchdog"));
    // The flat list has no id, so one is derived from the path.
    QCOMPARE(st.nodes[0].id, QStringLiteral("node:/proc/sys/kernel/nmi_watchdog"));
    QVERIFY(st.nodes[0].enabled);
    QCOMPARE(st.nodes[0].acValue, QStringLiteral("1"));
    QCOMPARE(st.nodes[0].batteryValue, QStringLiteral("0"));
}

void TestRootCompositeFeature::writingLeavesTheOtherSectionsAlone()
{
    // The file carries far more than the root features, and a save round-trips the
    // whole document. Anything outside features.root must survive untouched.
    const QString path = write("thresholds:\n"
                               "  low: 1\n"
                               "  high: 2\n"
                               "grace_period: 15\n"
                               "profiles:\n"
                               "  powersave:\n"
                               "    cpu_governor: powersave\n"
                               "features:\n"
                               "  root:\n"
                               "    nodes: []\n");

    RootCompositeFeature::State out;
    out.nodes.push_back(makeNode(QStringLiteral("node:/sys/a/power/control"),
                                 QStringLiteral("/sys/a/power/control"), true,
                                 QStringLiteral("on"), QStringLiteral("auto")));
    RootCompositeFeature writer(path);
    QVERIFY(writer.write(out));

    const QString after = QString::fromUtf8([&] {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return f.readAll();
    }());

    QVERIFY2(after.contains(QStringLiteral("grace_period: 15")), qPrintable(after));
    QVERIFY2(after.contains(QStringLiteral("low: 1")), qPrintable(after));
    QVERIFY2(after.contains(QStringLiteral("cpu_governor: powersave")), qPrintable(after));
    QVERIFY2(after.contains(QStringLiteral("/sys/a/power/control")), qPrintable(after));
}

void TestRootCompositeFeature::writingReplacesTheLegacyListWithNodes()
{
    // Serialising drops the old key, so one save converts a file to the current
    // format. This is what lets the compat branch eventually be deleted.
    const QString path = write("features:\n"
                               "  root:\n"
                               "    features:\n"
                               "      - enabled: false\n"
                               "        path: /old\n");

    RootCompositeFeature::State out;
    out.nodes.push_back(makeNode(QStringLiteral("node:/new"), QStringLiteral("/new"),
                                 false, QStringLiteral("auto"), QStringLiteral("auto")));
    RootCompositeFeature writer(path);
    QVERIFY(writer.write(out));

    const QString after = QString::fromUtf8([&] {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        return f.readAll();
    }());

    QVERIFY2(after.contains(QStringLiteral("nodes:")), qPrintable(after));
    QVERIFY2(!after.contains(QStringLiteral("/old")), qPrintable(after));
}

void TestRootCompositeFeature::anEmptyNodeListReadsBackEmpty()
{
    const QString path = write("features:\n  root:\n    nodes: []\n");
    RootCompositeFeature reader(path);
    QVERIFY(reader.read().nodes.isEmpty());
}

QTEST_MAIN(TestRootCompositeFeature)
#include "tst_rootcompositefeature.moc"
