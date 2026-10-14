// @otlicense
// File: generate-complex-model.cpp
// @otlicense-end

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/PrimitiveSet>
#include <osgDB/Options>
#include <osgDB/Registry>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

USE_OSGPLUGIN(osg2)
USE_SERIALIZER_WRAPPER_LIBRARY(osg)
USE_COMPRESSOR_WRAPPER(ZLibCompressor)

namespace {

constexpr int TileCountPerAxis = 8;
constexpr int CellsPerTileAxis = 64;
constexpr double VertexSpacing = 0.05;

osg::ref_ptr<osg::Geometry> createTile(int _tileX, int _tileY) {
    constexpr int verticesPerAxis = CellsPerTileAxis + 1;
    constexpr int vertexCount = verticesPerAxis * verticesPerAxis;
    constexpr int indexCount = CellsPerTileAxis * CellsPerTileAxis * 6;

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(GL_TRIANGLES);
    vertices->reserve(vertexCount);
    colors->reserve(vertexCount);
    indices->reserve(indexCount);

    constexpr double fullWidth = TileCountPerAxis * CellsPerTileAxis * VertexSpacing;
    for (int localY = 0; localY < verticesPerAxis; ++localY) {
        for (int localX = 0; localX < verticesPerAxis; ++localX) {
            const int gridX = _tileX * CellsPerTileAxis + localX;
            const int gridY = _tileY * CellsPerTileAxis + localY;
            const double x = gridX * VertexSpacing - fullWidth * 0.5;
            const double y = gridY * VertexSpacing - fullWidth * 0.5;
            const double z = 0.9 * std::sin(x * 0.42) * std::cos(y * 0.37)
                + 0.2 * std::sin((x + y) * 1.25);
            const float normalizedHeight = static_cast<float>((z + 1.1) / 2.2);
            vertices->push_back(osg::Vec3(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(z)
            ));
            colors->push_back(osg::Vec4(
                0.12f + normalizedHeight * 0.18f,
                0.34f + normalizedHeight * 0.42f,
                0.72f - normalizedHeight * 0.28f,
                1.0f
            ));
        }
    }

    for (int y = 0; y < CellsPerTileAxis; ++y) {
        for (int x = 0; x < CellsPerTileAxis; ++x) {
            const std::uint32_t lowerLeft = static_cast<std::uint32_t>(y * verticesPerAxis + x);
            const std::uint32_t lowerRight = lowerLeft + 1;
            const std::uint32_t upperLeft = lowerLeft + verticesPerAxis;
            const std::uint32_t upperRight = upperLeft + 1;
            indices->push_back(lowerLeft);
            indices->push_back(lowerRight);
            indices->push_back(upperRight);
            indices->push_back(lowerLeft);
            indices->push_back(upperRight);
            indices->push_back(upperLeft);
        }
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(indices.get());
    return geometry;
}

osg::ref_ptr<osg::Node> createModel() {
    osg::ref_ptr<osg::Group> root = new osg::Group;
    root->setName("OpenTwin benchmark complex surface");

    for (int tileY = 0; tileY < TileCountPerAxis; ++tileY) {
        for (int tileX = 0; tileX < TileCountPerAxis; ++tileX) {
            osg::ref_ptr<osg::Geode> geode = new osg::Geode;
            geode->setName(
                "Surface tile " + std::to_string(tileX) + " " + std::to_string(tileY)
            );
            geode->addDrawable(createTile(tileX, tileY).get());
            root->addChild(geode.get());
        }
    }
    return root;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: OpenTwinBenchmarkModelGenerator OUTPUT.osgb\n";
        return 2;
    }

    osgDB::ReaderWriter* readerWriter = osgDB::Registry::instance()->getReaderWriterForExtension("osgb");
    if (!readerWriter) {
        std::cerr << "The static osg2 plugin is not registered.\n";
        return 3;
    }

    osg::ref_ptr<osg::Node> model = createModel();
    osg::ref_ptr<osgDB::Options> options = new osgDB::Options("Compressor=zlib");
    const osgDB::ReaderWriter::WriteResult result = readerWriter->writeNode(*model, argv[1], options.get());
    if (!result.success()) {
        std::cerr << "Failed to write compressed OSGB model: " << result.message() << '\n';
        return 4;
    }

    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "Generated model cannot be reopened.\n";
        return 5;
    }

    constexpr std::uint64_t drawableCount = TileCountPerAxis * TileCountPerAxis;
    constexpr std::uint64_t vertexCount = drawableCount * (CellsPerTileAxis + 1) * (CellsPerTileAxis + 1);
    constexpr std::uint64_t triangleCount = drawableCount * CellsPerTileAxis * CellsPerTileAxis * 2;
    std::cout
        << "Generated " << argv[1]
        << " bytes=" << input.tellg()
        << " drawables=" << drawableCount
        << " vertices=" << vertexCount
        << " triangles=" << triangleCount
        << " compressor=zlib\n";
    return 0;
}
