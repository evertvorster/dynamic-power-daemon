// File: src/userctl/UserFeatures.cpp
#include "UserFeatures.h"

#include "features/FeatureBase.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <QDebug>   // optional: qInfo logs while we test
#include <QLoggingCategory>

#include <yaml-cpp/yaml.h>

Q_LOGGING_CATEGORY(dpUser, "dp.userfeatures")

UserFeaturesWidget::UserFeaturesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->setSpacing(6);

    // Row: [checkbox] "Screen Refresh Rate — Current: ..." [AC] [BAT]
    auto* row = new QWidget(this);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0,0,0,0);

    m_screenEnabled = new QCheckBox(row);
    h->addWidget(m_screenEnabled);

    m_status = new QLabel("Screen Refresh Rate — Current: (detecting…)", row);
    h->addWidget(m_status, 1);

    m_acBtn  = new QPushButton("Unchanged", row);
    m_batBtn = new QPushButton("Unchanged", row);
    h->addWidget(m_acBtn);
    h->addWidget(m_batBtn);

    connect(m_acBtn,  &QPushButton::clicked, this, [this]{ m_acBtn->setText(cycle3(m_acBtn->text())); });
    connect(m_batBtn, &QPushButton::clicked, this, [this]{ m_batBtn->setText(cycle3(m_batBtn->text())); });

    outer->addWidget(row);
}

QString UserFeaturesWidget::configPath() {
    return dp::features::FeatureBase::configPath();
}

QString UserFeaturesWidget::cycle3(const QString& cur) {
    if (cur.compare("Unchanged", Qt::CaseInsensitive) == 0) return "Min";
    if (cur.compare("Min", Qt::CaseInsensitive) == 0)       return "Max";
    return "Unchanged";
}

QString UserFeaturesWidget::normalizePolicy(const QString& s) {
    return dp::features::FeatureBase::normalizePolicy(s);
}

void UserFeaturesWidget::load() {
    YAML::Node root;
    try {
        root = YAML::LoadFile(configPath().toStdString());
    } catch (...) {
        root = YAML::Node(YAML::NodeType::Map);
    }

    bool enabled = false;
    QString ac = "unchanged";
    QString bat = "unchanged";

    if (root && root[dp::features::Keys::features] && root[dp::features::Keys::features][dp::features::Keys::user] && root[dp::features::Keys::features][dp::features::Keys::user][dp::features::Keys::screen_refresh]) {
        YAML::Node sr = root[dp::features::Keys::features][dp::features::Keys::user][dp::features::Keys::screen_refresh];
        if (sr[dp::features::Keys::enabled])  enabled = sr[dp::features::Keys::enabled].as<bool>();
        if (sr[dp::features::Keys::ac])       ac = QString::fromStdString(sr[dp::features::Keys::ac].as<std::string>());
        if (sr[dp::features::Keys::battery])  bat = QString::fromStdString(sr[dp::features::Keys::battery].as<std::string>());
    }

    m_screenEnabled->setChecked(enabled);
    auto toTitle = [](QString v){
        v = v.toLower();
        if (v == "min") return QString("Min");
        if (v == "max") return QString("Max");
        return QString("Unchanged");
    };
    m_acBtn->setText(toTitle(ac));
    m_batBtn->setText(toTitle(bat));
}

bool UserFeaturesWidget::save() {
    // Ensure directory exists
    QFileInfo fi(configPath());
    QDir().mkpath(fi.dir().absolutePath());

    YAML::Node root;
    try {
        root = YAML::LoadFile(configPath().toStdString());
    } catch (...) {
        root = YAML::Node(YAML::NodeType::Map);
    }

    YAML::Node features = root[dp::features::Keys::features];
    if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);

    YAML::Node user = features[dp::features::Keys::user];
    if (!user || !user.IsMap()) user = YAML::Node(YAML::NodeType::Map);

    YAML::Node sr;
    sr[dp::features::Keys::enabled] = m_screenEnabled->isChecked();
    sr[dp::features::Keys::ac]      = normalizePolicy(m_acBtn->text()).toStdString();
    sr[dp::features::Keys::battery] = normalizePolicy(m_batBtn->text()).toStdString();

    user[dp::features::Keys::screen_refresh] = sr;
    features[dp::features::Keys::user] = user;
    root[dp::features::Keys::features] = features;

    YAML::Emitter out;
    out << root;
    QByteArray data(out.c_str());

    QFile f(configPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(data);
    f.close();
    return true;
}

