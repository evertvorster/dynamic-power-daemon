#include "MainWindow.h"
#include "DbusClient.h"
#include "Config.h"
#include "LoadGraphWidget.h"
#include "ProcessRuleEditor.h"
#include "features/RootCompositeFeature.h"
#include "features/RootFeatureDetector.h"
#include "ProfileConfigDialog.h"
#include "UserFeatures.h"

#include <QApplication>
#include <QDBusConnection>
#include <QCheckBox>
#include <QComboBox>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLayoutItem>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QSet>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QRegularExpression>
#include <functional>

namespace {

using RootNode = dp::features::RootCompositeFeature::Node;
using RootState = dp::features::RootCompositeFeature::State;

class RootFeaturesDialog : public QDialog {
    Q_OBJECT
public:
    explicit RootFeaturesDialog(QWidget* parent, const QString& etcPath = "/etc/dynamic_power.yaml")
        : QDialog(parent), m_etcPath(etcPath)
    {
        setWindowTitle("Power Saving Features");
        resize(1120, 720);

        auto* outer = new QVBoxLayout(this);

        auto* userGroup = new QGroupBox("User Power Saving Features", this);
        userGroup->setStyleSheet(
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top center;"
            "  font-size: 15pt;"
            "  font-weight: 600;"
            "  padding: 6px 8px;"
            "}"
        );
        auto* userLay = new QVBoxLayout(userGroup);

        auto* headerRow = new QWidget(userGroup);
        auto* hh = new QHBoxLayout(headerRow);
        hh->setContentsMargins(0, 0, 0, 0);
        hh->addSpacing(24);
        hh->addStretch(1);
        auto* acHeader = new QLabel("On AC", headerRow);
        auto* batHeader = new QLabel("On Battery", headerRow);
        acHeader->setAlignment(Qt::AlignCenter);
        batHeader->setAlignment(Qt::AlignCenter);
        hh->addWidget(acHeader);
        hh->addWidget(batHeader);
        userLay->addWidget(headerRow);

        m_userWidget = new UserFeaturesWidget(userGroup);
        userLay->addWidget(m_userWidget);

        m_userSaveBtn = new QPushButton("Save (User)", userGroup);
        auto* btnRowUser = new QHBoxLayout();
        btnRowUser->addStretch(1);
        btnRowUser->addWidget(m_userSaveBtn);
        userLay->addLayout(btnRowUser);
        connect(m_userSaveBtn, &QPushButton::clicked, this, [this] {
            if (m_userWidget->save()) {
                QMessageBox::information(this, "Saved", "User features saved.");
            } else {
                QMessageBox::critical(this, "Error", "Unable to write user config file.");
            }
        });

        outer->addWidget(userGroup);
        m_userWidget->load();
        m_userWidget->refreshLiveStatus();

        auto* group = new QGroupBox("Root-required features (writes to /etc/dynamic_power.yaml)", this);
        auto* groupLay = new QVBoxLayout(group);

        auto* disclaimBox = new QHBoxLayout();
        m_confirmBtn = new QPushButton(this);
        m_confirmBtn->setToolTip("Read and accept the disclaimer to enable saving.");
        disclaimBox->addWidget(m_confirmBtn, 0);
        m_advancedToggle = new QCheckBox("Advanced View", this);
        m_advancedToggle->setToolTip("Show non-PCI device tree entries and other advanced device nodes.");
        disclaimBox->addWidget(m_advancedToggle, 0);
        m_confirmNote = new QLabel("Saving is blocked until you accept the disclaimer.", this);
        disclaimBox->addWidget(m_confirmNote, 1);
        groupLay->addLayout(disclaimBox);
        connect(m_confirmBtn, &QPushButton::clicked, this, [this] { showDisclaimer(); });
        connect(m_advancedToggle, &QCheckBox::toggled, this, [this](bool) { rebuildTree(); });

        auto* splitter = new QSplitter(Qt::Horizontal, group);
        m_tree = new QTreeWidget(splitter);
        m_tree->setHeaderLabels({"Target", "Current", "Class"});
        m_tree->setAlternatingRowColors(true);
        m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_tree->header()->setStretchLastSection(false);
        m_tree->header()->setMinimumSectionSize(80);

        auto* inspector = new QWidget(splitter);
        auto* inspectorLay = new QVBoxLayout(inspector);

        auto* inspectorTitle = new QLabel("Selected Target", inspector);
        QFont titleFont = inspectorTitle->font();
        titleFont.setBold(true);
        inspectorTitle->setFont(titleFont);
        inspectorLay->addWidget(inspectorTitle);

        m_labelValue = new QLabel("Select a node", inspector);
        m_labelValue->setWordWrap(true);
        inspectorLay->addWidget(m_labelValue);

        m_classValue = new QLabel(inspector);
        inspectorLay->addWidget(m_classValue);

        auto* form = new QFormLayout();
        m_currentValue = new QLabel("-", inspector);
        form->addRow("Current", m_currentValue);
        inspectorLay->addLayout(form);

        m_overrideCheck = new QCheckBox("Override inherited policy", inspector);
        inspectorLay->addWidget(m_overrideCheck);
        m_enabledCheck = new QCheckBox("Enabled", inspector);
        inspectorLay->addWidget(m_enabledCheck);

        auto* acRow = new QHBoxLayout();
        acRow->addWidget(new QLabel("On AC", inspector));
        m_acCombo = new QComboBox(inspector);
        m_acCombo->setEditable(true);
        acRow->addWidget(m_acCombo, 1);
        inspectorLay->addLayout(acRow);

        auto* batRow = new QHBoxLayout();
        batRow->addWidget(new QLabel("On Battery", inspector));
        m_batCombo = new QComboBox(inspector);
        m_batCombo->setEditable(true);
        batRow->addWidget(m_batCombo, 1);
        inspectorLay->addLayout(batRow);

        auto* kernelBtnRow = new QHBoxLayout();
        m_addKernelBtn = new QPushButton("Add Tuning", inspector);
        m_removeKernelBtn = new QPushButton("Remove Tuning", inspector);
        kernelBtnRow->addWidget(m_addKernelBtn);
        kernelBtnRow->addWidget(m_removeKernelBtn);
        inspectorLay->addLayout(kernelBtnRow);
        inspectorLay->addStretch(1);

        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        groupLay->addWidget(splitter, 1);

        auto* btnRow = new QHBoxLayout();
        m_saveBtn = new QPushButton("Save (Root)", group);
        btnRow->addStretch(1);
        btnRow->addWidget(m_saveBtn);
        groupLay->addLayout(btnRow);

        outer->addWidget(group);

        connect(m_tree, &QTreeWidget::currentItemChanged, this,
                [this](QTreeWidgetItem* current, QTreeWidgetItem*) { loadInspector(current); });
        connect(m_tree, &QTreeWidget::itemChanged, this,
                [this](QTreeWidgetItem* item, int column) { onTreeItemChanged(item, column); });
        connect(m_overrideCheck, &QCheckBox::toggled, this, [this](bool) { onInspectorChanged(); });
        connect(m_enabledCheck, &QCheckBox::toggled, this, [this](bool) { onInspectorChanged(); });
        connect(m_acCombo, &QComboBox::currentTextChanged, this, [this](const QString&) { onInspectorChanged(); });
        connect(m_batCombo, &QComboBox::currentTextChanged, this, [this](const QString&) { onInspectorChanged(); });
        connect(m_addKernelBtn, &QPushButton::clicked, this, [this] { onAddKernelTuning(); });
        connect(m_removeKernelBtn, &QPushButton::clicked, this, [this] { onRemoveKernelTuning(); });
        connect(m_saveBtn, &QPushButton::clicked, this, [this] { onSave(); });

        loadState();
        updateConfirmUI();
    }

protected:
    void showEvent(QShowEvent* event) override {
        QDialog::showEvent(event);
        QTimer::singleShot(0, this, [this] { adjustTreeColumns(); });
    }

