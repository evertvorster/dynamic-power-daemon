// Tests for the daemon-side config parser, src/config/config.cpp.
//
// The interesting part is resolveRootNodeTree(): persisted nodes carry a parent_id
// and a policy_scope, and a node whose scope is "inherit" takes the parent's
// effective values rather than its own. That resolution decides what the daemon
// actually writes, and until now it had never been exercised directly.
//
// Config::loadSettings() resets its globals on entry, so calling it repeatedly in
// one process is safe.

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QString>

#include "config/config.h"

namespace {

// The parser wants thresholds and grace_period present; it logs an error when they
// are missing, so every fixture carries them.
QByteArray configWith(const QByteArray& featuresBody)
{
    return QByteArray("thresholds:\n"
                      "  low: 1\n"
                      "  high: 2\n"
                      "grace_period: 15\n"
                      "features:\n"
                      "  root:\n"
                      "    disclaimer:\n"
                      "      accepted: true\n") + featuresBody;
}

} // namespace

class TestConfig : public QObject {
    Q_OBJECT

private slots:
    void aNodeWithAPathBecomesARule();
    void aChildOverridesItsDisabledParent();
    void anInheritingChildTakesTheParentsValues();
    void aChildWhoseParentIsMissingIsDropped();
    void scaffoldingWithoutAPathIsNotARule();
    void theLegacyFlatListStillParses();
    void aConfigWithoutARootSectionYieldsNoRules();

private:
    QString write(const QByteArray& body);
    QTemporaryDir m_dir;
    int m_seq = 0;
};

QString TestConfig::write(const QByteArray& body)
{
    const QString path = m_dir.filePath(QStringLiteral("config-%1.yaml").arg(++m_seq));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(body);
    f.close();
    return path;
}

void TestConfig::aNodeWithAPathBecomesARule()
{
    const QString path = write(configWith("    nodes:\n"
                                          "      - id: node:/sys/example/power/control\n"
                                          "        path: /sys/example/power/control\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"on\"\n"
                                          "        battery_value: \"auto\"\n"));
    Config::loadSettings(path);

    QCOMPARE(rootFeatures.items.size(), size_t(1));
    QVERIFY(rootFeatures.items[0].enabled);
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].path), QStringLiteral("/sys/example/power/control"));
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].ac_value), QStringLiteral("on"));
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].battery_value), QStringLiteral("auto"));
}

void TestConfig::aChildOverridesItsDisabledParent()
{
    const QString path = write(configWith("    nodes:\n"
                                          "      - id: parent\n"
                                          "        path: /sys/parent\n"
                                          "        enabled: false\n"
                                          "        ac_value: \"1\"\n"
                                          "        battery_value: \"0\"\n"
                                          "      - id: child\n"
                                          "        parent_id: parent\n"
                                          "        path: /sys/child\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"10\"\n"
                                          "        battery_value: \"1\"\n"));
    Config::loadSettings(path);

    // Both are emitted - resolveRootNodeTree records every node that has a path,
    // enabled or not, and the daemon filters afterwards. What matters is the flags.
    QCOMPARE(rootFeatures.items.size(), size_t(2));

    bool parentEnabled = true, childEnabled = false;
    for (const auto& item : rootFeatures.items) {
        if (item.path == "/sys/parent") parentEnabled = item.enabled;
        if (item.path == "/sys/child")  childEnabled = item.enabled;
    }
    QVERIFY(!parentEnabled);
    QVERIFY(childEnabled);
}

void TestConfig::anInheritingChildTakesTheParentsValues()
{
    const QString path = write(configWith("    nodes:\n"
                                          "      - id: parent\n"
                                          "        path: /sys/parent\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"10\"\n"
                                          "        battery_value: \"1\"\n"
                                          "        policy_scope: override\n"
                                          "      - id: child\n"
                                          "        parent_id: parent\n"
                                          "        path: /sys/child\n"
                                          "        enabled: false\n"
                                          "        ac_value: \"9999\"\n"
                                          "        policy_scope: inherit\n"));
    Config::loadSettings(path);

    bool found = false;
    for (const auto& item : rootFeatures.items) {
        if (item.path != "/sys/child")
            continue;
        found = true;
        // The child's own enabled and ac_value are ignored in favour of the parent's,
        // including the value the child does not set at all.
        QVERIFY(item.enabled);
        QCOMPARE(QString::fromStdString(item.ac_value), QStringLiteral("10"));
        QCOMPARE(QString::fromStdString(item.battery_value), QStringLiteral("1"));
    }
    QVERIFY(found);
}

void TestConfig::aChildWhoseParentIsMissingIsDropped()
{
    // The save-side prune relies on this: materialised rules are written flat, with
    // no parent_id, precisely so nothing can be orphaned this way.
    const QString path = write(configWith("    nodes:\n"
                                          "      - id: root\n"
                                          "        path: /sys/root\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"1\"\n"
                                          "        battery_value: \"0\"\n"
                                          "      - id: orphan\n"
                                          "        parent_id: no-such-parent\n"
                                          "        path: /sys/orphan\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"1\"\n"
                                          "        battery_value: \"0\"\n"));
    Config::loadSettings(path);

    QCOMPARE(rootFeatures.items.size(), size_t(1));
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].path), QStringLiteral("/sys/root"));
}

void TestConfig::scaffoldingWithoutAPathIsNotARule()
{
    // Tree scaffolding is persisted with an id but no path; it must not become a write.
    const QString path = write(configWith("    nodes:\n"
                                          "      - id: segment:/sys/devices/pci0000:00\n"
                                          "        enabled: false\n"
                                          "      - id: node:/sys/devices/pci0000:00/power/control\n"
                                          "        parent_id: segment:/sys/devices/pci0000:00\n"
                                          "        path: /sys/devices/pci0000:00/power/control\n"
                                          "        enabled: true\n"
                                          "        ac_value: \"on\"\n"
                                          "        battery_value: \"auto\"\n"));
    Config::loadSettings(path);

    QCOMPARE(rootFeatures.items.size(), size_t(1));
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].path),
             QStringLiteral("/sys/devices/pci0000:00/power/control"));
}

void TestConfig::theLegacyFlatListStillParses()
{
    // COMPAT(legacy-root-features): the flat list is still read, so that a config
    // written by an older version keeps working.
    const QString path = write(configWith("    features:\n"
                                          "      - enabled: true\n"
                                          "        path: /proc/sys/kernel/nmi_watchdog\n"
                                          "        ac_value: 1\n"
                                          "        battery_value: 0\n"));
    Config::loadSettings(path);

    QCOMPARE(rootFeatures.items.size(), size_t(1));
    QVERIFY(rootFeatures.items[0].enabled);
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].path),
             QStringLiteral("/proc/sys/kernel/nmi_watchdog"));
    // Numeric scalars arrive through readScalar as text.
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].ac_value), QStringLiteral("1"));
    QCOMPARE(QString::fromStdString(rootFeatures.items[0].battery_value), QStringLiteral("0"));
}

void TestConfig::aConfigWithoutARootSectionYieldsNoRules()
{
    const QString path = write("thresholds:\n"
                               "  low: 1\n"
                               "  high: 2\n"
                               "grace_period: 15\n");
    Config::loadSettings(path);

    QVERIFY(rootFeatures.items.empty());
    QVERIFY(!rootFeatures.disclaimerAccepted);
}

QTEST_MAIN(TestConfig)
#include "tst_config.moc"