void UserFeaturesWidget::refreshLiveStatus() {
    const auto list = detectDisplayRates();
    if (list.isEmpty()) {
        m_status->setText("Screen Refresh Rate — Current: (unavailable)");
    } else {
        m_status->setText(QString("Screen Refresh Rate — Current: %1").arg(list.join("; ")));
    }
}

QStringList UserFeaturesWidget::detectDisplayRates() const {
    QStringList outLines;

    QProcess p;
    p.start("kscreen-doctor", {"-j"});
    if (!p.waitForStarted(2000)) return outLines;
    if (!p.waitForFinished(3000)) return outLines;
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) return outLines;

    const QByteArray bytes = p.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject()) return outLines;

    const QJsonObject obj = doc.object();
    const QJsonArray outputs = obj.value("outputs").toArray();
    for (const QJsonValue& v : outputs) {
        const QJsonObject o = v.toObject();
        if (!o.value("connected").toBool() || !o.value("enabled").toBool()) continue;

        const QString name = o.value("name").toString(o.value("id").toString());
        double rateHz = 0.0;

        const QString curId = o.value("currentModeId").toString();
        const QJsonArray modes = o.value("modes").toArray();
        if (!curId.isEmpty() && !modes.isEmpty()) {
            for (const QJsonValue& mv : modes) {
                const QJsonObject m = mv.toObject();
                if (m.value("id").toString() == curId) {
                    rateHz = m.value("refreshRate").toDouble();
                    break;
                }
            }
        }
        if (rateHz <= 0.0) {
            const QJsonObject cm = o.value("currentMode").toObject();
            rateHz = cm.value("refreshRate").toDouble();
        }

        if (rateHz > 0.0) {
            outLines << QString("%1 %2 Hz").arg(name).arg(rateHz, 0, 'f', 1);
        }
    }
    return outLines;
}
// ─────────────────────────────────────────────────────────────────────────────
// Non-UI static applier on UserFeaturesWidget
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    struct Mode { QString id; int w=0; int h=0; double hz=0.0; };
    struct Output { QString id, name, currentModeId; int w=0, h=0; QList<Mode> modes; };

    static QList<Output> readOutputs() {
        QList<Output> outs;
        QProcess p; p.start("kscreen-doctor", {"-j"});
        if (!p.waitForStarted(2000) || !p.waitForFinished(3000) ||
            p.exitStatus()!=QProcess::NormalExit || p.exitCode()!=0) return outs;

        const QJsonDocument doc = QJsonDocument::fromJson(p.readAllStandardOutput());
        if (!doc.isObject()) return outs;

        const auto arr = doc.object().value("outputs").toArray();
        for (const auto& v : arr) {
            const auto o = v.toObject();
            if (!o.value("connected").toBool() || !o.value("enabled").toBool()) continue;

            Output out;

            // ***** FIX: id can be numeric; coerce to string safely *****
            const QJsonValue idVal = o.value("id");
            if (idVal.isDouble()) out.id = QString::number(idVal.toInt());
            else                   out.id = idVal.toString();

            out.name = o.value("name").toString(out.id);
            out.currentModeId = o.value("currentModeId").toString();

            // Current resolution (width × height)
            int curW=0, curH=0;
            const auto modes = o.value("modes").toArray();
            if (!out.currentModeId.isEmpty()) {
                for (const auto& mv : modes) {
                    const auto m = mv.toObject();
                    if (m.value("id").toString()==out.currentModeId) {
                        const auto sz = m.value("size").toObject();
                        curW = sz.value("width").toInt();
                        curH = sz.value("height").toInt();
                        break;
                    }
                }
            }
            if (!curW || !curH) {
                const auto cm = o.value("currentMode").toObject();
                const auto sz = cm.value("size").toObject();
                curW = sz.value("width").toInt();
                curH = sz.value("height").toInt();
                if (out.currentModeId.isEmpty()) out.currentModeId = cm.value("id").toString();
            }
            out.w = curW; out.h = curH;

            for (const auto& mv : modes) {
                const auto m = mv.toObject();
                Mode md;
                md.id = m.value("id").toString();
                const auto sz = m.value("size").toObject();
                md.w = sz.value("width").toInt();
                md.h = sz.value("height").toInt();
                md.hz = m.value("refreshRate").toDouble();
                out.modes.push_back(md);
            }
            outs.push_back(out);
        }
        return outs;
    }

    // Pick min/max ONLY among modes with SAME resolution as current
    static QString selectSameResModeId(const Output& o, const QString& policyLower) {
        if (policyLower == "unchanged") return QString();
        QList<Mode> same;
        for (const auto& m : o.modes) if (m.w==o.w && m.h==o.h) same.push_back(m);
        if (same.isEmpty()) return QString();
        std::sort(same.begin(), same.end(), [](const Mode& a, const Mode& b){ return a.hz < b.hz; });
        const Mode& target = (policyLower=="min") ? same.front() : same.back();
        if (!o.currentModeId.isEmpty() && target.id == o.currentModeId) return QString(); // no-op
        return target.id;
    }

    static bool applyMode(const QString& outId, const QString& modeId) {
        if (outId.isEmpty() || modeId.isEmpty()) return false;
        QProcess p;
        p.start("kscreen-doctor", { QString("output.%1.mode.%2").arg(outId, modeId) });
        if (!p.waitForStarted(2000)) return false;
        if (!p.waitForFinished(5000)) return false;
        return p.exitStatus()==QProcess::NormalExit && p.exitCode()==0;
    }

    // Non-UI probe for verification (same semantics as detectDisplayRates())
    static QStringList probeCurrentRefreshStrings() {
        QStringList out;
        QProcess p; p.start("kscreen-doctor", {"-j"});
        if (!p.waitForStarted(2000) || !p.waitForFinished(3000) ||
            p.exitStatus()!=QProcess::NormalExit || p.exitCode()!=0) return out;

        const QJsonDocument doc = QJsonDocument::fromJson(p.readAllStandardOutput());
        if (!doc.isObject()) return out;

        const auto outputs = doc.object().value("outputs").toArray();
        for (const auto& v : outputs) {
            const auto o = v.toObject();
            if (!o.value("connected").toBool() || !o.value("enabled").toBool()) continue;

            const QString name = o.value("name").toString(
                o.value("id").isDouble() ? QString::number(o.value("id").toInt())
                                         : o.value("id").toString());
            double hz = 0.0;
            const QString curId = o.value("currentModeId").toString();
            const auto modes = o.value("modes").toArray();
            if (!curId.isEmpty()) {
                for (const auto& mv : modes) {
                    const auto m = mv.toObject();
                    if (m.value("id").toString()==curId) { hz = m.value("refreshRate").toDouble(); break; }
                }
            }
            if (hz <= 0.0) hz = o.value("currentMode").toObject().value("refreshRate").toDouble();
            if (hz > 0.0) out << QString("%1 %2 Hz").arg(name).arg(hz, 0, 'f', 1);
        }
        return out;
    }
} // anon