    void resizeEvent(QResizeEvent* event) override {
        QDialog::resizeEvent(event);
        adjustTreeColumns();
    }

private:
    QString m_etcPath;
    bool m_disclaimerAccepted = false;
    QString m_disclaimerAcceptedAt;
    QPushButton* m_saveBtn{};
    QPushButton* m_confirmBtn{};
    QCheckBox* m_advancedToggle{};
    QLabel* m_confirmNote{};
    UserFeaturesWidget* m_userWidget{};
    QPushButton* m_userSaveBtn{};
    QTreeWidget* m_tree{};
    QLabel* m_labelValue{};
    QLabel* m_classValue{};
    QLabel* m_currentValue{};
    QCheckBox* m_overrideCheck{};
    QCheckBox* m_enabledCheck{};
    QComboBox* m_acCombo{};
    QComboBox* m_batCombo{};
    QPushButton* m_addKernelBtn{};
    QPushButton* m_removeKernelBtn{};
    RootState m_state;
    QMap<QString, int> m_indexById;
    QMap<QString, QTreeWidgetItem*> m_treeItems;
    bool m_updatingUi = false;

private slots:
    void onDaemonPowerStateChanged() {
        refreshCurrentValues();
    }

    bool isPciNode(const RootNode& node) const {
        if (node.nodeClass != QStringLiteral("device")) return true;
        if (node.id.contains("group:legacy")) return true;
        if (node.absPath.contains("/sys/devices/pci")) return true;
        if (node.id.contains("segment:/sys/devices/pci")) return true;
        const RootNode* parent = parentNode(node);
        return parent ? isPciNode(*parent) : false;
    }

