
#pragma once
#include <QWidget>
#include <deque>

class QTimer;

class LoadGraphWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoadGraphWidget(QWidget* parent = nullptr);
    void setThresholds(double low, double high);

signals:
    void thresholdsChanged(double low, double high);
    void thresholdsPreview(double low, double high);   // while dragging
    void thresholdsCommitted(double low, double high); // on mouse release

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    QSize minimumSizeHint() const override { return {300, 160}; }

private:
    QTimer* m_timer;
    std::deque<double> m_samples;
    int m_maxSamples = 120; // e.g. 2 min at 1s
    double m_low = 1.0;
    double m_high = 2.0;
    bool m_draggingLow = false;
    bool m_draggingHigh = false;

    void sampleLoad();
    double currentMaxY() const;
    double valueToY(double v) const;
    double yToValue(double y) const;
    int m_grabTol = 12;  // was ~6; make it easier to grab
    int m_padLeft = 42;
    int m_padTop = 8;
    int m_padBottom = 12;
};
