
#pragma once
#include <QString>
#include <QVector>
#include <utility>

struct ProcessRule {
    QString name;
    QString process_name;
    QString active_profile; // "dynamic", "inhibit powersave", "performance", "balanced", "powersave"
    int priority = 0;
};

class Config {
public:
    Config();
    void ensureExists();
    bool load();
    bool save() const;

    std::pair<double,double> thresholds() const { return m_thresholds; }
    void setThresholds(double low, double high) { m_thresholds = {low, high}; }

    const QVector<ProcessRule>& processRules() const { return m_rules; }
    void setProcessRules(QVector<ProcessRule> rules) { m_rules = std::move(rules); }

    QString configPath() const { return m_path; }

private:
    QString m_path;
    std::pair<double,double> m_thresholds {1.0, 2.0};
    QVector<ProcessRule> m_rules;
};