    bool isVisibleNode(const RootNode& node) const {
        if (m_advancedToggle && m_advancedToggle->isChecked()) return true;
        if (node.id == QStringLiteral("group:devices")) return true;
        if (node.nodeClass != QStringLiteral("device")) return true;
        return isPciNode(node);
    }

    void updateConfirmUI() {
        m_confirmBtn->setText(m_disclaimerAccepted ? "Confirm ✅" : "Confirm ❌");
        m_confirmNote->setVisible(!m_disclaimerAccepted);
    }

    void showDisclaimer() {
        QMessageBox box(this);
        box.setWindowTitle("Disclaimer");
        box.setText("The settings here write variables YOU specify into files the daemon will write as ROOT.\n\n"
                    "With this great power comes great responsibility. YOUR responsibility.\n\n"
                    "The authors accept NO responsibility for anything bad happening to your system when you set these.");
        box.setInformativeText("Do you agree?");
        auto* agree = box.addButton("Agree", QMessageBox::AcceptRole);
        box.addButton("Disagree", QMessageBox::RejectRole);
        box.setIcon(QMessageBox::Warning);
        box.exec();
        if (box.clickedButton() == agree) {
            m_disclaimerAccepted = true;
            m_disclaimerAcceptedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        } else {
            m_disclaimerAccepted = false;
        }
        updateConfirmUI();
    }

    int indexForId(const QString& id) const {
        return m_indexById.value(id, -1);
    }

    RootNode* nodeById(const QString& id) {
        const int idx = indexForId(id);
        return idx >= 0 ? &m_state.nodes[idx] : nullptr;
    }

    const RootNode* nodeById(const QString& id) const {
        const int idx = indexForId(id);
        return idx >= 0 ? &m_state.nodes[idx] : nullptr;
    }

    RootNode* selectedNode() {
        if (!m_tree || !m_tree->currentItem()) return nullptr;
        return nodeById(m_tree->currentItem()->data(0, Qt::UserRole).toString());
    }

    const RootNode* parentNode(const RootNode& node) const {
        return nodeById(node.parentId);
    }

    bool isKernelNode(const RootNode& node) const {
        return node.nodeClass == QStringLiteral("kernel");
    }

    QString kernelParentForPath(const QString& path) const {
        if (path.startsWith("/proc/sys/vm/")) return QStringLiteral("group:kernel:vm");
        if (path.contains("/snd_hda_intel/")) return QStringLiteral("group:kernel:audio");
        return QStringLiteral("group:kernel:general");
    }

    QString kernelLabelForPath(const QString& path) const {
        return path;
    }

    QStringList detectKernelOptions(const QString& path) const {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QString content = QString::fromUtf8(f.readLine()).trimmed();
        if (content.contains('[') && content.contains(']')) {
            QString normalized = content;
            normalized.replace('\n', ' ');
            QStringList out;
            for (QString token : normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
                token.remove('[');
                token.remove(']');
                if (!token.isEmpty() && !out.contains(token)) out.push_back(token);
            }
            return out;
        }
        if (content == "0" || content == "1") return {"0", "1"};
        if (content.compare("on", Qt::CaseInsensitive) == 0 || content.compare("off", Qt::CaseInsensitive) == 0) {
            return {"on", "off"};
        }
        return content.isEmpty() ? QStringList{} : QStringList{content};
    }

    bool validateKernelPath(const QString& path, QString& error) const {
        if (!(path.startsWith("/proc/sys/") || path.startsWith("/sys/module/"))) {
            error = "Kernel tunings must live under /proc/sys/ or /sys/module/.../parameters/.";
            return false;
        }
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) {
            error = "That path does not exist or is not a file.";
            return false;
        }
        if (path.startsWith("/sys/module/") && !path.contains("/parameters/")) {
            error = "Only module parameters under /sys/module/.../parameters/ are accepted.";
            return false;
        }
        return true;
    }

