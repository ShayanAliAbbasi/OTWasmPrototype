// @otlicense
// File: PlotWidget.h
// @otlicense-end

#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <QVector>

class QCheckBox;
class QLabel;
class QSpinBox;
class QwtPlot;
class QwtPlotCurve;
class QwtPlotGrid;
class QwtPlotMagnifier;
class QwtPlotPanner;
class QwtPlotZoomer;

namespace prototype {

class PlotWidget : public QWidget {
public:
    explicit PlotWidget(QWidget* _parent = nullptr);
    double runBenchmarkIterations(int _iterations);

private:
    struct CurveEntry {
        QString entityPath;
        QColor color;
        QVector<QPointF> points;
        QwtPlotCurve* curve = nullptr;
    };

    void createPlot();
    void populateCurves();
    void resetView();
    void setGridVisible(bool _visible);
    void setSymbolsVisible(bool _visible);
    void setPointInterval(int _interval);
    void setSelectedCurve(QwtPlotCurve* _curve, int _pointIndex);
    void updateSelectionText(const CurveEntry* _entry, int _pointIndex);

    QwtPlot* m_plot = nullptr;
    QwtPlotGrid* m_grid = nullptr;
    QwtPlotZoomer* m_zoomer = nullptr;
    QwtPlotPanner* m_panner = nullptr;
    QwtPlotMagnifier* m_magnifier = nullptr;
    QLabel* m_selectionLabel = nullptr;
    QCheckBox* m_pointsToggle = nullptr;
    QSpinBox* m_pointInterval = nullptr;
    QVector<CurveEntry> m_curves;
    QwtPlotCurve* m_selectedCurve = nullptr;
};

} // namespace prototype
