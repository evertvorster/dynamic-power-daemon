#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "features/RootCompositeFeature.h"

using RootNode = dp::features::RootCompositeFeature::Node;
using RootState = dp::features::RootCompositeFeature::State;

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class UserFeaturesWidget;

// Edits the root-feature rules in /etc/dynamic_power.yaml against the tree the
// kernel actually exposes. Moved out of MainWindow.cpp so it can be built and
// tested on its own; MainWindow only opens it.
class RootFeaturesDialog : public QDialog {
    Q_OBJECT

public:
    explicit RootFeaturesDialog(QWidget* parent, const QString& etcPath = "/etc/dynamic_power.yaml");
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    QString m_etcPath;
    bool m_disclaimerAccepted = false;
    QString m_disclaimerAcceptedAt;
    QPushButton* m_saveBtn{};
    QPushButton* m_confirmBtn{};
    QLineEdit* m_filterEdit{};
    QLabel* m_confirmNote{};
    UserFeaturesWidget* m_userWidget{};
    QPushButton* m_userSaveBtn{};
    QTreeWidget* m_tree{};
    QLabel* m_labelValue{};
    QLabel* m_classValue{};
    QLabel* m_currentValue{};
    QCheckBox* m_overrideCheck{};
    QCheckBox* m_enabledCheck{};
    QPushButton* m_deleteRuleBtn{};
    QComboBox* m_acCombo{};
    QComboBox* m_batCombo{};
    QPushButton* m_addKernelBtn{};
    QPushButton* m_removeKernelBtn{};
    RootState m_state;
    QMap<QString, int> m_indexById;
    QMap<QString, QTreeWidgetItem*> m_treeItems;
    bool m_updatingUi = false;

    // Which inspector field the user touched. A bulk edit must apply only that one: a container
    // row shows an aggregate (typically Enabled=0 with mixed or empty values), so applying all
    // three on any edit disabled the whole subtree and blanked its values.
    enum BulkField { BulkEnabled = 1, BulkAc = 2, BulkBat = 4 };
private slots:
    void refreshLiveState();
    void onDaemonPowerStateChanged();
    bool isVisibleNode(const RootNode& node) const;
    QString filterText() const;
    bool nodeMatchesFilter(const RootNode& node, const QString& needle) const;
    bool shouldShowNode(const RootNode& node, const QString& needle) const;
    bool subtreeHasDirectMatch(const RootNode& node, const QString& needle) const;
    QVector<const RootNode*> visibleEditableDescendants(const QString& parentId, const QString& needle) const;
    void updateConfirmUI();
    void showDisclaimer();
    int indexForId(const QString& id) const;
    RootNode* nodeById(const QString& id);
    const RootNode* nodeById(const QString& id) const;
    RootNode* selectedNode();
    const RootNode* parentNode(const RootNode& node) const;
    bool isKernelNode(const RootNode& node) const;
    QString kernelParentForPath(const QString& path) const;
    QString kernelLabelForPath(const QString& path) const;
    QString nodePathForOrdering(const RootNode& node) const;
    QString addressOf(const RootNode& node) const;
    QStringList detectKernelOptions(const QString& path) const;
    bool validateKernelPath(const QString& path, QString& error) const;
    bool isRealControlLeaf(const RootNode& node) const;
    bool isBulkEditableDeviceNode(const RootNode& node) const;
    QVector<RootNode*> descendantControlLeaves(const QString& parentId);
    QVector<const RootNode*> descendantControlLeavesConst(const QString& parentId) const;
    void applyBulkEditToLeaves(const RootNode& source, int fields, bool enabled,
                               const QString& acValue, const QString& batteryValue);
    bool effectiveEnabled(const RootNode& node) const;
    QString effectiveValue(const RootNode& node, bool battery) const;
    QStringList effectiveOptions(const RootNode& node) const;
    void mergeSavedState(RootState& detected, const RootState& saved);
    void seedDefaults();
    void loadState();
    void connectPowerRefresh();
    bool pkexecWrite(const QByteArray& data, const QString& path);
    void rebuildTree();
    void adjustTreeColumns();
    void refreshCurrentValues();
    Qt::CheckState subtreeCheckState(const RootNode& node) const;
    void refreshTreeState();
    void populateCombo(QComboBox* combo, const QStringList& options, const QString& current);
    void loadInspector(QTreeWidgetItem* current);
    static QString markerText(bool own, bool below);
    bool subtreeHasRule(const RootNode& node) const;
    void refreshRowMarker(const RootNode& node);
    void onDeleteRule();
    void onTreeItemChanged(QTreeWidgetItem* item, int column);
    void onInspectorChanged(int changed);
    void onAddKernelTuning();
    void onRemoveKernelTuning();
    RootState stateToSave() const;
    void onSave();
};
