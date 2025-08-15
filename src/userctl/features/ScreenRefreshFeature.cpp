// File: src/userctl/features/ScreenRefreshFeature.cpp
#include "ScreenRefreshFeature.h"
#include "FeatureBase.h"

#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QDebug>
#include <algorithm>

#include <yaml-cpp/yaml.h>

// logging category used in original file; guard if missing
#ifndef dpUser
#  define dpUser QtMsgType::QtInfoMsg
#endif

namespace dp::features {
    using K  = dp::features::Keys;

using FB = dp::features::FeatureBase;

ScreenRefreshFeature::State ScreenRefreshFeature::readState() const {
    ScreenRefreshFeature::State st;
    YAML::Node root;
    try { root = YAML::LoadFile(FB::configPath().toStdString()); }
    catch (...) { root = YAML::Node(YAML::NodeType::Map); }

    if (root && root[K::features] && root[K::features][K::user]
        && root[K::features][K::user][K::screen_refresh]) {
        auto sr = root[K::features][K::user][K::screen_refresh];
        if (sr[K::enabled])  st.enabled = sr[K::enabled].as<bool>();
        if (sr[K::ac])       st.ac  = QString::fromStdString(sr[K::ac].as<std::string>());
        if (sr[K::battery])  st.battery = QString::fromStdString(sr[K::battery].as<std::string>());
    }
    return st;
}

bool ScreenRefreshFeature::writeState(const State& s) const {
    QFileInfo fi(FB::configPath());
    QDir().mkpath(fi.dir().absolutePath());

    YAML::Node root;
    try { root = YAML::LoadFile(FB::configPath().toStdString()); }
    catch (...) { root = YAML::Node(YAML::NodeType::Map); }

    YAML::Node features = root[K::features];
    if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);

    YAML::Node user = features[K::user];
    if (!user || !user.IsMap()) user = YAML::Node(YAML::NodeType::Map);

    YAML::Node sr = user[K::screen_refresh];
    if (!sr || !sr.IsMap()) sr = YAML::Node(YAML::NodeType::Map);

    sr[K::enabled] = s.enabled;
    sr[K::ac]      = normalizePolicy(s.ac).toStdString();
    sr[K::battery] = normalizePolicy(s.battery).toStdString();

    user[K::screen_refresh] = sr;
    features[K::user] = user;
    root[K::features] = features;

    YAML::Emitter out;
    out << root;
    QByteArray data(out.c_str());