    bool effectiveEnabled(const RootNode& node) const {
        const RootNode* parent = parentNode(node);
        if (!parent || node.policyScope != QStringLiteral("inherit")) return node.enabled;
        return effectiveEnabled(*parent);
    }

    QString effectiveValue(const RootNode& node, bool battery) const {
        const RootNode* parent = parentNode(node);
        if (!parent || node.policyScope != QStringLiteral("inherit")) {
            return battery ? node.batteryValue : node.acValue;
        }
        return effectiveValue(*parent, battery);
    }

    QStringList effectiveOptions(const RootNode& node) const {
        if (!node.allowedValues.isEmpty()) return node.allowedValues;

        QStringList shared;
        bool seeded = false;
        for (const auto& child : m_state.nodes) {
            if (child.parentId != node.id) continue;
            const QStringList childOptions = effectiveOptions(child);
            if (childOptions.isEmpty()) continue;
            if (!seeded) {
                shared = childOptions;
                seeded = true;
                continue;
            }
            QStringList next;
            for (const QString& value : shared) {
                if (childOptions.contains(value) && !next.contains(value)) next.push_back(value);
            }
            shared = next;
        }
        return shared;
    }

    void mergeSavedState(RootState& detected, const RootState& saved) {
        QMap<QString, int> byId;
        QMap<QString, int> byPath;
        for (int i = 0; i < detected.nodes.size(); ++i) {
            byId.insert(detected.nodes[i].id, i);
            if (!detected.nodes[i].absPath.isEmpty()) byPath.insert(detected.nodes[i].absPath, i);
        }

        for (const auto& savedNode : saved.nodes) {
            int idx = byId.value(savedNode.id, -1);
            if (idx < 0 && !savedNode.absPath.isEmpty()) idx = byPath.value(savedNode.absPath, -1);
            if (idx >= 0) {
                auto& target = detected.nodes[idx];
                target.enabled = savedNode.enabled;
                target.acValue = savedNode.acValue;
                target.batteryValue = savedNode.batteryValue;
                target.policyScope = savedNode.policyScope;
                if (!savedNode.label.isEmpty()) target.label = savedNode.label;
                continue;
            }

            auto legacy = savedNode;
            legacy.detected = false;
            legacy.legacy = true;
            if (legacy.parentId.isEmpty()) {
                legacy.parentId = legacy.nodeClass == QStringLiteral("kernel")
                    ? kernelParentForPath(legacy.absPath)
                    : QStringLiteral("group:legacy");
            }
            detected.nodes.push_back(legacy);
        }
    }

    void seedDefaults() {
        for (auto& node : m_state.nodes) {
            if (node.policyScope.isEmpty()) node.policyScope = node.parentId.isEmpty() ? "override" : "inherit";
        }

        for (auto& node : m_state.nodes) {
            if (node.policyScope == QStringLiteral("inherit")) {
                if (node.acValue.isEmpty()) node.acValue = effectiveValue(node, false);
                if (node.batteryValue.isEmpty()) node.batteryValue = effectiveValue(node, true);
            } else {
                const QStringList options = effectiveOptions(node);
                if (node.acValue.isEmpty()) node.acValue = !node.currentValue.isEmpty() ? node.currentValue : (options.isEmpty() ? QString() : options.first());
                if (node.batteryValue.isEmpty()) node.batteryValue = node.acValue;
            }
        }
    }

    void loadState() {
        dp::features::RootCompositeFeature composite(m_etcPath);
        RootState detected = dp::features::RootFeatureDetector::detect();
        const RootState saved = composite.read();
        detected.disclaimerAccepted = saved.disclaimerAccepted;
        detected.acceptedAt = saved.acceptedAt;
        mergeSavedState(detected, saved);
        m_state = detected;
        m_disclaimerAccepted = m_state.disclaimerAccepted;
        m_disclaimerAcceptedAt = m_state.acceptedAt;
        m_indexById.clear();
        for (int i = 0; i < m_state.nodes.size(); ++i) m_indexById.insert(m_state.nodes[i].id, i);
        seedDefaults();
        rebuildTree();
        connectPowerRefresh();
    }

    void connectPowerRefresh() {
        static bool connected = false;
        if (connected) return;
        connected = QDBusConnection::systemBus().connect(
            "org.dynamic_power.Daemon",
            "/org/dynamic_power/Daemon",
            "org.dynamic_power.Daemon",
            "PowerStateChanged",
            this,
            SLOT(onDaemonPowerStateChanged()));
    }

