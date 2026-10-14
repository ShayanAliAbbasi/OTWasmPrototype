// @otlicense
// File: BenchmarkRunner.cpp
// @otlicense-end

#include "BenchmarkRunner.h"

#include "ModelingWindow.h"
#include "OsgModelingView.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>
#include <QtGlobal>

#include <osg/Version>

#include <algorithm>
#include <cmath>
#include <numeric>

#if defined(Q_OS_WASM)
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#else
#include <sys/resource.h>
#endif

namespace prototype {
namespace {

#if defined(Q_OS_WASM)
EM_JS(int, queryInteger, (const char* name, int fallback), {
    const value = new URLSearchParams(window.location.search).get(UTF8ToString(name));
    if (value === null) {
        return fallback;
    }
    const parsed = Number.parseInt(value, 10);
    return Number.isFinite(parsed) ? parsed : fallback;
});

#endif

int positiveValue(const QString& _value, int _fallback) {
    bool ok = false;
    const int value = _value.toInt(&ok);
    return ok && value > 0 ? value : _fallback;
}

QJsonArray samplesToJson(const QVector<double>& _samples) {
    QJsonArray values;
    for (double sample : _samples) {
        values.append(sample);
    }
    return values;
}

QJsonObject statistics(const QVector<double>& _samples) {
    QJsonObject result;
    result.insert(QStringLiteral("count"), _samples.size());
    result.insert(QStringLiteral("samples"), samplesToJson(_samples));
    if (_samples.isEmpty()) {
        return result;
    }

    QVector<double> sorted = _samples;
    std::sort(sorted.begin(), sorted.end());
    const double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    const double mean = sum / static_cast<double>(sorted.size());
    const int middle = sorted.size() / 2;
    const double median = sorted.size() % 2 == 0
        ? (sorted.at(middle - 1) + sorted.at(middle)) * 0.5
        : sorted.at(middle);
    const int p95Index = std::max(0, static_cast<int>(std::ceil(sorted.size() * 0.95)) - 1);

    double squaredDifferenceSum = 0.0;
    for (double value : sorted) {
        const double difference = value - mean;
        squaredDifferenceSum += difference * difference;
    }
    const double sampleStandardDeviation = sorted.size() > 1
        ? std::sqrt(squaredDifferenceSum / static_cast<double>(sorted.size() - 1))
        : 0.0;

    result.insert(QStringLiteral("mean"), mean);
    result.insert(QStringLiteral("median"), median);
    result.insert(QStringLiteral("sample_standard_deviation"), sampleStandardDeviation);
    result.insert(QStringLiteral("minimum"), sorted.front());
    result.insert(QStringLiteral("maximum"), sorted.back());
    result.insert(QStringLiteral("p95"), sorted.at(p95Index));
    return result;
}

qint64 memoryFootprintBytes() {
#if defined(Q_OS_WASM)
    return static_cast<qint64>(emscripten_get_heap_size());
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return -1;
    }
#if defined(Q_OS_MACOS)
    return static_cast<qint64>(usage.ru_maxrss);
#else
    return static_cast<qint64>(usage.ru_maxrss) * 1024;
#endif
#endif
}

QString targetName() {
#if defined(Q_OS_WASM)
    return QStringLiteral("wasm-webgl2");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("native-macos");
#else
    return QStringLiteral("native");
#endif
}

} // namespace

BenchmarkConfiguration benchmarkConfiguration(const QCoreApplication& _application) {
    BenchmarkConfiguration configuration;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("OpenTwin frontend prototype"));
    parser.addOption({ QStringLiteral("benchmark"), QStringLiteral("Run the deterministic benchmark and exit.") });
    parser.addOption({ QStringLiteral("benchmark-output"), QStringLiteral("Write one JSON result to this file."), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("benchmark-run-id"), QStringLiteral("Identifier stored with this run."), QStringLiteral("id") });
    parser.addOption({ QStringLiteral("benchmark-entities"), QStringLiteral("Number of entities in the measured scene."), QStringLiteral("count"), QStringLiteral("100") });
    parser.addOption({ QStringLiteral("benchmark-warmup-frames"), QStringLiteral("Frames excluded before measurement."), QStringLiteral("count"), QStringLiteral("30") });
    parser.addOption({ QStringLiteral("benchmark-frames"), QStringLiteral("Number of measured OSG frames."), QStringLiteral("count"), QStringLiteral("180") });
    parser.addOption({ QStringLiteral("benchmark-operations"), QStringLiteral("Number of modeling service operations."), QStringLiteral("count"), QStringLiteral("24") });
    parser.addOption({ QStringLiteral("benchmark-plot-iterations"), QStringLiteral("Number of Qwt replots."), QStringLiteral("count"), QStringLiteral("30") });
    parser.parse(_application.arguments());

    configuration.enabled = parser.isSet(QStringLiteral("benchmark"));
    configuration.outputPath = parser.value(QStringLiteral("benchmark-output"));
    configuration.runId = parser.value(QStringLiteral("benchmark-run-id"));
    configuration.entityCount = positiveValue(parser.value(QStringLiteral("benchmark-entities")), configuration.entityCount);
    configuration.warmupFrames = positiveValue(parser.value(QStringLiteral("benchmark-warmup-frames")), configuration.warmupFrames);
    configuration.measuredFrames = positiveValue(parser.value(QStringLiteral("benchmark-frames")), configuration.measuredFrames);
    configuration.modelingOperations = positiveValue(parser.value(QStringLiteral("benchmark-operations")), configuration.modelingOperations);
    configuration.plotIterations = positiveValue(parser.value(QStringLiteral("benchmark-plot-iterations")), configuration.plotIterations);

