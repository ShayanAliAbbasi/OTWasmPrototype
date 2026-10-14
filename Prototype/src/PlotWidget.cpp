// @otlicense
// File: PlotWidget.cpp
// @otlicense-end

#include "PlotWidget.h"

#include <qwt_legend.h>
#include <qwt_picker.h>
#include <qwt_plot.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_zoomer.h>
#include <qwt_scale_map.h>
#include <qwt_symbol.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace prototype {
namespace {

constexpr double g_selectionThresholdPx = 150.0;

int verifyRange(int _size, int& _first, int& _last) {
    if (_size < 1) {
        return 0;
    }

    _first = qBound(0, _first, _size - 1);
    _last = qBound(0, _last, _size - 1);

    if (_first > _last) {
        qSwap(_first, _last);
    }

    return _last - _first + 1;
}

QRectF intersectedClipRect(const QRectF& _rect, QPainter* _painter) {
    QRectF clipRect = _rect;
    if (_painter->hasClipping()) {
        clipRect &= _painter->clipBoundingRect();
    }

    return clipRect;
}

class PrototypePlotCurve : public QwtPlotCurve {
public:
    explicit PrototypePlotCurve(const QString& _title)
        : QwtPlotCurve(_title)
    {
        setRenderHint(QwtPlotItem::RenderAntialiased, true);
    }

    void setBasePen(const QPen& _pen) {
        m_basePen = _pen;
        setSelected(m_selected);
    }

    void setSelected(bool _selected) {
        m_selected = _selected;

        QPen pen = m_basePen;
        if (m_selected) {
            pen.setWidthF(std::max(3.0, pen.widthF() + 1.5));
        }
        setPen(pen);
    }

    void setSymbolsVisible(bool _visible) {
        if (_visible) {
            setSymbol(new QwtSymbol(
                QwtSymbol::Ellipse,
                QBrush(Qt::white),
                QPen(m_basePen.color(), 1.2),
                QSize(5, 5)
            ));
        }
        else {
            setSymbol(nullptr);
        }
    }

    void setPointInterval(int _interval) {
        m_pointInterval = std::max(1, _interval);
    }

protected:
    void drawSeries(QPainter* _painter, const QwtScaleMap& _xMap, const QwtScaleMap& _yMap, const QRectF& _canvasRect, int _from, int _to) const override {
        const int numSamples = static_cast<int>(dataSize());
        if (!_painter || numSamples <= 0) {
            return;
        }

        if (_to < 0) {
            _to = numSamples - 1;
        }

        if (verifyRange(numSamples, _from, _to) <= 0) {
            return;
        }

        if (pen().style() != Qt::NoPen) {
            _painter->save();
            _painter->setPen(pen());
            drawCurve(_painter, style(), _xMap, _yMap, _canvasRect, _from, _to);
            _painter->restore();
        }

        if (symbol() && symbol()->style() != QwtSymbol::NoSymbol) {
            _painter->save();
            drawSymbols(_painter, *symbol(), _xMap, _yMap, _canvasRect, _from, _to);
            _painter->restore();
        }
    }