    bool pkexecWrite(const QByteArray& data, const QString& path) {
        QProcess proc;
        proc.start("pkexec", {"tee", path});
        if (!proc.waitForStarted(10000)) {
            QMessageBox::critical(this, "Error", "Failed to start pkexec. Is polkit installed?");
            return false;
        }
        proc.write(data);
        proc.closeWriteChannel();
        proc.waitForFinished(-1);
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            QMessageBox::critical(this, "Error", QString("Write failed (exit %1).").arg(proc.exitCode()));
            return false;
        }
        return true;
    }

    void rebuildTree() {
        m_updatingUi = true;
        m_tree->clear();
        m_treeItems.clear();

        bool hasLegacyChildren = false;
        for (const auto& node : m_state.nodes) {
            if (node.parentId == QStringLiteral("group:legacy")) {
                hasLegacyChildren = true;
                break;
            }
        }

        for (const auto& node : m_state.nodes) {
            if (node.id == QStringLiteral("group:legacy") && !hasLegacyChildren) continue;
            if (!isVisibleNode(node)) continue;
            auto* item = new QTreeWidgetItem();
            item->setText(0, node.label);
            item->setText(1, node.isGroup ? QString() : (node.currentValue.isEmpty() ? "-" : node.currentValue));
            item->setText(2, node.nodeClass);
            item->setData(0, Qt::UserRole, node.id);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            m_treeItems.insert(node.id, item);
            if (node.parentId.isEmpty()) {
                m_tree->addTopLevelItem(item);
            } else if (m_treeItems.contains(node.parentId)) {
                m_treeItems.value(node.parentId)->addChild(item);
            }
        }

        refreshTreeState();
        adjustTreeColumns();
        m_tree->collapseAll();
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
            m_tree->topLevelItem(i)->setExpanded(true);
        }
        if (m_tree->topLevelItemCount() > 0) m_tree->setCurrentItem(m_tree->topLevelItem(0));
        m_updatingUi = false;
    }

    void adjustTreeColumns() {
        if (!m_tree) return;
        auto* header = m_tree->header();
        header->resizeSection(1, header->sectionSizeHint(1));
        header->resizeSection(2, header->sectionSizeHint(2));
        const int viewportWidth = m_tree->viewport()->width();
        const int otherWidth = header->sectionSize(1) + header->sectionSize(2);
        const int targetWidth = qMax(320, viewportWidth - otherWidth - 8);
        header->resizeSection(0, targetWidth);
    }

    void refreshCurrentValues() {
        for (auto& node : m_state.nodes) {
            if (node.isGroup || node.absPath.isEmpty()) continue;
            QFile f(node.absPath);
            QString current;
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                current = QString::fromUtf8(f.readLine()).trimmed();
            }
            node.currentValue = current;
            if (auto* item = m_treeItems.value(node.id, nullptr)) {
                item->setText(1, current.isEmpty() ? "-" : current);
            }
        }
        if (m_tree && m_tree->currentItem()) loadInspector(m_tree->currentItem());
        adjustTreeColumns();
    }

    Qt::CheckState subtreeCheckState(const RootNode& node) const {
        bool anyEnabled = false;
        bool allEnabled = true;
        bool mixedPolicy = false;

        std::function<void(const RootNode&)> walk = [&](const RootNode& current) {
            if (!current.isGroup) {
                const bool enabled = effectiveEnabled(current);
                anyEnabled |= enabled;
                allEnabled &= enabled;
            }
            for (const auto& child : m_state.nodes) {
                if (child.parentId != current.id) continue;
                if (child.policyScope == QStringLiteral("override")) mixedPolicy = true;
                walk(child);
            }
        };
        walk(node);
        if (!anyEnabled) return Qt::Unchecked;
        if (allEnabled && !mixedPolicy) return Qt::Checked;
        return Qt::PartiallyChecked;
    }

    void refreshTreeState() {
        m_updatingUi = true;
        for (const auto& node : m_state.nodes) {
            if (!m_treeItems.contains(node.id)) continue;
            if (auto* item = m_treeItems.value(node.id, nullptr)) item->setCheckState(0, subtreeCheckState(node));
        }
        m_updatingUi = false;
    }

    void populateCombo(QComboBox* combo, const QStringList& options, const QString& current) {
        combo->blockSignals(true);
        combo->clear();
        QStringList values = options;
        if (!current.isEmpty() && !values.contains(current)) values.push_front(current);
        for (const QString& value : values) combo->addItem(value);
        combo->setCurrentText(current);
        combo->blockSignals(false);
    }

    void loadInspector(QTreeWidgetItem* current) {
        m_updatingUi = true;
        RootNode* node = current ? selectedNode() : nullptr;
        if (!node) {
            m_labelValue->setText("Select a node");
            m_classValue->clear();
            m_currentValue->setText("-");
            m_overrideCheck->setEnabled(false);
            m_enabledCheck->setEnabled(false);
            m_acCombo->setEnabled(false);
            m_batCombo->setEnabled(false);
            m_addKernelBtn->setEnabled(false);
            m_removeKernelBtn->setEnabled(false);
            m_updatingUi = false;
            return;
        }

        const RootNode* parent = parentNode(*node);
        const bool canInherit = parent != nullptr;
        const bool override = !canInherit || node->policyScope != QStringLiteral("inherit");
        const QStringList options = effectiveOptions(*node);

        m_labelValue->setText(node->label);
        m_classValue->setText(QString("Class: %1").arg(node->nodeClass));
        m_currentValue->setText(node->currentValue.isEmpty() ? "-" : node->currentValue);
        m_overrideCheck->setEnabled(canInherit);
        m_overrideCheck->setChecked(override);
        m_enabledCheck->setEnabled(override || !canInherit);
        m_enabledCheck->setChecked(node->enabled);
        populateCombo(m_acCombo, options, override ? node->acValue : effectiveValue(*node, false));
        populateCombo(m_batCombo, options, override ? node->batteryValue : effectiveValue(*node, true));
        m_acCombo->setEnabled(override || !canInherit);
        m_batCombo->setEnabled(override || !canInherit);
        const bool kernelGroup = node->id.startsWith("group:kernel");
        const bool removableKernel = isKernelNode(*node) && !node->isGroup;
        m_addKernelBtn->setEnabled(kernelGroup);
        m_removeKernelBtn->setEnabled(removableKernel);
        m_updatingUi = false;
    }

    void onTreeItemChanged(QTreeWidgetItem* item, int column) {
        if (m_updatingUi || column != 0 || !item) return;
        RootNode* node = nodeById(item->data(0, Qt::UserRole).toString());
        if (!node) return;
        node->policyScope = QStringLiteral("override");
        node->enabled = item->checkState(0) == Qt::Checked;
        refreshTreeState();
        if (m_tree->currentItem() == item) loadInspector(item);
    }

    void onInspectorChanged() {
        if (m_updatingUi) return;
        RootNode* node = selectedNode();
        if (!node) return;

        const bool canInherit = parentNode(*node) != nullptr;
        node->policyScope = (!canInherit || m_overrideCheck->isChecked()) ? QStringLiteral("override")
                                                                          : QStringLiteral("inherit");
        if (node->policyScope == QStringLiteral("override")) {
            node->enabled = m_enabledCheck->isChecked();
            node->acValue = m_acCombo->currentText().trimmed();
            node->batteryValue = m_batCombo->currentText().trimmed();
        }

        refreshTreeState();
        loadInspector(m_tree->currentItem());
    }

    void onAddKernelTuning() {
        RootNode* node = selectedNode();
        if (!node || !node->id.startsWith("group:kernel")) return;

        bool ok = false;
        const QString input = QInputDialog::getText(this, "Add Kernel Tuning", "Kernel tuning path:",
                                                    QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok || input.isEmpty()) return;

        QString error;
        if (!validateKernelPath(input, error)) {
            QMessageBox::warning(this, "Invalid Kernel Tuning", error);
            return;
        }

        const QFileInfo fi(input);
        const QString canonical = fi.canonicalFilePath().isEmpty() ? input : fi.canonicalFilePath();
        for (const auto& existing : m_state.nodes) {
            if (existing.absPath == canonical) {
                QMessageBox::information(this, "Already Present", "That kernel tuning is already in the tree.");
                return;
            }
        }

        RootNode added;
        added.id = QString("node:%1").arg(canonical);
        added.parentId = node->id == "group:kernel" ? kernelParentForPath(canonical) : node->id;
        added.absPath = canonical;
        added.label = kernelLabelForPath(canonical);
        added.nodeClass = "kernel";
        added.enabled = true;
        added.policyScope = "override";
        QFile probe(canonical);
        if (probe.open(QIODevice::ReadOnly | QIODevice::Text)) {
            added.currentValue = QString::fromUtf8(probe.readLine()).trimmed();
        }
        added.allowedValues = detectKernelOptions(canonical);
        added.acValue = added.currentValue;
        added.batteryValue = added.currentValue;
        m_state.nodes.push_back(std::move(added));

        m_indexById.clear();
        for (int i = 0; i < m_state.nodes.size(); ++i) m_indexById.insert(m_state.nodes[i].id, i);
        rebuildTree();
        if (auto* item = m_treeItems.value(QString("node:%1").arg(canonical), nullptr)) {
            m_tree->setCurrentItem(item);
        }
    }

    void onRemoveKernelTuning() {
        RootNode* node = selectedNode();
        if (!node || !isKernelNode(*node) || node->isGroup) return;
        const QString id = node->id;
        for (int i = 0; i < m_state.nodes.size(); ++i) {
            if (m_state.nodes[i].id == id) {
                m_state.nodes.removeAt(i);
                break;
            }
        }
        m_indexById.clear();
        for (int i = 0; i < m_state.nodes.size(); ++i) m_indexById.insert(m_state.nodes[i].id, i);
        rebuildTree();
    }

    void onSave() {
        if (!m_disclaimerAccepted) {
            QMessageBox::warning(this, "Confirmation required",
                                 "Saving is disabled until you click Confirm and agree to the disclaimer.");
            return;
        }
        dp::features::RootCompositeFeature composite(m_etcPath);
        m_state.disclaimerAccepted = m_disclaimerAccepted;
        m_state.acceptedAt = m_disclaimerAcceptedAt;
        const QByteArray data = composite.serialize(m_state);
        if (pkexecWrite(data, m_etcPath)) {
            QMessageBox::information(this, "Saved", "Root features saved. The daemon will auto-reload.");
            accept();
        }
    }
};

} // namespace

