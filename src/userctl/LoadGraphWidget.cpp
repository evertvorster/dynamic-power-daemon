
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
    m_timer->start(1000);
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
    emit thresholdsChanged(m_low, m_high);
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
    double h = height() - 20.0;
    double ratio = v / maxY;
    return 10.0 + (1.0 - ratio) * h;
}

double LoadGraphWidget::yToValue(double y) const {
    double maxY = currentMaxY();
    double h = height() - 20.0;
    double ratio = 1.0 - ((y - 10.0) / h);
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    return ratio * maxY;
}

void LoadGraphWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Axes
    p.setPen(QPen(Qt::gray, 1));
    p.drawRect(rect().adjusted(0,0,-1,-1));

    // Threshold lines
    p.setPen(QPen(Qt::red, 2, Qt::DashLine));
    p.drawLine(0, valueToY(m_low), width(), valueToY(m_low));
    p.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
    p.drawLine(0, valueToY(m_high), width(), valueToY(m_high));

    // Samples
    p.setPen(QPen(Qt::green, 2));
    double step = width() / double(m_maxSamples - 1);
    QPointF prev(0, valueToY(m_samples.front()));
    int i = 0;
    for (double v : m_samples) {
        QPointF pt(i * step, valueToY(v));
        if (i) p.drawLine(prev, pt);
        prev = pt;
        ++i;
    }

    // Labels
    p.setPen(Qt::white);
    p.drawText(8, 16, QString("MaxY=%1  Low=%2  High=%3").arg(currentMaxY()).arg(m_low).arg(m_high));
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
}