void UserFeaturesWidget::applyForPowerState(bool onBattery) {
    // Read config
    YAML::Node root;
    try { root = YAML::LoadFile(configPath().toStdString()); }
    catch (...) { root = YAML::Node(YAML::NodeType::Map); }

    // Screen Refresh Rate
    bool enabled = false; QString ac = "unchanged", bat = "unchanged";
    if (root && root[dp::features::Keys::features] && root[dp::features::Keys::features][dp::features::Keys::user] && root[dp::features::Keys::features][dp::features::Keys::user][dp::features::Keys::screen_refresh]) {
        auto sr = root[dp::features::Keys::features][dp::features::Keys::user][dp::features::Keys::screen_refresh];
        if (sr[dp::features::Keys::enabled])  enabled = sr[dp::features::Keys::enabled].as<bool>();
        if (sr[dp::features::Keys::ac])       ac  = QString::fromStdString(sr[dp::features::Keys::ac].as<std::string>());
        if (sr[dp::features::Keys::battery])  bat = QString::fromStdString(sr[dp::features::Keys::battery].as<std::string>());
    }
    const QString policy = normalizePolicy(onBattery ? bat : ac); // "min"|"max"|"unchanged"
    if (enabled && policy != "unchanged") {
        const auto outs = readOutputs();
        for (const auto& o : outs) {
            const QString modeId = selectSameResModeId(o, policy);
            if (!modeId.isEmpty()) {
                qCDebug(dpUser) << "output" << o.id
                                << "policy" << policy
                                << "switching to mode" << modeId
                                << "(current w×h:" << o.w << "x" << o.h << ")";
                applyMode(o.id, modeId);
            }
        }
    } else {
        qCDebug(dpUser) << "screen_refresh disabled or policy unchanged (" << policy << ")";
    }
}

void UserFeaturesWidget::refreshStatusProbe() {
    (void)probeCurrentRefreshStrings(); // verifies current; UI updates via refreshLiveStatus()
}