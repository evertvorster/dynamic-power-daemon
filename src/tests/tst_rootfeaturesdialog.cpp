// Tests for RootFeaturesDialog::stateToSave() - the rule that decides which nodes are
// written back to /etc/dynamic_power.yaml.
//
// This rule has a history: an earlier version dropped every rule that was switched
// off, so saving after disabling something erased it. It also used to keep rules for
// hardware that no longer exists. Both were only found by accident. The rule now is
// that a node is written when it is either enabled or already a rule in the file,
// untouched nodes are left out, and kept nodes are written flat - no parent_id,
// scope forced to override, values materialised - which is what makes the daemon's
// inheritance resolution produce the same result.
//
// stateToSave() is private, so this target is built with -fno-access-control rather
// than including the .cpp with private/public swapped, which breaks libstdc++.
//
// Fixtures use /proc/sys/kernel paths: they exist on any Linux, and the detector
// never emits them, so a node that appears in the output got there because the
// config asked for it rather than because the machine happens to expose it.

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QString>

#include "userctl/RootFeaturesDialog.h"

namespace {

const char* kNmiPath = "/proc/sys/kernel/nmi_watchdog";
const char* kWritebackPath = "/proc/sys/vm/dirty_writeback_centisecs";

QByteArray configWith(const QByteArray& disclaimerAccepted, const QByteArray& nodes)
{
    return QByteArray("features:\n"
                      "  root:\n"
                      "    disclaimer:\n"
                      "      accepted: ") + disclaimerAccepted + "\n"
                      "    nodes:\n" + nodes;
}

QByteArray rule(const char* id, const char* path, bool enabled, const char* ac, const char* battery)
{
    return QByteArray("      - id: ") + id + "\n"
           "        path: " + path + "\n"
           "        enabled: " + (enabled ? "true" : "false") + "\n"
           "        ac_value: \"" + ac + "\"\n"
           "        battery_value: \"" + battery + "\"\n"
           "        policy_scope: override\n";
}

// A rule count that does not depend on whatever hardware this machine has: the
// detector's own nodes are untouched and must never be written.
int countRule(const RootState& s, const char* path)
{
    int n = 0;
    for (const auto& node : s.nodes)
        if (node.absPath == QLatin1String(path))
            ++n;
    return n;
}

} // namespace

class TestRootFeaturesDialog : public QObject {
    Q_OBJECT

private slots:
    void keepsAnEnabledRule();
    void keepsARuleThatIsSwitchedOff();
    void writesNothingForUntouchedDetectedNodes();
    void writesEverythingDisabledWhenTheDisclaimerIsNotAccepted();
    void dropsScaffoldingAndWritesRulesFlat();

private:
    QString write(const QByteArray& body);
    QTemporaryDir m_dir;
    int m_seq = 0;
};

// The dialog defers detection until the event loop runs, so that it can appear before it
// fills itself in. A test therefore has to let that happen.
static void constructLoaded(RootFeaturesDialog& dialog)
{
    QCoreApplication::processEvents();
}

QString TestRootFeaturesDialog::write(const QByteArray& body)
{
    const QString path = m_dir.filePath(QStringLiteral("etc-%1.yaml").arg(++m_seq));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(body);
    f.close();
    return path;
}

void TestRootFeaturesDialog::keepsAnEnabledRule()
{
    const QString path = write(configWith("true", rule("node:/proc/sys/kernel/nmi_watchdog",
                                                        kNmiPath, true, "1", "0")));
    RootFeaturesDialog dialog(nullptr, path);
    constructLoaded(dialog);   // detection is deferred until the event loop runs
    const RootState saved = dialog.stateToSave();

    QCOMPARE(countRule(saved, kNmiPath), 1);
    for (const auto& node : saved.nodes) {
        if (node.absPath != QLatin1String(kNmiPath))
            continue;
        QVERIFY(node.enabled);
        QCOMPARE(node.acValue, QStringLiteral("1"));
        QCOMPARE(node.batteryValue, QStringLiteral("0"));
    }
}

