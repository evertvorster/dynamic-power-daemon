#include "RootFeatureDetector.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

namespace dp::features {
namespace {

using Node = RootCompositeFeature::Node;
using State = RootCompositeFeature::State;

QString readLine(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readLine()).trimmed();
}

QString canonicalPath(const QString& path) {
    QFileInfo fi(path);
    const QString canonical = fi.canonicalFilePath();
    return canonical.isEmpty() ? path : canonical;
}

QString humanizeSegment(const QString& segment) {
    if (segment.isEmpty()) return segment;
    static const QRegularExpression pciRe("^\\d{4}:\\d{2}:\\d{2}\\.\\d$");
    static const QRegularExpression usbRe("^\\d+-\\d+(\\.\\d+)*$");
    if (pciRe.match(segment).hasMatch()) return QString("PCI Device %1").arg(segment);
    if (usbRe.match(segment).hasMatch()) return QString("USB Device %1").arg(segment);
    QString label = segment;
    label.replace('_', ' ');
    return label;
}

QMap<QString, QString> pciDescriptions() {
    QMap<QString, QString> out;
    QProcess proc;
    proc.start("lspci", {"-D"});
    if (!proc.waitForStarted(2000) || !proc.waitForFinished(4000) ||
        proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return out;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    for (const QString& line : output.split('\n', Qt::SkipEmptyParts)) {
        const int firstSpace = line.indexOf(' ');
        if (firstSpace <= 0) continue;
        const QString slot = line.left(firstSpace).trimmed();
        const QString desc = line.mid(firstSpace + 1).trimmed();
        if (!slot.isEmpty() && !desc.isEmpty()) out.insert(slot, desc);
    }
    return out;
}

QString deviceLabel(const QString& devicePath) {
    const QString modalias = readLine(devicePath + "/modalias");
    const QString subsystem = QFileInfo(devicePath + "/subsystem").symLinkTarget();
    const QString subsystemName = subsystem.isEmpty() ? QString() : QFileInfo(subsystem).fileName();
    const QString seg = QFileInfo(devicePath).fileName();
    static const QMap<QString, QString> pciMap = pciDescriptions();

    if (subsystemName == "pci") {
        const QString desc = pciMap.value(seg);
        return desc.isEmpty() ? QString("PCI Device %1").arg(seg)
                              : QString("%1 (%2)").arg(desc, seg);
    }
    if (subsystemName == "usb") return QString("USB Device %1").arg(seg);
    if (subsystemName == "nvme") return QString("NVMe %1").arg(seg);
    if (subsystemName == "net") return QString("Network Device %1").arg(seg);
    if (subsystemName == "drm") return QString("Display Device %1").arg(seg);
    if (subsystemName == "sound") return QString("Audio Device %1").arg(seg);
    if (!modalias.isEmpty()) return QString("%1 %2").arg(subsystemName.isEmpty() ? "Device" : subsystemName, seg);
    return humanizeSegment(seg);
}

QStringList optionsFromBracketList(const QString& content) {
    QString normalized = content;
    normalized.replace('\n', ' ');
    QStringList out;
    const auto tokens = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token.remove('[');
        token.remove(']');
        if (!token.isEmpty() && !out.contains(token)) out.push_back(token);
    }
    return out;
}

QStringList detectOptions(const QString& path) {
    if (path.endsWith("/power/control")) return {"on", "auto"};
    if (path == "/proc/sys/kernel/nmi_watchdog") return {"0", "1"};

    if (path == "/sys/firmware/acpi/platform_profile") {
        const QStringList opts = optionsFromBracketList(readLine("/sys/firmware/acpi/platform_profile_choices"));
        if (!opts.isEmpty()) return opts;
    }
    if (path == "/sys/module/pcie_aspm/parameters/policy") {
        const QStringList opts = optionsFromBracketList(readLine(path));
        if (!opts.isEmpty()) return opts;
    }
    const QString current = readLine(path);
    return current.isEmpty() ? QStringList{} : QStringList{current};
}

Node makeNode(const QString& id, const QString& parentId, const QString& label,
              const QString& nodeClass, const QString& path = QString()) {
    Node node;
    node.id = id;
    node.parentId = parentId;
    node.label = label;
    node.nodeClass = nodeClass;
    node.absPath = path;
    node.isGroup = path.isEmpty();
    node.supported = path.isEmpty() || QFileInfo::exists(path);
    node.detected = true;
    if (!path.isEmpty()) {
        node.currentValue = readLine(path);
        node.allowedValues = detectOptions(path);
    }
    return node;
}

} // namespace

RootCompositeFeature::State RootFeatureDetector::detect()
{
    State st;

    st.nodes.push_back(makeNode("group:kernel", {}, "Kernel Tunings", "kernel"));
    st.nodes.push_back(makeNode("group:kernel:general", "group:kernel", "General", "kernel"));
    st.nodes.push_back(makeNode("group:kernel:vm", "group:kernel", "Memory Writeback", "kernel"));
    st.nodes.push_back(makeNode("group:kernel:audio", "group:kernel", "Audio Power", "kernel"));
    st.nodes.push_back(makeNode("group:devices", {}, "Device Tree", "device"));
    st.nodes.push_back(makeNode("group:legacy", {}, "Legacy / Unmatched Rules", "legacy"));

    QMap<QString, QString> segmentToId;
    QDirIterator it("/sys/devices", {"control"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString controlPath = canonicalPath(it.next());
        if (!controlPath.endsWith("/power/control")) continue;

        const QString devicePath = QFileInfo(controlPath).dir().absolutePath().chopped(QString("/power").size());
        if (!devicePath.startsWith("/sys/devices/")) continue;

        QString parentId = "group:devices";
        const QStringList segments = devicePath.mid(QString("/sys/devices/").size()).split('/', Qt::SkipEmptyParts);
        QString runningPath = "/sys/devices";
        for (int i = 0; i < segments.size(); ++i) {
            runningPath += "/" + segments[i];
            const QString segmentKey = canonicalPath(runningPath);
            if (!segmentToId.contains(segmentKey)) {
                const QString id = QString("segment:%1").arg(segmentKey);
                QString label = (i == segments.size() - 1) ? deviceLabel(segmentKey) : humanizeSegment(segments[i]);
                st.nodes.push_back(makeNode(id, parentId, label, "device"));
                segmentToId.insert(segmentKey, id);
            }
            parentId = segmentToId.value(segmentKey);
        }

        Node leaf = makeNode(QString("node:%1").arg(controlPath), parentId, "Runtime Power Control", "device", controlPath);
        st.nodes.push_back(leaf);
    }

    return st;
}

} // namespace dp::features
