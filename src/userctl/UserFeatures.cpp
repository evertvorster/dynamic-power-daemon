// File: src/userctl/UserFeatures.cpp
#include "UserFeatures.h"

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

#include <yaml-cpp/yaml.h>

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
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + "/dynamic_power/config.yaml";
}

QString UserFeaturesWidget::cycle3(const QString& cur) {
    if (cur.compare("Unchanged", Qt::CaseInsensitive) == 0) return "Min";
    if (cur.compare("Min", Qt::CaseInsensitive) == 0)       return "Max";
    return "Unchanged";
}

QString UserFeaturesWidget::normalizePolicy(const QString& s) {
    const QString t = s.trimmed().toLower();
    if (t == "min" || t == "max") return t;
    return "unchanged";
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

    if (root && root["features"] && root["features"]["user"] && root["features"]["user"]["screen_refresh"]) {
        YAML::Node sr = root["features"]["user"]["screen_refresh"];
        if (sr["enabled"])  enabled = sr["enabled"].as<bool>();
        if (sr["ac"])       ac = QString::fromStdString(sr["ac"].as<std::string>());
        if (sr["battery"])  bat = QString::fromStdString(sr["battery"].as<std::string>());
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

    YAML::Node features = root["features"];
    if (!features || !features.IsMap()) features = YAML::Node(YAML::NodeType::Map);

    YAML::Node user = features["user"];
    if (!user || !user.IsMap()) user = YAML::Node(YAML::NodeType::Map);

    YAML::Node sr;
    sr["enabled"] = m_screenEnabled->isChecked();
    sr["ac"]      = normalizePolicy(m_acBtn->text()).toStdString();
    sr["battery"] = normalizePolicy(m_batBtn->text()).toStdString();

    user["screen_refresh"] = sr;
    features["user"] = user;
    root["features"] = features;

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
    const QStringList list = detectDisplayRates();
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