MainWindow::MainWindow(DbusClient* dbus, Config* config, QWidget* parent)
    : QMainWindow(parent), m_dbus(dbus), m_config(config)
{
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);

    m_graph = new LoadGraphWidget(this);
    layout->addWidget(m_graph);
    connect(m_graph, &LoadGraphWidget::thresholdsPreview, this, [this](double low, double high) {
        Q_UNUSED(low);
        Q_UNUSED(high);
    });
    connect(m_graph, &LoadGraphWidget::thresholdsCommitted, this, &MainWindow::onGraphThresholdChanged);

    m_overrideBtn = new QPushButton(this);
    layout->addWidget(m_overrideBtn);
    connect(m_overrideBtn, &QPushButton::clicked, this, &MainWindow::onOverrideButtonClicked);

    auto* profileBtn = new QPushButton("Profile Configuration", this);
    layout->addWidget(profileBtn);
    connect(profileBtn, &QPushButton::clicked, this, [this] {
        ProfileConfigDialog dlg(this, "/etc/dynamic_power.yaml");
        dlg.exec();
    });

    auto* rootFeatBtn = new QPushButton("Power-saving Features…", this);
    layout->addWidget(rootFeatBtn);
    connect(rootFeatBtn, &QPushButton::clicked, this, [this] {
        if (m_rootDialog) {
            m_rootDialog->raise();
            m_rootDialog->activateWindow();
            return;
        }
        m_rootDialog = new RootFeaturesDialog(this, "/etc/dynamic_power.yaml");
        m_rootDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(m_rootDialog, &QObject::destroyed, this, [this] { m_rootDialog = nullptr; });
        m_rootDialog->show();
    });

    m_powerLabel = new QLabel(this);
    m_powerLabel->setText("Power: …");
    QFont f = m_powerLabel->font();
    f.setBold(true);
    m_powerLabel->setFont(f);
    m_powerLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_powerLabel);

    auto* sectionLabel = new QLabel("Process Matches", this);
    sectionLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(sectionLabel);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    m_rulesPanel = new QWidget(scroll);
    m_rulesLayout = new QVBoxLayout(m_rulesPanel);
    m_rulesLayout->setContentsMargins(6, 6, 6, 6);
    scroll->setWidget(m_rulesPanel);
    layout->addWidget(scroll);

    layout->setStretch(0, 3);
    layout->setStretch(2, 1);

    refreshProcessButtons();
    setCentralWidget(central);
    setWindowTitle("Dynamic Power Control");
    resize(600, 600);
    refreshOverrideButton();
}