#if defined(Q_OS_WASM)
    configuration.enabled = queryInteger("benchmark", configuration.enabled ? 1 : 0) == 1;
    configuration.entityCount = std::max(1, queryInteger("entities", configuration.entityCount));
    configuration.warmupFrames = std::max(1, queryInteger("warmupFrames", configuration.warmupFrames));
    configuration.measuredFrames = std::max(1, queryInteger("frames", configuration.measuredFrames));
    configuration.modelingOperations = std::max(1, queryInteger("operations", configuration.modelingOperations));
    configuration.plotIterations = std::max(1, queryInteger("plotIterations", configuration.plotIterations));
#endif

    return configuration;
}

BenchmarkRunner::BenchmarkRunner(
    ModelingWindow& _window,
    const BenchmarkConfiguration& _configuration,
    const QElapsedTimer& _startupTimer,
    QObject* _parent
)
    : QObject(_parent),
      m_window(_window),
      m_configuration(_configuration),
      m_startupTimer(_startupTimer)
{
    if (!m_configuration.enabled || !m_window.sceneView()) {
        return;
    }

    connect(m_window.sceneView(), &OsgModelingView::frameRendered, this, [this](double milliseconds) {
        handleFrameRendered(milliseconds);
    });
}

void BenchmarkRunner::handleFrameRendered(double _renderMilliseconds) {
    if (!m_configuration.enabled) {
        return;
    }

    if (!m_started) {
        m_started = true;
        m_startupToFirstFrameMs = static_cast<double>(m_startupTimer.nsecsElapsed()) / 1000000.0;
        QTimer::singleShot(100, this, [this]() { runWorkloads(); });
        return;
    }

    if (!m_collectingFrames) {
        return;
    }

    const qint64 now = m_frameClock.nsecsElapsed();
    if (m_seenFrames >= m_configuration.warmupFrames) {
        m_renderSamplesMs.push_back(_renderMilliseconds);
        if (m_previousFrameNs > 0) {
            m_intervalSamplesMs.push_back(static_cast<double>(now - m_previousFrameNs) / 1000000.0);
        }
    }
    m_previousFrameNs = now;
    ++m_seenFrames;

    const int targetFrames = m_configuration.warmupFrames + m_configuration.measuredFrames;
    if (m_seenFrames >= targetFrames) {
        m_collectingFrames = false;
        m_window.sceneView()->setBenchmarkSynchronizationEnabled(false);
        QTimer::singleShot(0, this, [this]() { finish(); });
        return;
    }

    QTimer::singleShot(0, m_window.sceneView(), [this]() {
        m_window.sceneView()->requestBenchmarkFrame();
    });
}

void BenchmarkRunner::runWorkloads() {
    QElapsedTimer timer;

    timer.start();
    m_window.modelingState().resetBenchmarkScene(m_configuration.entityCount);
    m_sceneSetupMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;

    timer.restart();
    m_window.runBenchmarkModelingOperations(m_configuration.modelingOperations);
    m_modelingWorkloadMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;

    m_window.showPlotTab();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_plotWorkloadMs = m_window.runBenchmarkPlotIterations(m_configuration.plotIterations);

    m_window.showSceneTab();
    m_window.modelingState().resetBenchmarkScene(m_configuration.entityCount);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    timer.restart();
    QFile complexModel(QStringLiteral(":/benchmark/complex_surface_zlib.osgb"));
    QByteArray complexModelPayload;
    if (complexModel.open(QIODevice::ReadOnly)) {
        complexModelPayload = complexModel.readAll();
    }
    m_complexModelResourceReadMs = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    m_complexModelPayloadBytes = complexModelPayload.size();
    m_complexModelLoaded = m_window.sceneView()->loadBenchmarkOsgb(
        complexModelPayload,
        m_complexModelDeserializeMs
    );
    if (!m_complexModelLoaded) {
        qCritical() << "Failed to load the compressed OSGB benchmark model.";
    }
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QTimer::singleShot(100, this, [this]() { startFrameMeasurement(); });
}

void BenchmarkRunner::startFrameMeasurement() {
    m_seenFrames = 0;
    m_previousFrameNs = 0;
    m_renderSamplesMs.clear();
    m_intervalSamplesMs.clear();
    m_frameClock.start();
    m_collectingFrames = true;
    m_window.sceneView()->setBenchmarkSynchronizationEnabled(true);
    m_window.sceneView()->requestBenchmarkFrame();
}