    void drawSymbols(QPainter* _painter, const QwtSymbol& _symbol, const QwtScaleMap& _xMap, const QwtScaleMap& _yMap, const QRectF& _canvasRect, int _from, int _to) const override {
        const QRectF clipRect = intersectedClipRect(_canvasRect, _painter).adjusted(-8.0, -8.0, 8.0, 8.0);
        const int chunkSize = 500;

        for (int first = _from; first <= _to; first += chunkSize) {
            const int last = std::min(first + chunkSize - 1, _to);
            QPolygonF points;
            points.reserve((last - first + m_pointInterval) / m_pointInterval);

            for (int i = first; i <= last; ++i) {
                if ((i - _from) % m_pointInterval != 0) {
                    continue;
                }

                const QPointF sample = data()->sample(i);
                const QPointF mapped(_xMap.transform(sample.x()), _yMap.transform(sample.y()));
                if (clipRect.contains(mapped)) {
                    points.append(mapped);
                }
            }

            if (!points.isEmpty()) {
                _symbol.drawSymbols(_painter, points);
            }
        }
    }

private:
    QPen m_basePen = QPen(Qt::white, 1.8);
    bool m_selected = false;
    int m_pointInterval = 1;
};

class PrototypePlotMagnifier : public QwtPlotMagnifier {
public:
    explicit PrototypePlotMagnifier(QWidget* _canvas)
        : QwtPlotMagnifier(_canvas)
    {
        setMouseButton(Qt::NoButton);
        setWheelFactor(0.85);
    }

protected:
    void widgetWheelEvent(QWheelEvent* _event) override {
        m_cursorPos = _event->position();

        if (_event->modifiers() != wheelModifiers() || wheelFactor() == 0.0) {
            return;
        }

        int delta = _event->angleDelta().y() * -1;
        double factor = std::pow(wheelFactor(), std::abs(delta / 120.0));
        if (delta > 0) {
            factor = 1.0 / factor;
        }

        rescaleAroundCursor(factor);
    }

private:
    void rescaleAroundCursor(double _factor) {
        QwtPlot* plt = plot();
        if (!plt) {
            return;
        }

        _factor = std::abs(_factor);
        if (_factor == 0.0 || _factor == 1.0) {
            return;
        }

        const bool autoReplot = plt->autoReplot();
        plt->setAutoReplot(false);

        bool doReplot = false;
        const int axes[] = { QwtPlot::xBottom, QwtPlot::yLeft };
        for (int axisId : axes) {
            if (!isAxisEnabled(axisId)) {
                continue;
            }

            const QwtScaleMap scaleMap = plt->canvasMap(axisId);
            double lower = scaleMap.s1();
            double upper = scaleMap.s2();

            if (scaleMap.transformation()) {
                lower = scaleMap.transform(lower);
                upper = scaleMap.transform(upper);
            }

            double cursor = 0.0;
            if (axisId == QwtPlot::xBottom) {
                cursor = scaleMap.invTransform(m_cursorPos.x());
            }
            else {
                cursor = scaleMap.invTransform(m_cursorPos.y());
            }

            if (scaleMap.transformation()) {
                cursor = scaleMap.transform(cursor);
            }

            const double center = 0.5 * (lower + upper);
            const double halfWidth = 0.5 * (upper - lower) * _factor;
            const double newCenter = cursor - _factor * (cursor - center);

            lower = newCenter - halfWidth;
            upper = newCenter + halfWidth;

            if (scaleMap.transformation()) {
                lower = scaleMap.invTransform(lower);
                upper = scaleMap.invTransform(upper);
            }

            plt->setAxisScale(axisId, lower, upper);
            doReplot = true;
        }

        plt->setAutoReplot(autoReplot);
        if (doReplot) {
            plt->replot();
        }
    }

    QPointF m_cursorPos;
};

class PrototypePlot : public QwtPlot {
public:
    explicit PrototypePlot(QWidget* _parent = nullptr)
        : QwtPlot(_parent)
    {
    }

    void setCurveSelectionHandler(std::function<void(QwtPlotCurve*, int)> _handler) {
        m_curveSelectionHandler = std::move(_handler);
    }

    QwtPlotCurve* findNearestCurve(const QPoint& _pos, int& _pointIndex, double& _distance) const {
        QwtPlotCurve* nearestCurve = nullptr;
        _distance = std::numeric_limits<double>::max();
        _pointIndex = -1;

        for (QwtPlotItem* item : itemList()) {
            if (item->rtti() != QwtPlotItem::Rtti_PlotCurve) {
                continue;
            }

            QwtPlotCurve* curve = static_cast<QwtPlotCurve*>(item);
            double distance = 0.0;
            const int index = curve->closestPoint(_pos, &distance);
            if (index >= 0 && distance < _distance) {
                nearestCurve = curve;
                _distance = distance;
                _pointIndex = index;
            }
        }

        return nearestCurve;
    }

protected:
    void mouseDoubleClickEvent(QMouseEvent* _event) override {
        QwtPlot::mouseDoubleClickEvent(_event);
        if (_event->button() != Qt::LeftButton) {
            return;
        }

        int pointIndex = -1;
        double distance = std::numeric_limits<double>::max();
        QwtPlotCurve* curve = findNearestCurve(_event->pos(), pointIndex, distance);
        if (!m_curveSelectionHandler) {
            return;
        }

        if (curve && distance <= g_selectionThresholdPx) {
            m_curveSelectionHandler(curve, pointIndex);
        }
        else {
            m_curveSelectionHandler(nullptr, -1);
        }
    }

private:
    std::function<void(QwtPlotCurve*, int)> m_curveSelectionHandler;
};

QVector<QPointF> makeCurveSamples(double _phase, double _amplitude, double _offset) {
    QVector<QPointF> points;
    points.reserve(720);

    for (int i = 0; i < 720; ++i) {
        const double x = static_cast<double>(i) * 0.025;
        const double envelope = 1.0 + 0.18 * std::sin(x * 0.28 + _phase);
        const double y = _offset + _amplitude * envelope * std::sin(x + _phase) + 0.08 * std::cos(x * 3.0 + _phase);
        points.append(QPointF(x, y));
    }

    return points;
}

QToolButton* createToolButton(const QString& _text, QWidget* _parent) {
    QToolButton* button = new QToolButton(_parent);
    button->setText(_text);
    button->setAutoRaise(false);
    button->setMinimumHeight(24);
    return button;
}

} // namespace

