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
#include <QRadioButton>
#include <QToolButton>
#include <QSignalSpy>
#include <QMenu>
#include <QAction>
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
  balanced:
    epp_profile: alpha
)";

} // namespace

class TestProfileConfigDialog : public QObject {
    Q_OBJECT

private slots:
    void init();
    void declaredModesSurviveOpeningTheDialog();
    void theAcceptedListStillFollowsTheMachine();
    void everyRowStartsGreyed();
    void selectingAProfileAsksForThatModeAndUngreysOnlyThatRow();
    void onlyTheSelectedProfilesValuesAreMarked();
    void selectingAProfileOffersTheMachinesValuesToo();

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
    o.write("alpha\ndelta\n");
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
    QCOMPARE(cap.accepted, QStringList({ QStringLiteral("alpha"), QStringLiteral("delta"),
                                         QStringLiteral("disabled") }));
    QVERIFY2(cap.accepted != cap.modes, "the two lists must be distinguishable");
}

static QRadioButton* toggleFor(ProfileConfigDialog& dlg, const QString& profile)
{
    for (QRadioButton* r : dlg.findChildren<QRadioButton*>())
        if (r->text() == profile) return r;
    return nullptr;
}

void TestProfileConfigDialog::everyRowStartsGreyed()
{
    // Nothing is selected to begin with, which means "let the daemon decide". The rows
    // are greyed rather than hidden so the whole table is still readable.
    ProfileConfigDialog dlg(nullptr, m_configPath);

    for (const auto& profile : dlg.m_profiles) {
        const auto btns = dlg.m_buttons.value(profile);
        QVERIFY2(!btns.isEmpty(), qPrintable(profile));
        for (QToolButton* b : btns)
            QVERIFY2(!b->isEnabled(), qPrintable(profile + QStringLiteral(" should start greyed")));
    }
}

void TestProfileConfigDialog::selectingAProfileAsksForThatModeAndUngreysOnlyThatRow()
{
    ProfileConfigDialog dlg(nullptr, m_configPath);
    QSignalSpy spy(&dlg, &ProfileConfigDialog::modeRequested);

    QRadioButton* balanced = toggleFor(dlg, QStringLiteral("balanced"));
    QVERIFY(balanced);
    balanced->setChecked(true);

    // The machine is asked for that profile - which is what makes the option lists
    // truthful - and only that row becomes editable.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("Balanced"));

    for (QToolButton* b : dlg.m_buttons.value(QStringLiteral("balanced")))
        QVERIFY2(b->isEnabled(), "the selected row must be editable");
    for (const auto& other : { QStringLiteral("powersave"), QStringLiteral("performance") }) {
        for (QToolButton* b : dlg.m_buttons.value(other))
            QVERIFY2(!b->isEnabled(), qPrintable(other + QStringLiteral(" must stay greyed")));
    }
}

static QString eppButtonStyle(ProfileConfigDialog& dlg, const QString& profile)
{
    QToolButton* b = dlg.m_buttons.value(profile).value(QStringLiteral("epp_profile"));
    return b ? b->styleSheet() : QStringLiteral("<missing>");
}

void TestProfileConfigDialog::onlyTheSelectedProfilesValuesAreMarked()
{
    // The accepted set is read from the mode that is running, so it can only speak for
    // the profile the machine is actually in. Marking the others would be guessing.
    ProfileConfigDialog dlg(nullptr, m_configPath);

    toggleFor(dlg, QStringLiteral("balanced"))->setChecked(true);
    // balanced asks for alpha, which this machine accepts -> nothing marked
    QVERIFY2(eppButtonStyle(dlg, QStringLiteral("balanced")).isEmpty(), "alpha is accepted");
    // powersave asks for beta, which it does not - but powersave is not selected, so it
    // is not judged either
    QVERIFY2(eppButtonStyle(dlg, QStringLiteral("powersave")).isEmpty(),
             "an unselected profile cannot be judged");

    toggleFor(dlg, QStringLiteral("powersave"))->setChecked(true);
    QVERIFY2(!eppButtonStyle(dlg, QStringLiteral("powersave")).isEmpty(),
             "beta is refused by the running mode and must be marked");
    QVERIFY2(eppButtonStyle(dlg, QStringLiteral("balanced")).isEmpty(),
             "balanced is no longer selected, so its marking must be cleared");
}

void TestProfileConfigDialog::selectingAProfileOffersTheMachinesValuesToo()
{
    // Evert's case: a config whose declared EPP list had been narrowed by an earlier
    // governor cannot offer the value a profile needs. Selecting the profile re-reads the
    // machine, and the menu offers those values alongside the declared ones.
    ProfileConfigDialog dlg(nullptr, m_configPath);
    toggleFor(dlg, QStringLiteral("balanced"))->setChecked(true);

    QToolButton* btn = dlg.m_buttons.value(QStringLiteral("balanced"))
                          .value(QStringLiteral("epp_profile"));
    QVERIFY(btn);
    QMenu* menu = btn->menu();
    QVERIFY(menu);

    QStringList items;
    bool sawSeparator = false;
    for (QAction* a : menu->actions()) {
        if (a->isSeparator()) { sawSeparator = true; continue; }
        items << a->text();
    }

    // "delta" is accepted by the machine but not declared in the config - the whole point.
    QVERIFY2(items.contains(QStringLiteral("delta")), qPrintable(items.join(", ")));
    // "beta" is declared but refused; it must still be reachable, below the separator.
    QVERIFY2(items.contains(QStringLiteral("beta")), qPrintable(items.join(", ")));
    QVERIFY2(sawSeparator, "accepted and merely-declared values must be separated");
}

QTEST_MAIN(TestProfileConfigDialog)
#include "tst_profileconfigdialog.moc"
