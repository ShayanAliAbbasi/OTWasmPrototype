// @otlicense
// File: BenchmarkRunner.h
// @otlicense-end

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>

class QCoreApplication;

namespace prototype {

    class ModelingWindow;

    struct BenchmarkConfiguration {
        bool enabled = false;
        QString outputPath;
        QString runId;
        int entityCount = 100;
        int warmupFrames = 30;
        int measuredFrames = 180;
        int modelingOperations = 24;
        int plotIterations = 30;
        int viewportWidth = 1024;
        int viewportHeight = 540;
    };

    BenchmarkConfiguration benchmarkConfiguration(const QCoreApplication& _application);

    class BenchmarkRunner : public QObject {
    public:
        BenchmarkRunner(
            ModelingWindow& _window,
            const BenchmarkConfiguration& _configuration,
            const QElapsedTimer& _startupTimer,
            QObject* _parent = nullptr
        );

    private:
        void handleFrameRendered(double _renderMilliseconds);
        void runWorkloads();
        void startFrameMeasurement();
        void finish();

        ModelingWindow& m_window;
        BenchmarkConfiguration m_configuration;
        QElapsedTimer m_startupTimer;
        QElapsedTimer m_frameClock;
        double m_startupToFirstFrameMs = 0.0;
        double m_sceneSetupMs = 0.0;
        double m_modelingWorkloadMs = 0.0;
        double m_plotWorkloadMs = 0.0;
        double m_complexModelResourceReadMs = 0.0;
        double m_complexModelDeserializeMs = 0.0;
        qint64 m_complexModelPayloadBytes = 0;
        bool m_complexModelLoaded = false;
        qint64 m_previousFrameNs = 0;
        int m_seenFrames = 0;
        bool m_started = false;
        bool m_collectingFrames = false;
        QVector<double> m_renderSamplesMs;
        QVector<double> m_intervalSamplesMs;
    };

} // namespace prototype
