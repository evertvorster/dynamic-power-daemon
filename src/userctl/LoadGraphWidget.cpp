
#include "LoadGraphWidget.h"
#include <QPainter>
#include <QTimer>
#include <QFile>
#include <QMouseEvent>
#include <QTextStream>
#include <cmath>
#include <QCursor> 

LoadGraphWidget::LoadGraphWidget(QWidget* parent) : QWidget(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LoadGraphWidget::sampleLoad);
    m_timer->start(5000);
    m_samples.assign(m_maxSamples, 0.0);
    setMouseTracking(true); 
    setMinimumHeight(240);                    // keep some vertical space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);  
}

void LoadGraphWidget::mousePressEvent(QMouseEvent* e) {
    double y = e->position().y();
    double lowY = valueToY(m_low);
    double highY = valueToY(m_high);
    if (std::abs(y - lowY) < m_grabTol) {
        m_draggingLow = true;
        setCursor(Qt::SizeVerCursor);
    } else if (std::abs(y - highY) < m_grabTol) {
        m_draggingHigh = true;
        setCursor(Qt::SizeVerCursor);
    }
}

void LoadGraphWidget::mouseMoveEvent(QMouseEvent* e) {
    double y = e->position().y();

    // Hover feedback when not dragging
    if (!m_draggingLow && !m_draggingHigh) {
        double lowY = valueToY(m_low);
        double highY = valueToY(m_high);
        if (std::abs(y - lowY) < m_grabTol || std::abs(y - highY) < m_grabTol)
            setCursor(Qt::SizeVerCursor);
        else
            unsetCursor();
        return;  // not dragging; nothing else to do
    }

    // Dragging
    double v = yToValue(y);
    if (m_draggingLow) {
        if (v > m_high) v = m_high;
        m_low = v;
    } else {
        if (v < m_low) v = m_low;
        m_high = v;
    }
    emit thresholdsPreview(m_low, m_high);
    update();
}

double LoadGraphWidget::currentMaxY() const {
    double m = 0.0;
    for (double v : m_samples) m = std::max(m, v);
    m = std::ceil(m + 1.0);
    if (m < 6.0) m = 6.0;
    return m;
}

double LoadGraphWidget::valueToY(double v) const {
    double maxY = currentMaxY();
    double h = height() - (m_padTop + m_padBottom);
    double ratio = (maxY > 0.0) ? (v / maxY) : 0.0;
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    return m_padTop + (1.0 - ratio) * h;
}

double LoadGraphWidget::yToValue(double y) const {
    double maxY = currentMaxY();
    double h = height() - (m_padTop + m_padBottom);
    double ratio = 1.0 - ((y - m_padTop) / h);
    if (ratio < 0) ratio = 0; if (ratio > 1) ratio = 1;
    return ratio * maxY;
}

void LoadGraphWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Frame
    p.setPen(QPen(Qt::gray, 1));
    p.drawRect(rect().adjusted(0,0,-1,-1));

    const double maxY = currentMaxY();
    const int x0 = m_padLeft;
    const int x1 = width() - 1;

    // Y grid + integer labels
    p.setPen(QPen(QColor(70,70,70), 1, Qt::DotLine));
    QFontMetrics fm(p.font());
    for (int i = 0; i <= int(std::ceil(maxY)); ++i) {
        const double yy = valueToY(i);
        p.drawLine(x0, yy, x1, yy);                        // grid
        const QString lab = QString::number(i);
        p.setPen(Qt::lightGray);
        // Center the label horizontally within the left gutter [0 .. x0-6]
        QRect labRect(0, int(yy - fm.height()/2), x0 - 6, fm.height());
        p.drawText(labRect, Qt::AlignHCenter | Qt::AlignVCenter, lab);
        p.setPen(QPen(QColor(70,70,70), 1, Qt::DotLine));  // restore for next grid
    }

    // Threshold lines
    // Subtle threshold lines: thinner + semi-transparent + solid
    p.setPen(QPen(QColor(255, 0, 0, 140), 1, Qt::SolidLine));        // low
    p.drawLine(x0, valueToY(m_low), x1, valueToY(m_low));
    p.setPen(QPen(QColor(255, 255, 0, 140), 1, Qt::SolidLine));      // high
    p.drawLine(x0, valueToY(m_high), x1, valueToY(m_high));

    // Samples
    p.setPen(QPen(Qt::green, 2));
    const double step = (x1 - x0) / double(std::max(1, m_maxSamples - 1));
    QPointF prev(x0, valueToY(m_samples.front()));
    int i = 0;
    for (double v : m_samples) {
        const QPointF pt(x0 + i * step, valueToY(v));
        if (i) p.drawLine(prev, pt);
        prev = pt;
        ++i;
    }
    // Title at top-center above the plot area
    p.setPen(Qt::lightGray);  // use Qt::white for brighter
    QFontMetrics fmTitle(p.font());
    QRect titleRect(x0, 0, x1 - x0, m_padTop);
    p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignVCenter, tr("Load Average"));

}

void LoadGraphWidget::setThresholds(double low, double high) {
    m_low = low;
    m_high = high;
    update();
}

void LoadGraphWidget::sampleLoad() {
    QFile f("/proc/loadavg");
    double val = 0.0;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        in >> val; // 1-minute load average
    }
    if (!m_samples.empty()) m_samples.pop_front();
    m_samples.push_back(val);
    update();
}

void LoadGraphWidget::mouseReleaseEvent(QMouseEvent*) {
    m_draggingLow = m_draggingHigh = false;
    unsetCursor();
    emit thresholdsCommitted(m_low, m_high);
}
