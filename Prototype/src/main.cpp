// @otlicense
// File: main.cpp
// @otlicense-end

#include "BenchmarkRunner.h"
#include "ModelingWindow.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QSurfaceFormat>

#if defined(OPENTWIN_OSG_ENABLE_STATIC_OSGDB_PLUGINS)
#include <osgDB/Registry>

USE_OSGPLUGIN(jpeg)
USE_OSGPLUGIN(osg2)
USE_COMPRESSOR_WRAPPER(ZLibCompressor)
USE_SERIALIZER_WRAPPER_LIBRARY(osg)
USE_SERIALIZER_WRAPPER_LIBRARY(osgAnimation)
USE_SERIALIZER_WRAPPER_LIBRARY(osgFX)
USE_SERIALIZER_WRAPPER_LIBRARY(osgManipulator)
USE_SERIALIZER_WRAPPER_LIBRARY(osgParticle)
USE_SERIALIZER_WRAPPER_LIBRARY(osgShadow)
USE_SERIALIZER_WRAPPER_LIBRARY(osgSim)
USE_SERIALIZER_WRAPPER_LIBRARY(osgTerrain)
USE_SERIALIZER_WRAPPER_LIBRARY(osgText)
USE_SERIALIZER_WRAPPER_LIBRARY(osgVolume)
#endif

int main(int argc, char* argv[]) {
    QElapsedTimer startupTimer;
    startupTimer.start();

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(0);
#if defined(Q_OS_WASM)
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
#else
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::NoProfile);
#endif
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    const prototype::BenchmarkConfiguration benchmark = prototype::benchmarkConfiguration(app);
    prototype::ModelingWindow window;
    if (benchmark.enabled) {
        window.configureBenchmarkViewport(benchmark.viewportWidth, benchmark.viewportHeight);
    }
    prototype::BenchmarkRunner benchmarkRunner(window, benchmark, startupTimer);
    window.show();

    return app.exec();
}
