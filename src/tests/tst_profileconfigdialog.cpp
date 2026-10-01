// Tests for ProfileConfigDialog's capability lists.
//
// There are two lists and they were previously one. `modes` is what the config
// declares the knob may be set to — the intent, and what save writes back. `accepted`
// is what the machine takes right now, read from options_path.
//
// They were conflated: opening the dialog copied the machine's current list over the
// declared one, and saving then persisted it. With the governor on `performance` that
// narrowed the config's EPP list to `performance` alone, permanently, even though the
// governor would later allow the rest — which is how a shipped list of six values
// became a list of one on a real machine.
//
// Built with -fno-access-control because the lists are private state.

#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QString>

#include "userctl/ProfileConfigDialog.h"

namespace {

// A config declaring an EPP list that is deliberately different from what the
// "machine" reports, so the two cannot be confused.
const char* kConfig = R"(thresholds:
  low: 1
  high: 2
grace_period: 15
hardware:
  epp_profile:
    path: /sys/not/real/energy_performance_preference
    options_path: OPTIONS_PATH
    modes:
      - alpha
      - beta
      - disabled
profiles:
  powersave:
    epp_profile: beta
)";

} // namespace

class TestProfileConfigDialog : public QObject {
    Q_OBJECT

private slots:
    void init();
    void declaredModesSurviveOpeningTheDialog();
    void theAcceptedListStillFollowsTheMachine();

private:
    QTemporaryDir m_dir;
    QString m_configPath;
};

void TestProfileConfigDialog::init()
{
    QVERIFY(m_dir.isValid());

    const QString optionsPath = m_dir.filePath(QStringLiteral("available"));
    QFile o(optionsPath);
    QVERIFY(o.open(QIODevice::WriteOnly | QIODevice::Truncate));
    o.write("gamma\ndelta\n");
    o.close();

    QString cfg = QString::fromUtf8(kConfig);
    cfg.replace(QStringLiteral("OPTIONS_PATH"), optionsPath);

    m_configPath = m_dir.filePath(QStringLiteral("dynamic_power.yaml"));
    QFile c(m_configPath);
    QVERIFY(c.open(QIODevice::WriteOnly | QIODevice::Truncate));
    c.write(cfg.toUtf8());
    c.close();
}

void TestProfileConfigDialog::declaredModesSurviveOpeningTheDialog()
{
    // The regression: the machine's list must not overwrite what the config declares,
    // because the config's list is what save writes back.
    ProfileConfigDialog dlg(nullptr, m_configPath);

    const CapabilityInfo cap = dlg.m_caps.value(QStringLiteral("epp_profile"));
    QCOMPARE(cap.modes, QStringList({ QStringLiteral("alpha"),
                                      QStringLiteral("beta"),
                                      QStringLiteral("disabled") }));
}

void TestProfileConfigDialog::theAcceptedListStillFollowsTheMachine()
{
    // The other half: the machine's list is still read, so the dialog can show what
    // the running governor actually accepts.
    ProfileConfigDialog dlg(nullptr, m_configPath);

    const CapabilityInfo cap = dlg.m_caps.value(QStringLiteral("epp_profile"));
    // readModesFromFile appends the project's "disabled" sentinel, which means "do not
    // write this knob". Both lists carry it, so they stay comparable.
    QCOMPARE(cap.accepted, QStringList({ QStringLiteral("gamma"), QStringLiteral("delta"),
                                         QStringLiteral("disabled") }));
    QVERIFY2(cap.accepted != cap.modes, "the two lists must be distinguishable");
}

QTEST_MAIN(TestProfileConfigDialog)
#include "tst_profileconfigdialog.moc"