    QFile f(FB::configPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(data);
    f.close();
    return true;
}

QString ScreenRefreshFeature::statusText() const {
    const auto list = probeCurrentRefreshStrings();
    if (list.isEmpty()) return QStringLiteral("Screen Refresh Rate — Current: (unavailable)");
    return QStringLiteral("Screen Refresh Rate — Current: %1").arg(list.join(QStringLiteral("; ")));
}

void ScreenRefreshFeature::applyForPowerState(bool onBattery) const {
    const auto st = readState();
    const QString policy = normalizePolicy(onBattery ? st.battery : st.ac); // "min"|"max"|"unchanged"
    if (st.enabled && policy != QStringLiteral("unchanged")) {
        const auto outs = readOutputs();
        for (const auto& o : outs) {
            const QString modeId = selectSameResModeId(o, policy);
            if (!modeId.isEmpty()) {
                qInfo() << "output" << o.id
                        << "policy" << policy
                        << "switching to mode" << modeId
                        << "(current w×h:" << o.w << "x" << o.h << ")";
                applyMode(o.id, modeId);
            }
        }
    } else {
        qInfo() << "screen_refresh disabled or policy unchanged (" << policy << ")";
    }
}

void ScreenRefreshFeature::refreshStatus() const {
    (void)probeCurrentRefreshStrings(); // no-op except verification
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers (ported from UserFeatures.cpp anon namespace)
// ─────────────────────────────────────────────────────────────────────────────

QList<ScreenRefreshFeature::Output> ScreenRefreshFeature::readOutputs() {
    QList<Output> outs;
    QProcess p; p.start(QStringLiteral("kscreen-doctor"), {QStringLiteral("-j")});
    if (!p.waitForStarted(2000) || !p.waitForFinished(3000) ||
        p.exitStatus()!=QProcess::NormalExit || p.exitCode()!=0) return outs;

    const QJsonDocument doc = QJsonDocument::fromJson(p.readAllStandardOutput());
    if (!doc.isObject()) return outs;

    const auto arr = doc.object().value(QStringLiteral("outputs")).toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        if (!o.value(QStringLiteral("connected")).toBool() || !o.value(QStringLiteral("enabled")).toBool()) continue;

        Output out;
        const QJsonValue idVal = o.value(QStringLiteral("id"));
        if (idVal.isDouble()) out.id = QString::number(idVal.toInt());
        else                  out.id = idVal.toString();

        out.name = o.value(QStringLiteral("name")).toString(out.id);
        out.currentModeId = o.value(QStringLiteral("currentModeId")).toString();

        // Current resolution (width × height)
        int curW=0, curH=0;
        const auto modes = o.value(QStringLiteral("modes")).toArray();
        if (!out.currentModeId.isEmpty()) {
            for (const auto& mv : modes) {
                const auto m = mv.toObject();
                if (m.value(QStringLiteral("id")).toString()==out.currentModeId) {
                    const auto sz = m.value(QStringLiteral("size")).toObject();
                    curW = sz.value(QStringLiteral("width")).toInt();
                    curH = sz.value(QStringLiteral("height")).toInt();
                    break;
                }
            }
        } else {
            const auto cm = o.value(QStringLiteral("currentMode")).toObject();
            const auto sz = cm.value(QStringLiteral("size")).toObject();
            curW = sz.value(QStringLiteral("width")).toInt();
            curH = sz.value(QStringLiteral("height")).toInt();
            if (out.currentModeId.isEmpty()) out.currentModeId = cm.value(QStringLiteral("id")).toString();
        }
        out.w = curW; out.h = curH;

        for (const auto& mv : modes) {
            const auto m = mv.toObject();
            Mode md;
            md.id = m.value(QStringLiteral("id")).toString();
            const auto sz = m.value(QStringLiteral("size")).toObject();
            md.w = sz.value(QStringLiteral("width")).toInt();
            md.h = sz.value(QStringLiteral("height")).toInt();
            md.hz = m.value(QStringLiteral("refreshRate")).toDouble();
            out.modes.push_back(md);
        }
        outs.push_back(out);
    }
    return outs;
}

// Pick min/max ONLY among modes with SAME resolution as current
QString ScreenRefreshFeature::selectSameResModeId(const Output& o, const QString& policyLower) {
    if (policyLower == QStringLiteral("unchanged")) return QString();
    QList<Mode> same;
    for (const auto& m : o.modes) if (m.w==o.w && m.h==o.h) same.push_back(m);
    if (same.isEmpty()) return QString();
    std::sort(same.begin(), same.end(), [](const Mode& a, const Mode& b){ return a.hz < b.hz; });
    const Mode& target = (policyLower==QStringLiteral("min")) ? same.front() : same.back();
    if (!o.currentModeId.isEmpty() && target.id == o.currentModeId) return QString(); // no-op
    return target.id;
}

bool ScreenRefreshFeature::applyMode(const QString& outId, const QString& modeId) {
    if (outId.isEmpty() || modeId.isEmpty()) return false;
    QProcess p;
    p.start(QStringLiteral("kscreen-doctor"), { QStringLiteral("output.%1.mode.%2").arg(outId, modeId) });
    if (!p.waitForStarted(2000)) return false;
    if (!p.waitForFinished(5000)) return false;
    return p.exitStatus()==QProcess::NormalExit && p.exitCode()==0;
}

// Non-UI probe for verification
QStringList ScreenRefreshFeature::probeCurrentRefreshStrings() {
    QStringList out;
    QProcess p; p.start(QStringLiteral("kscreen-doctor"), {QStringLiteral("-j")});
    if (!p.waitForStarted(2000) || !p.waitForFinished(3000) ||
        p.exitStatus()!=QProcess::NormalExit || p.exitCode()!=0) return out;

    const QJsonDocument doc = QJsonDocument::fromJson(p.readAllStandardOutput());
    if (!doc.isObject()) return out;

    const auto arr = doc.object().value(QStringLiteral("outputs")).toArray();
    for (const auto& v : arr) {
        const auto o = v.toObject();
        if (!o.value(QStringLiteral("connected")).toBool() || !o.value(QStringLiteral("enabled")).toBool()) continue;

        const QString name = o.value(QStringLiteral("name")).toString(
            o.value(QStringLiteral("id")).isDouble() ? QString::number(o.value(QStringLiteral("id")).toInt())
                                                     : o.value(QStringLiteral("id")).toString());
        double hz = 0.0;
        const QString curId = o.value(QStringLiteral("currentModeId")).toString();
        const auto modes = o.value(QStringLiteral("modes")).toArray();
        if (!curId.isEmpty()) {
            for (const auto& mv : modes) {
                const auto m = mv.toObject();
                if (m.value(QStringLiteral("id")).toString()==curId) { hz = m.value(QStringLiteral("refreshRate")).toDouble(); break; }
            }
        }
        if (hz <= 0.0) hz = o.value(QStringLiteral("currentMode")).toObject().value(QStringLiteral("refreshRate")).toDouble();
        if (hz > 0.0) out << QStringLiteral("%1 %2 Hz").arg(name).arg(hz, 0, 'f', 1);
    }
    return out;
}

QString ScreenRefreshFeature::normalizePolicy(const QString& s) {
    return FeatureBase::normalizePolicy(s);
}

} // namespace dp::features