void MainWindow::setThresholds(double low, double high) {
    m_graph->setThresholds(low, high);
    refreshOverrideButton();
}

void MainWindow::setActiveProfile(const QString& profile) {
    m_activeProfile = profile;
    refreshOverrideButton();
}

void MainWindow::setProcessMatchState(const QSet<QString>& matches, const QString& winnerLower) {
    m_matchedProcs = matches;
    m_winnerProc = winnerLower;
    refreshProcessButtons();
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    emit visibilityChanged(true);
}

void MainWindow::hideEvent(QHideEvent* e) {
    QMainWindow::hideEvent(e);
    emit visibilityChanged(false);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    e->ignore();
    hide();
    emit visibilityChanged(false);
}

void MainWindow::onOverrideButtonClicked() {
    QMenu menu(this);
    const QStringList modes = {"Dynamic", "Inhibit Powersave", "Performance", "Balanced", "Powersave"};
    for (const auto& m : modes) {
        QAction* act = menu.addAction(m);
        connect(act, &QAction::triggered, this, [this, m]() {
            m_userMode = m;
            const bool boss = (m != QStringLiteral("Dynamic"));
            emit userOverrideSelected(m, boss);
            refreshOverrideButton();
        });
    }
    menu.exec(QCursor::pos());
}

void MainWindow::onGraphThresholdChanged(double low, double high) {
    emit thresholdsAdjusted(low, high);
}