PlotWidget::PlotWidget(QWidget* _parent)
    : QWidget(_parent)
{
    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    QFrame* toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("PlotToolbar"));
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(8, 6, 8, 6);
    toolbarLayout->setSpacing(10);

    QToolButton* resetButton = createToolButton(QStringLiteral("Reset View"), toolbar);
    connect(resetButton, &QToolButton::clicked, this, &PlotWidget::resetView);
    toolbarLayout->addWidget(resetButton);

    QCheckBox* gridToggle = new QCheckBox(QStringLiteral("Grid"), toolbar);
    gridToggle->setChecked(true);
    connect(gridToggle, &QCheckBox::toggled, this, &PlotWidget::setGridVisible);
    toolbarLayout->addWidget(gridToggle);

    m_pointsToggle = new QCheckBox(QStringLiteral("Curve points"), toolbar);
    m_pointsToggle->setChecked(true);
    connect(m_pointsToggle, &QCheckBox::toggled, this, &PlotWidget::setSymbolsVisible);
    toolbarLayout->addWidget(m_pointsToggle);

    QLabel* intervalLabel = new QLabel(QStringLiteral("Point interval"), toolbar);
    toolbarLayout->addWidget(intervalLabel);

    m_pointInterval = new QSpinBox(toolbar);
    m_pointInterval->setRange(1, 40);
    m_pointInterval->setValue(6);
    connect(m_pointInterval, &QSpinBox::valueChanged, this, &PlotWidget::setPointInterval);
    toolbarLayout->addWidget(m_pointInterval);

    toolbarLayout->addStretch(1);
    m_selectionLabel = new QLabel(QStringLiteral("No curve selected"), toolbar);
    toolbarLayout->addWidget(m_selectionLabel);

    rootLayout->addWidget(toolbar);

    createPlot();
    rootLayout->addWidget(m_plot, 1);

    populateCurves();
    setPointInterval(m_pointInterval->value());
    setSymbolsVisible(m_pointsToggle->isChecked());
    resetView();
}

double PlotWidget::runBenchmarkIterations(int _iterations) {
    const int iterations = std::max(1, _iterations);
    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < iterations; ++i) {
        const double phase = static_cast<double>(i % 12) / 12.0;
        m_plot->setAxisScale(QwtPlot::xBottom, phase, 18.0 - phase);
        m_plot->setAxisScale(QwtPlot::yLeft, -1.8 + phase * 0.2, 1.8 - phase * 0.2);
        m_plot->replot();
        m_plot->canvas()->repaint();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    resetView();
    return static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
}

void PlotWidget::createPlot() {
    PrototypePlot* plot = new PrototypePlot(this);
    plot->setCurveSelectionHandler([this](QwtPlotCurve* _curve, int _pointIndex) {
        setSelectedCurve(_curve, _pointIndex);
    });

    plot->setTitle(QStringLiteral("Plot Signal Response"));
    plot->setAxisTitle(QwtPlot::xBottom, QStringLiteral("Time"));
    plot->setAxisTitle(QwtPlot::yLeft, QStringLiteral("Value"));
    plot->setCanvasBackground(QColor(30, 30, 30));
    plot->insertLegend(new QwtLegend(plot), QwtPlot::RightLegend);
    plot->setAutoReplot(false);

    QwtPlotCanvas* canvas = new QwtPlotCanvas(plot);
    canvas->setFrameStyle(QFrame::NoFrame);
    canvas->setPaintAttribute(QwtPlotCanvas::BackingStore, false);
    canvas->setFocusPolicy(Qt::StrongFocus);
    plot->setCanvas(canvas);

    m_grid = new QwtPlotGrid;
    m_grid->setMajorPen(QPen(QColor(82, 82, 82), 0.0, Qt::DotLine));
    m_grid->setMinorPen(QPen(QColor(50, 50, 50), 0.0, Qt::DotLine));
    m_grid->enableXMin(true);
    m_grid->enableYMin(true);
    m_grid->attach(plot);

    m_zoomer = new QwtPlotZoomer(plot->canvas());
    m_zoomer->setRubberBand(QwtPicker::RectRubberBand);
    m_zoomer->setRubberBandPen(QPen(QColor(0, 215, 255), 1.0, Qt::DashLine));
    m_zoomer->setTrackerMode(QwtPicker::AlwaysOff);

    m_panner = new QwtPlotPanner(plot->canvas());
    m_panner->setMouseButton(Qt::MiddleButton);

    m_magnifier = new PrototypePlotMagnifier(plot->canvas());
    m_magnifier->setAxisEnabled(QwtPlot::xBottom, true);
    m_magnifier->setAxisEnabled(QwtPlot::yLeft, true);

    m_plot = plot;
}