void TestRootFeaturesDialog::keepsARuleThatIsSwitchedOff()
{
    // The regression this pins down: switching a rule off and saving used to delete it.
    const QString path = write(configWith("true", rule("node:/proc/sys/kernel/nmi_watchdog",
                                                        kNmiPath, false, "1", "0")));
    RootFeaturesDialog dialog(nullptr, path);
    constructLoaded(dialog);   // detection is deferred until the event loop runs
    const RootState saved = dialog.stateToSave();

    QCOMPARE(countRule(saved, kNmiPath), 1);
    for (const auto& node : saved.nodes) {
        if (node.absPath != QLatin1String(kNmiPath))
            continue;
        QVERIFY(!node.enabled);
        QCOMPARE(node.acValue, QStringLiteral("1"));
    }
}

void TestRootFeaturesDialog::writesNothingForUntouchedDetectedNodes()
{
    // The machine's own device tree must not leak into the config file. This is the
    // anti-bloat guarantee: the file holds the rules you set, not the tree you saw.
    const QString path = write(configWith("true", rule("node:/proc/sys/kernel/nmi_watchdog",
                                                        kNmiPath, true, "1", "0")));
    RootFeaturesDialog dialog(nullptr, path);
    constructLoaded(dialog);   // detection is deferred until the event loop runs
    const RootState saved = dialog.stateToSave();

    QCOMPARE(saved.nodes.size(), 1);
    QCOMPARE(saved.nodes[0].absPath, QLatin1String(kNmiPath));
}

void TestRootFeaturesDialog::writesEverythingDisabledWhenTheDisclaimerIsNotAccepted()
{
    // Saving is allowed without the disclaimer, but nothing may reach the machine, so
    // every rule is written switched off while the values are kept.
    const QString path = write(configWith("false", rule("node:/proc/sys/kernel/nmi_watchdog",
                                                         kNmiPath, true, "1", "0")
                                        + rule("node:/proc/sys/vm/dirty_writeback_centisecs",
                                               kWritebackPath, true, "1000", "6000")));
    RootFeaturesDialog dialog(nullptr, path);
    constructLoaded(dialog);   // detection is deferred until the event loop runs
    const RootState saved = dialog.stateToSave();

    QCOMPARE(saved.nodes.size(), 2);
    for (const auto& node : saved.nodes) {
        QVERIFY2(!node.enabled, qPrintable(node.absPath));
        QVERIFY(!node.acValue.isEmpty());
        QVERIFY(!node.batteryValue.isEmpty());
    }
}

void TestRootFeaturesDialog::dropsScaffoldingAndWritesRulesFlat()
{
    // A rule nested under a scaffolding node must survive, but come out flat: the
    // daemon resolves inheritance through parent_id, so a rule whose parent was
    // pruned would be silently orphaned and never applied.
    QByteArray nodes;
    nodes += "      - id: group:kernel\n"
             "        enabled: false\n";
    nodes += rule("node:/proc/sys/kernel/nmi_watchdog", kNmiPath, true, "1", "0");
    const QString path = write(configWith("true", nodes));

    RootFeaturesDialog dialog(nullptr, path);
    constructLoaded(dialog);   // detection is deferred until the event loop runs
    const RootState saved = dialog.stateToSave();

    QCOMPARE(countRule(saved, kNmiPath), 1);
    for (const auto& node : saved.nodes) {
        QVERIFY2(!node.absPath.isEmpty(), "scaffolding must not be written");
        QVERIFY2(node.parentId.isEmpty(), qPrintable(node.parentId));
        QCOMPARE(node.policyScope, QStringLiteral("override"));
    }
}

QTEST_MAIN(TestRootFeaturesDialog)
#include "tst_rootfeaturesdialog.moc"