void MainWindow::refreshOverrideButton() {
    m_overrideBtn->setText(QString("User Override: %1 - Current power profile: %2")
                           .arg(m_userMode, m_activeProfile));
    m_overrideBtn->setStyleSheet(m_userMode != QStringLiteral("Dynamic")
        ? "background: palette(highlight); color: palette(highlighted-text);"
        : "");
}

void MainWindow::refreshProcessButtons() {
    if (!m_rulesLayout) return;
    QLayoutItem* child;
    while ((child = m_rulesLayout->takeAt(0)) != nullptr) {
        if (auto* w = child->widget()) w->deleteLater();
        delete child;
    }

    const auto& rules = m_config->processRules();
    for (int i = 0; i < rules.size(); ++i) {
        const auto& r = rules[i];
        const QString label = r.name.trimmed().isEmpty() ? r.process_name : r.name;
        auto* btn = new QPushButton(label, m_rulesPanel);
        btn->setToolTip(QString("Process “%1” · Priority %2 · Mode %3")
                            .arg(r.process_name)
                            .arg(r.priority)
                            .arg(r.active_profile));

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            auto current = m_config->processRules();
            if (i < 0 || i >= current.size()) return;

            ProcessRuleEditor dlg(this);
            dlg.setRule(current[i]);

            bool didDelete = false;
            connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&] { didDelete = true; });

            if (dlg.exec() == QDialog::Accepted) {
                if (didDelete) current.removeAt(i);
                else current[i] = dlg.rule();
                m_config->setProcessRules(current);
                m_config->save();
                refreshProcessButtons();
            }
        });

        const QString lname = r.process_name.toLower();
        if (m_userMode == QStringLiteral("Dynamic") && !m_winnerProc.isEmpty() && lname == m_winnerProc) {
            btn->setStyleSheet("background: palette(highlight); color: palette(highlighted-text);");
        } else if (m_matchedProcs.contains(lname)) {
            const QColor c = qApp->palette().color(QPalette::Active, QPalette::Highlight);
            btn->setStyleSheet(QString("border-left: 5px solid rgb(%1,%2,%3); background: rgba(%1,%2,%3,0.16);")
                               .arg(c.red()).arg(c.green()).arg(c.blue()));
        }

        m_rulesLayout->addWidget(btn);
    }

    auto* addBtn = new QPushButton("Add process to match", m_rulesPanel);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        ProcessRuleEditor dlg(this);
        ProcessRule empty;
        dlg.setRule(empty);

        bool didDelete = false;
        connect(&dlg, &ProcessRuleEditor::deleteRequested, this, [&] { didDelete = true; });

        if (dlg.exec() == QDialog::Accepted && !didDelete) {
            auto rules = m_config->processRules();
            auto r = dlg.rule();
            if (!r.process_name.trimmed().isEmpty()) {
                rules.push_back(r);
                m_config->setProcessRules(rules);
                m_config->save();
                refreshProcessButtons();
            }
        }
    });

    m_rulesLayout->addWidget(addBtn);
    m_rulesLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void MainWindow::setPowerInfo(const QString& text) {
    m_powerLabel->setText(text);
}

void MainWindow::closeFeaturesDialogIfOpen() {
    if (m_rootDialog) m_rootDialog->close();
}

#include "MainWindow.moc"