void PlotWidget::populateCurves() {
    m_curves = {
        { QStringLiteral("Dataset/Result/Voltage"), QColor(56, 186, 255), makeCurveSamples(0.0, 1.05, 0.0), nullptr },
        { QStringLiteral("Dataset/Result/Current"), QColor(255, 193, 80), makeCurveSamples(0.9, 0.76, 0.25), nullptr },
        { QStringLiteral("Dataset/Result/Temperature"), QColor(117, 222, 112), makeCurveSamples(1.8, 0.46, -0.35), nullptr }
    };

    for (CurveEntry& entry : m_curves) {
        PrototypePlotCurve* curve = new PrototypePlotCurve(entry.entityPath);
        curve->setBasePen(QPen(entry.color, 1.8));
        curve->setSamples(entry.points);
        curve->attach(m_plot);
        entry.curve = curve;
    }
}

void PlotWidget::resetView() {
    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    for (const CurveEntry& entry : m_curves) {
        if (!entry.curve || !entry.curve->isVisible()) {
            continue;
        }

        for (const QPointF& point : entry.points) {
            xMin = std::min(xMin, point.x());
            xMax = std::max(xMax, point.x());
            yMin = std::min(yMin, point.y());
            yMax = std::max(yMax, point.y());
        }
    }

    if (xMin < xMax && yMin < yMax) {
        const double xPadding = (xMax - xMin) * 0.035;
        const double yPadding = (yMax - yMin) * 0.12;
        m_plot->setAxisScale(QwtPlot::xBottom, xMin - xPadding, xMax + xPadding);
        m_plot->setAxisScale(QwtPlot::yLeft, yMin - yPadding, yMax + yPadding);
    }

    m_plot->replot();
    if (m_zoomer) {
        m_zoomer->setZoomBase(false);
    }
}

void PlotWidget::setGridVisible(bool _visible) {
    if (_visible) {
        m_grid->setMajorPen(QPen(QColor(82, 82, 82), 0.0, Qt::DotLine));
        m_grid->setMinorPen(QPen(QColor(50, 50, 50), 0.0, Qt::DotLine));
    }
    else {
        m_grid->setMajorPen(QPen(Qt::NoPen));
        m_grid->setMinorPen(QPen(Qt::NoPen));
    }

    m_plot->replot();
}

void PlotWidget::setSymbolsVisible(bool _visible) {
    for (CurveEntry& entry : m_curves) {
        PrototypePlotCurve* curve = dynamic_cast<PrototypePlotCurve*>(entry.curve);
        if (curve) {
            curve->setSymbolsVisible(_visible);
        }
    }

    m_plot->replot();
}

void PlotWidget::setPointInterval(int _interval) {
    for (CurveEntry& entry : m_curves) {
        PrototypePlotCurve* curve = dynamic_cast<PrototypePlotCurve*>(entry.curve);
        if (curve) {
            curve->setPointInterval(_interval);
        }
    }

    m_plot->replot();
}

void PlotWidget::setSelectedCurve(QwtPlotCurve* _curve, int _pointIndex) {
    m_selectedCurve = _curve;

    const CurveEntry* selectedEntry = nullptr;
    for (CurveEntry& entry : m_curves) {
        PrototypePlotCurve* curve = dynamic_cast<PrototypePlotCurve*>(entry.curve);
        if (!curve) {
            continue;
        }

        const bool selected = (curve == _curve);
        curve->setSelected(selected);
        if (selected) {
            selectedEntry = &entry;
        }
    }

    updateSelectionText(selectedEntry, _pointIndex);
    m_plot->replot();
}

void PlotWidget::updateSelectionText(const CurveEntry* _entry, int _pointIndex) {
    if (!_entry || _pointIndex < 0 || _pointIndex >= _entry->points.size()) {
        m_selectionLabel->setText(QStringLiteral("No curve selected"));
        return;
    }

    const QPointF point = _entry->points.at(_pointIndex);
    m_selectionLabel->setText(QStringLiteral("%1  x=%2  y=%3")
        .arg(_entry->entityPath)
        .arg(point.x(), 0, 'f', 3)
        .arg(point.y(), 0, 'f', 3));
}

} // namespace prototype
