cmake_minimum_required(VERSION 3.21)

project(OpenTwinPrototype
        VERSION 0.1.0
        DESCRIPTION "OpenTwin Modeling tab proof of concept using Qt WebAssembly and OpenSceneGraph"
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

get_filename_component(OPENTWIN_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(OPENTWIN_DEVELOPMENT_ROOT "${OPENTWIN_REPOSITORY_ROOT}/.." ABSOLUTE)

set(OPENTWIN_THIRDPARTY_ROOT "" CACHE PATH
        "Path to an OpenTwin ThirdParty checkout containing OpenSceneGraph and Qwt"
)
if(NOT OPENTWIN_THIRDPARTY_ROOT)
    foreach(_opentwin_thirdparty_candidate
            "${OPENTWIN_REPOSITORY_ROOT}/ThirdParty"
            "${OPENTWIN_DEVELOPMENT_ROOT}/ThirdParty"
    )
        if(EXISTS "${_opentwin_thirdparty_candidate}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3"
                AND EXISTS "${_opentwin_thirdparty_candidate}/qwt/qwt-6.2.0")
            set(OPENTWIN_THIRDPARTY_ROOT "${_opentwin_thirdparty_candidate}")
            break()
        endif()
    endforeach()
endif()

set(OPENTWIN_OSG_ROOT
        "${OPENTWIN_THIRDPARTY_ROOT}/OpenSceneGraph/OpenSceneGraph-OpenSceneGraph-3.6.3"
        CACHE PATH
        "Path to the local OpenSceneGraph 3.6.3 source tree"
)

set(OPENTWIN_QWT_ROOT
        "${OPENTWIN_THIRDPARTY_ROOT}/qwt/qwt-6.2.0"
        CACHE PATH
        "Path to the local Qwt source tree used by the OpenTwin plotting prototype"
)

if(NOT EXISTS "${OPENTWIN_OSG_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
            "OpenSceneGraph 3.6.3 was not found at '${OPENTWIN_OSG_ROOT}'. "
            "Set OPENTWIN_THIRDPARTY_ROOT or OPENTWIN_OSG_ROOT."
    )
endif()
if(NOT EXISTS "${OPENTWIN_QWT_ROOT}/src/qwt.cpp")
    message(FATAL_ERROR
            "Qwt 6.2.0 was not found at '${OPENTWIN_QWT_ROOT}'. "
            "Set OPENTWIN_THIRDPARTY_ROOT or OPENTWIN_QWT_ROOT."
    )
endif()

if(EMSCRIPTEN)
    set(_opentwin_default_osg_build_dir "${CMAKE_CURRENT_SOURCE_DIR}/build/osg-wasm")
else()
    set(_opentwin_default_osg_build_dir "${CMAKE_CURRENT_SOURCE_DIR}/build/osg-macos")
endif()

set(OPENTWIN_OSG_BUILD_DIR
        "${_opentwin_default_osg_build_dir}"
        CACHE PATH
        "Path to the OpenSceneGraph build directory generated for this platform"
)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets OpenGL PrintSupport Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL PrintSupport Concurrent)
find_package(Threads REQUIRED)

set(_opentwin_qwt_src_dir "${OPENTWIN_QWT_ROOT}/src")
set(_opentwin_qwt_sources)
set(_opentwin_qwt_headers)

foreach(_qwt_source
        qwt.cpp
        qwt_abstract_scale_draw.cpp
        qwt_bezier.cpp
        qwt_clipper.cpp
        qwt_color_map.cpp
        qwt_column_symbol.cpp
        qwt_date.cpp
        qwt_date_scale_draw.cpp
        qwt_date_scale_engine.cpp
        qwt_dyngrid_layout.cpp
        qwt_event_pattern.cpp
        qwt_graphic.cpp
        qwt_interval.cpp
        qwt_interval_symbol.cpp
        qwt_math.cpp
        qwt_magnifier.cpp
        qwt_null_paintdevice.cpp
        qwt_painter.cpp
        qwt_painter_command.cpp
        qwt_panner.cpp
        qwt_picker.cpp
        qwt_picker_machine.cpp
        qwt_pixel_matrix.cpp
        qwt_point_3d.cpp
        qwt_point_polar.cpp
        qwt_round_scale_draw.cpp
        qwt_scale_div.cpp
        qwt_scale_draw.cpp
        qwt_scale_map.cpp
        qwt_scale_engine.cpp
        qwt_spline.cpp
        qwt_spline_basis.cpp
        qwt_spline_parametrization.cpp
        qwt_spline_local.cpp
        qwt_spline_cubic.cpp
        qwt_spline_pleasing.cpp
        qwt_spline_polynomial.cpp
        qwt_symbol.cpp
        qwt_system_clock.cpp
        qwt_text_engine.cpp
        qwt_text_label.cpp
        qwt_text.cpp
        qwt_transform.cpp
        qwt_widget_overlay.cpp
        qwt_curve_fitter.cpp
        qwt_spline_curve_fitter.cpp
        qwt_weeding_curve_fitter.cpp
        qwt_abstract_legend.cpp
        qwt_legend.cpp
        qwt_legend_data.cpp
        qwt_legend_label.cpp
        qwt_plot.cpp
        qwt_plot_renderer.cpp
        qwt_plot_axis.cpp
        qwt_plot_curve.cpp
        qwt_plot_dict.cpp
        qwt_plot_directpainter.cpp
        qwt_plot_graphicitem.cpp
        qwt_plot_grid.cpp
        qwt_plot_histogram.cpp
        qwt_plot_item.cpp
        qwt_plot_abstract_barchart.cpp
        qwt_plot_barchart.cpp
        qwt_plot_multi_barchart.cpp
        qwt_plot_intervalcurve.cpp
        qwt_plot_zoneitem.cpp
        qwt_plot_tradingcurve.cpp
        qwt_plot_spectrogram.cpp
        qwt_plot_spectrocurve.cpp
        qwt_plot_scaleitem.cpp
        qwt_plot_legenditem.cpp
        qwt_plot_seriesitem.cpp
        qwt_plot_shapeitem.cpp
        qwt_plot_vectorfield.cpp
        qwt_plot_marker.cpp
        qwt_plot_textlabel.cpp
        qwt_plot_layout.cpp
        qwt_plot_abstract_canvas.cpp
        qwt_plot_canvas.cpp
        qwt_plot_panner.cpp
        qwt_plot_rasteritem.cpp
        qwt_plot_picker.cpp
        qwt_plot_zoomer.cpp
        qwt_plot_magnifier.cpp
        qwt_plot_rescaler.cpp
        qwt_point_mapper.cpp
        qwt_raster_data.cpp
        qwt_matrix_raster_data.cpp
        qwt_vectorfield_symbol.cpp
        qwt_sampling_thread.cpp
        qwt_series_data.cpp
        qwt_point_data.cpp
        qwt_scale_widget.cpp
)
    list(APPEND _opentwin_qwt_sources "${_opentwin_qwt_src_dir}/${_qwt_source}")
endforeach()

foreach(_qwt_header
        qwt.h
        qwt_abstract_scale_draw.h
        qwt_bezier.h
        qwt_clipper.h
        qwt_color_map.h
        qwt_column_symbol.h
        qwt_date.h
        qwt_date_scale_draw.h
        qwt_date_scale_engine.h
        qwt_dyngrid_layout.h
        qwt_event_pattern.h
        qwt_global.h
        qwt_graphic.h
        qwt_interval.h
        qwt_interval_symbol.h
        qwt_math.h
        qwt_magnifier.h
        qwt_null_paintdevice.h
        qwt_painter.h
        qwt_painter_command.h
        qwt_panner.h
        qwt_picker.h
        qwt_picker_machine.h
        qwt_pixel_matrix.h
        qwt_point_3d.h
        qwt_point_polar.h
        qwt_round_scale_draw.h
        qwt_scale_div.h
        qwt_scale_draw.h
        qwt_scale_engine.h
        qwt_scale_map.h
        qwt_spline.h
        qwt_spline_basis.h
        qwt_spline_parametrization.h
        qwt_spline_local.h
        qwt_spline_cubic.h
        qwt_spline_pleasing.h
        qwt_spline_polynomial.h
        qwt_symbol.h
        qwt_system_clock.h
        qwt_text_engine.h
        qwt_text_label.h
        qwt_text.h
        qwt_transform.h
        qwt_widget_overlay.h
        qwt_axis.h
        qwt_axis_id.h
        qwt_curve_fitter.h
        qwt_spline_curve_fitter.h
        qwt_weeding_curve_fitter.h
        qwt_abstract_legend.h
        qwt_legend.h
        qwt_legend_data.h
        qwt_legend_label.h
        qwt_plot.h
        qwt_plot_renderer.h
        qwt_plot_curve.h
        qwt_plot_dict.h
        qwt_plot_directpainter.h
        qwt_plot_graphicitem.h
        qwt_plot_grid.h
        qwt_plot_histogram.h
        qwt_plot_item.h
        qwt_plot_abstract_barchart.h
        qwt_plot_barchart.h
        qwt_plot_multi_barchart.h
        qwt_plot_intervalcurve.h
        qwt_plot_tradingcurve.h
        qwt_plot_layout.h
        qwt_plot_marker.h
        qwt_plot_zoneitem.h
        qwt_plot_textlabel.h
        qwt_plot_rasteritem.h
        qwt_plot_spectrogram.h
        qwt_plot_spectrocurve.h
        qwt_plot_scaleitem.h
        qwt_plot_legenditem.h
        qwt_plot_seriesitem.h
        qwt_plot_shapeitem.h
        qwt_plot_vectorfield.h
        qwt_plot_abstract_canvas.h
        qwt_plot_canvas.h
        qwt_plot_panner.h
        qwt_plot_picker.h
        qwt_plot_zoomer.h
        qwt_plot_magnifier.h
        qwt_plot_rescaler.h
        qwt_point_mapper.h
        qwt_raster_data.h
        qwt_matrix_raster_data.h
        qwt_vectorfield_symbol.h
        qwt_sampling_thread.h
        qwt_samples.h
        qwt_series_data.h
        qwt_series_store.h
        qwt_point_data.h
        qwt_scale_widget.h
)
    list(APPEND _opentwin_qwt_headers "${_opentwin_qwt_src_dir}/${_qwt_header}")
endforeach()

add_library(opentwin_prototype_qwt STATIC
        ${_opentwin_qwt_sources}
        ${_opentwin_qwt_headers}
)

target_compile_definitions(opentwin_prototype_qwt
        PUBLIC
        QWT_MOC_INCLUDE=1
        QWT_NO_SVG
        QWT_NO_OPENGL
)

target_include_directories(opentwin_prototype_qwt
        PUBLIC
        "${_opentwin_qwt_src_dir}"
)

target_link_libraries(opentwin_prototype_qwt
        PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::PrintSupport
        Qt${QT_VERSION_MAJOR}::Concurrent
)

set(PROTOTYPE_SOURCES
        src/BenchmarkRunner.cpp
        src/BenchmarkRunner.h
        src/main.cpp
        src/PlotWidget.cpp
        src/PlotWidget.h
        src/ModelingState.cpp
        src/ModelingState.h
        src/ModelingServiceAdapter.cpp
        src/ModelingServiceAdapter.h
        src/ModelingWindow.cpp
        src/ModelingWindow.h
        src/OsgModelingView.cpp
        src/OsgModelingView.h
        src/VersionTimeline.cpp
        src/VersionTimeline.h
        src/stdafx.h
        resources.qrc
        ${OPENTWIN_REPOSITORY_ROOT}/Libraries/Viewer/src/ViewManipulator.cpp
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(OpenTwinPrototype
            MANUAL_FINALIZATION
            ${PROTOTYPE_SOURCES}
    )
else()
    add_executable(OpenTwinPrototype ${PROTOTYPE_SOURCES})
endif()

set(_opentwin_osg_lib_dir "${OPENTWIN_OSG_BUILD_DIR}/lib")
set(_opentwin_required_osg_libs
        osgdb_jpeg
        osgdb_osg
        osgdb_serializers_osg
        osgdb_serializers_osganimation
        osgdb_serializers_osgfx
        osgdb_serializers_osgga
        osgdb_serializers_osgmanipulator
        osgdb_serializers_osgparticle
        osgdb_serializers_osgshadow
        osgdb_serializers_osgsim
        osgdb_serializers_osgterrain
        osgdb_serializers_osgtext
        osgdb_serializers_osgui
        osgdb_serializers_osgutil
        osgdb_serializers_osgviewer
        osgdb_serializers_osgvolume
        osgViewer
        osgWidget
        osgPresentation
        osgVolume
        osgTerrain
        osgShadow
        osgParticle
        osgManipulator
        osgFX
        osgAnimation
        osgSim
        osgUI
        osgDB
        osgGA
        osgText
        osgUtil
        osg
        OpenThreads
)

set(_opentwin_osg_link_libraries)
foreach(_lib IN LISTS _opentwin_required_osg_libs)
    set(OPENTWIN_${_lib}_LIBRARY "${_opentwin_osg_lib_dir}/lib${_lib}.a")
    if(NOT EXISTS "${OPENTWIN_${_lib}_LIBRARY}")
        message(FATAL_ERROR
                "Missing OSG library '${_lib}' in ${_opentwin_osg_lib_dir}. "
                "Run scripts/build-osg-wasm.sh or scripts/build-native-macos.sh first."
        )
    endif()
    list(APPEND _opentwin_osg_link_libraries "${OPENTWIN_${_lib}_LIBRARY}")
endforeach()

target_include_directories(OpenTwinPrototype
        PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${OPENTWIN_OSG_BUILD_DIR}/include"
        "${OPENTWIN_OSG_ROOT}/include"
        "${OPENTWIN_REPOSITORY_ROOT}/Libraries/Viewer/include"
)

target_link_libraries(OpenTwinPrototype
        PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Threads::Threads
        opentwin_prototype_qwt
        ${_opentwin_osg_link_libraries}
)

target_compile_definitions(OpenTwinPrototype
        PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        OPENTWIN_OSG_ENABLE_STATIC_OSGDB_PLUGINS=1
)

if(EMSCRIPTEN)
    target_compile_definitions(OpenTwinPrototype PRIVATE OPENTWIN_OSG_WASM=1)
    set(OPENTWIN_EM_CACHE
            "${CMAKE_CURRENT_SOURCE_DIR}/build/emscripten-cache"
            CACHE PATH
            "Emscripten cache containing the libjpeg port used by the OSG JPEG plugin"
    )
    set(OPENTWIN_EMSCRIPTEN_LIBJPEG
            "${OPENTWIN_EM_CACHE}/sysroot/lib/wasm32-emscripten/libjpeg.a"
            CACHE FILEPATH
            "Emscripten libjpeg archive used by osgdb_jpeg"
    )
    if(NOT EXISTS "${OPENTWIN_EMSCRIPTEN_LIBJPEG}")
        message(FATAL_ERROR
                "Missing Emscripten libjpeg archive at ${OPENTWIN_EMSCRIPTEN_LIBJPEG}. "
                "Run scripts/build-osg-wasm.sh first so the libjpeg port is populated."
        )
    endif()
    set(OPENTWIN_EMSCRIPTEN_ZLIB
            "${OPENTWIN_EM_CACHE}/sysroot/lib/wasm32-emscripten/libz.a"
            CACHE FILEPATH
            "Emscripten zlib archive used by compressed OSG binary payloads"
    )
    if(NOT EXISTS "${OPENTWIN_EMSCRIPTEN_ZLIB}")
        message(FATAL_ERROR
                "Missing Emscripten zlib archive at ${OPENTWIN_EMSCRIPTEN_ZLIB}. "
                "Run scripts/build-osg-wasm.sh first so the zlib port is populated."
        )
    endif()
    target_link_libraries(OpenTwinPrototype PRIVATE GL)
    target_link_libraries(OpenTwinPrototype PRIVATE
            "${OPENTWIN_EMSCRIPTEN_LIBJPEG}"
            "${OPENTWIN_EMSCRIPTEN_ZLIB}"
    )
    target_link_options(OpenTwinPrototype PRIVATE
            "SHELL:-sINITIAL_MEMORY=256MB"
            "SHELL:-sALLOW_MEMORY_GROWTH=1"
            "SHELL:-sMIN_WEBGL_VERSION=2"
            "SHELL:-sMAX_WEBGL_VERSION=2"
    )
else()
    find_package(JPEG REQUIRED)
    find_package(ZLIB REQUIRED)
    if(APPLE)
        find_library(OPENTWIN_APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(OPENTWIN_OPENGL_FRAMEWORK OpenGL REQUIRED)
        if(TARGET JPEG::JPEG)
            target_link_libraries(OpenTwinPrototype PRIVATE JPEG::JPEG)
        else()
            target_link_libraries(OpenTwinPrototype PRIVATE ${JPEG_LIBRARIES})
        endif()
        target_link_libraries(OpenTwinPrototype PRIVATE
                ${OPENTWIN_APPKIT_FRAMEWORK}
                ${OPENTWIN_OPENGL_FRAMEWORK}
                ZLIB::ZLIB
        )

        add_executable(OpenTwinBenchmarkModelGenerator EXCLUDE_FROM_ALL
                benchmark/generate-complex-model.cpp
        )
        target_include_directories(OpenTwinBenchmarkModelGenerator PRIVATE
                "${OPENTWIN_OSG_BUILD_DIR}/include"
                "${OPENTWIN_OSG_ROOT}/include"
        )
        target_compile_definitions(OpenTwinBenchmarkModelGenerator PRIVATE
                OPENTWIN_OSG_ENABLE_STATIC_OSGDB_PLUGINS=1
        )
        target_link_libraries(OpenTwinBenchmarkModelGenerator PRIVATE
                ${_opentwin_osg_link_libraries}
                JPEG::JPEG
                ZLIB::ZLIB
                ${OPENTWIN_APPKIT_FRAMEWORK}
                ${OPENTWIN_OPENGL_FRAMEWORK}
        )
    endif()
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(OpenTwinPrototype)
endif()