void BenchmarkRunner::finish() {
    QJsonObject configuration;
    configuration.insert(QStringLiteral("entity_count"), m_configuration.entityCount);
    configuration.insert(QStringLiteral("warmup_frames"), m_configuration.warmupFrames);
    configuration.insert(QStringLiteral("measured_frames"), m_configuration.measuredFrames);
    configuration.insert(QStringLiteral("modeling_operations"), m_configuration.modelingOperations);
    configuration.insert(QStringLiteral("plot_iterations"), m_configuration.plotIterations);
    configuration.insert(QStringLiteral("viewport_width"), m_configuration.viewportWidth);
    configuration.insert(QStringLiteral("viewport_height"), m_configuration.viewportHeight);
    configuration.insert(QStringLiteral("complex_model_format"), QStringLiteral("osgb"));
    configuration.insert(QStringLiteral("complex_model_compressor"), QStringLiteral("zlib"));
    configuration.insert(QStringLiteral("complex_model_drawables"), 64);
    configuration.insert(QStringLiteral("complex_model_vertices"), 270400);
    configuration.insert(QStringLiteral("complex_model_triangles"), 524288);

    QJsonObject runtime;
    runtime.insert(QStringLiteral("target"), targetName());
    runtime.insert(QStringLiteral("qt_version"), QString::fromLatin1(qVersion()));
    runtime.insert(QStringLiteral("osg_version"), QString::fromLatin1(osgGetVersion()));
    runtime.insert(QStringLiteral("kernel_type"), QSysInfo::kernelType());
    runtime.insert(QStringLiteral("kernel_version"), QSysInfo::kernelVersion());
    runtime.insert(QStringLiteral("cpu_architecture"), QSysInfo::currentCpuArchitecture());
    runtime.insert(QStringLiteral("gl_vendor"), m_window.sceneView()->glVendor());
    runtime.insert(QStringLiteral("gl_renderer"), m_window.sceneView()->glRenderer());
    runtime.insert(QStringLiteral("gl_version"), m_window.sceneView()->glVersion());
    runtime.insert(QStringLiteral("viewport_width"), m_window.sceneView()->width());
    runtime.insert(QStringLiteral("viewport_height"), m_window.sceneView()->height());

    QJsonObject metrics;
    metrics.insert(QStringLiteral("startup_to_first_frame_ms"), m_startupToFirstFrameMs);
    metrics.insert(QStringLiteral("scene_setup_ms"), m_sceneSetupMs);
    metrics.insert(QStringLiteral("modeling_workload_total_ms"), m_modelingWorkloadMs);
    metrics.insert(QStringLiteral("modeling_operation_mean_ms"), m_modelingWorkloadMs / m_configuration.modelingOperations);
    metrics.insert(QStringLiteral("plot_workload_total_ms"), m_plotWorkloadMs);
    metrics.insert(QStringLiteral("plot_iteration_mean_ms"), m_plotWorkloadMs / m_configuration.plotIterations);
    metrics.insert(QStringLiteral("complex_model_resource_read_ms"), m_complexModelResourceReadMs);
    metrics.insert(QStringLiteral("complex_model_deserialize_ms"), m_complexModelDeserializeMs);
    metrics.insert(QStringLiteral("complex_model_payload_bytes"), static_cast<double>(m_complexModelPayloadBytes));
    metrics.insert(QStringLiteral("complex_model_loaded"), m_complexModelLoaded);
    metrics.insert(QStringLiteral("frame_render_ms"), statistics(m_renderSamplesMs));
    metrics.insert(QStringLiteral("frame_interval_ms"), statistics(m_intervalSamplesMs));
    metrics.insert(QStringLiteral("memory_footprint_bytes"), static_cast<double>(memoryFootprintBytes()));

    QJsonObject result;
    result.insert(QStringLiteral("schema_version"), 1);
    result.insert(QStringLiteral("benchmark_version"), QStringLiteral("2.1.0"));
    result.insert(QStringLiteral("timestamp_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    result.insert(QStringLiteral("run_id"), m_configuration.runId);
    result.insert(QStringLiteral("configuration"), configuration);
    result.insert(QStringLiteral("runtime"), runtime);
    result.insert(QStringLiteral("metrics"), metrics);

    const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
    qInfo().noquote() << QStringLiteral("BENCHMARK_RESULT ") + QString::fromUtf8(json);

    if (!m_configuration.outputPath.isEmpty()) {
        QSaveFile output(m_configuration.outputPath);
        if (output.open(QIODevice::WriteOnly | QIODevice::Text)) {
            output.write(json);
            output.write("\n");
            if (!output.commit()) {
                qCritical().noquote() << QStringLiteral("Failed to commit benchmark result: %1").arg(m_configuration.outputPath);
            }
        }
        else {
            qCritical().noquote() << QStringLiteral("Failed to open benchmark result: %1").arg(m_configuration.outputPath);
        }
    }

    QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
}

} // namespace prototype
