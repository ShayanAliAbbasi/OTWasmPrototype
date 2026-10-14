// @otlicense
// File: OsgModelingView.cpp
// @otlicense-end

#include "OsgModelingView.h"

#include "ModelingState.h"
#include "ViewManipulator.h"

#include <QExposeEvent>
#include <QByteArray>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/Geode>
#include <osg/LineWidth>
#include <osg/Program>
#include <osg/Shader>
#include <osg/StateSet>
#include <osg/Viewport>
#include <osgGA/EventQueue>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>
#include <osgDB/ReaderWriter>
#include <osgDB/Registry>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace prototype {
namespace {

constexpr double Pi = 3.14159265358979323846;

QColor mixColor(const QColor& _a, const QColor& _b, double _t) {
    const double inv = 1.0 - _t;
    return QColor::fromRgbF(
        _a.redF() * inv + _b.redF() * _t,
        _a.greenF() * inv + _b.greenF() * _t,
        _a.blueF() * inv + _b.blueF() * _t,
        _a.alphaF() * inv + _b.alphaF() * _t
    );
}

void enableWebGeometry(osg::Geometry& _geometry) {
    _geometry.setUseDisplayList(false);
    _geometry.setUseVertexBufferObjects(true);
}

osg::Vec3 normalFromTriangle(const osg::Vec3& _a, const osg::Vec3& _b, const osg::Vec3& _c) {
    osg::Vec3 normal = (_b - _a) ^ (_c - _a);
    if (normal.length2() <= 0.000001f) {
        return osg::Vec3(0.0f, 0.0f, 1.0f);
    }
    normal.normalize();
    return normal;
}

} // namespace

OsgModelingView::OsgModelingView(QWindow* _parent)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate, _parent)
{
    setTitle(QStringLiteral("OpenTwin OSG Modeling View"));
    setCursor(Qt::CrossCursor);
}

OsgModelingView::~OsgModelingView() {
    m_viewer = nullptr;
    m_camera = nullptr;
    m_graphicsWindow = nullptr;
}

void OsgModelingView::setState(ModelingState* _state) {
    if (m_state) {
        disconnect(m_state, nullptr, this, nullptr);
    }

    m_state = _state;

    if (m_state) {
        connect(m_state, &ModelingState::sceneChanged, this, &OsgModelingView::rebuildScene);
        connect(m_state, &ModelingState::selectionChanged, this, &OsgModelingView::rebuildScene);
    }

    resetView();
    rebuildScene();
}

void OsgModelingView::requestBenchmarkFrame() {
    if (m_cameraManipulator) {
        osg::Vec3d eye;
        osg::Vec3d center;
        osg::Vec3d up;
        m_cameraManipulator->getTransformation(eye, center, up);
        const osg::Quat rotation(0.35 * Pi / 180.0, osg::Vec3d(0.0, 0.0, 1.0));
        eye = center + rotation * (eye - center);
        m_cameraManipulator->setTransformation(eye, center, up);
    }
    requestFrame();
}

void OsgModelingView::setBenchmarkSynchronizationEnabled(bool _enabled) {
    m_benchmarkSynchronizationEnabled = _enabled;
}

bool OsgModelingView::loadBenchmarkOsgb(const QByteArray& _payload, double& _deserializeMilliseconds) {
    _deserializeMilliseconds = 0.0;
    osgDB::ReaderWriter* readerWriter = osgDB::Registry::instance()->getReaderWriterForExtension("osgb");
    if (!readerWriter || _payload.isEmpty()) {
        return false;
    }

    const std::string bytes(_payload.constData(), static_cast<std::size_t>(_payload.size()));
    std::istringstream input(bytes, std::ios::in | std::ios::binary);
    QElapsedTimer timer;
    timer.start();
    osgDB::ReaderWriter::ReadResult result = readerWriter->readNode(input);
    _deserializeMilliseconds = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    if (!result.success()) {
        return false;
    }

    m_benchmarkSceneNode = result.takeNode();
    if (!m_benchmarkSceneNode) {
        return false;
    }

    m_root = new osg::Group;
    configureRootState();
    m_root->addChild(m_benchmarkSceneNode);
    m_axisOverlayCamera = createAxisOverlay();
    m_root->addChild(m_axisOverlayCamera);
    if (m_viewer) {
        m_viewer->setSceneData(m_root);
    }
    resetView();
    requestFrame();
    return true;
}

void OsgModelingView::resetView() {
    if (!m_cameraManipulator) {
        return;
    }

    const osg::Vec3d center = modelCenter();
    const double distance = std::max(6.0, modelRadius() * 5.0);
    const osg::Vec3d eye = center + osg::Vec3d(distance * 0.55, -distance * 0.75, distance * 0.48);

    m_cameraManipulator->setTransformation(eye, center, osg::Vec3d(0.0, 0.0, 1.0));
    m_cameraManipulator->setMinimumDistance(0.05, true);
    requestFrame();
}

void OsgModelingView::initializeGL() {
    initializeViewer();
}

void OsgModelingView::resizeGL(int _width, int _height) {
    if (!m_graphicsWindow || !m_camera) {
        return;
    }

    const QPointF scaled = scaledPosition(QPointF(_width, _height));
    const int pixelWidth = std::max(1, static_cast<int>(std::round(scaled.x())));
    const int pixelHeight = std::max(1, static_cast<int>(std::round(scaled.y())));

    eventQueue()->windowResize(0, 0, pixelWidth, pixelHeight);
    m_graphicsWindow->resized(0, 0, pixelWidth, pixelHeight);
    m_camera->setViewport(0, 0, pixelWidth, pixelHeight);
    updateProjection();
    requestFrame();
}

void OsgModelingView::paintGL() {
    if (!m_viewer || !m_camera || !m_cameraManipulator) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    m_cameraManipulator->updateCamera(*m_camera);
    updateAxisOverlay();
    m_viewer->frame();
    if (m_benchmarkSynchronizationEnabled) {
        glFinish();
    }

    if (m_glVendor.isEmpty()) {
        const GLubyte* vendor = glGetString(GL_VENDOR);
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);
        m_glVendor = vendor ? QString::fromLatin1(reinterpret_cast<const char*>(vendor)) : QStringLiteral("unknown");
        m_glRenderer = renderer ? QString::fromLatin1(reinterpret_cast<const char*>(renderer)) : QStringLiteral("unknown");
        m_glVersion = version ? QString::fromLatin1(reinterpret_cast<const char*>(version)) : QStringLiteral("unknown");
    }

    emit frameRendered(static_cast<double>(timer.nsecsElapsed()) / 1000000.0);
}

void OsgModelingView::exposeEvent(QExposeEvent* _event) {
    QOpenGLWindow::exposeEvent(_event);
    if (isExposed()) {
        requestFrame();
    }
}

void OsgModelingView::mouseMoveEvent(QMouseEvent* _event) {
    if (!eventQueue()) {
        return;
    }

    const QPointF pos = scaledPosition(_event->position());
    eventQueue()->mouseMotion(pos.x(), pos.y());
    requestFrame();
}

void OsgModelingView::mouseDoubleClickEvent(QMouseEvent* _event) {
    if (!eventQueue()) {
        return;
    }

    const QPointF pos = scaledPosition(_event->position());
    eventQueue()->mouseDoubleButtonPress(pos.x(), pos.y(), qtButtonToOsgButton(_event->button()));

    if (_event->button() == Qt::LeftButton) {
        pickEntity(_event->position());
    }
    else if (_event->button() == Qt::RightButton && pickEntity(_event->position()) && m_cameraManipulator && m_state) {
        const SceneEntity* entity = m_state->selectedEntity();
        if (entity) {
            m_cameraManipulator->setCenter(entityCenter(*entity));
        }
    }

    requestFrame();
}

void OsgModelingView::mousePressEvent(QMouseEvent* _event) {
    if (!eventQueue()) {
        return;
    }

    requestActivate();

    const QPointF pos = scaledPosition(_event->position());
    const int button = qtButtonToOsgButton(_event->button());

    if (_event->button() == Qt::LeftButton) {
        m_leftButtonDown = true;
    }
    else if (_event->button() == Qt::MiddleButton) {
        m_middleButtonDown = true;
    }
    else if (_event->button() == Qt::RightButton) {
        m_rightButtonDown = true;
    }

    eventQueue()->mouseButtonPress(pos.x(), pos.y(), button);
    requestFrame();
}

void OsgModelingView::mouseReleaseEvent(QMouseEvent* _event) {
    if (!eventQueue()) {
        return;
    }

    const QPointF pos = scaledPosition(_event->position());
    const int button = qtButtonToOsgButton(_event->button());

    if (_event->button() == Qt::LeftButton) {
        m_leftButtonDown = false;
    }
    else if (_event->button() == Qt::MiddleButton) {
        m_middleButtonDown = false;
    }
    else if (_event->button() == Qt::RightButton) {
        m_rightButtonDown = false;
    }

    eventQueue()->mouseButtonRelease(pos.x(), pos.y(), button);
    requestFrame();
}

void OsgModelingView::wheelEvent(QWheelEvent* _event) {
    if (!eventQueue() || m_middleButtonDown) {
        return;
    }

    const int delta = _event->angleDelta().y();
    const osgGA::GUIEventAdapter::ScrollingMotion motion =
        delta > 0 ? osgGA::GUIEventAdapter::SCROLL_DOWN : osgGA::GUIEventAdapter::SCROLL_UP;
    eventQueue()->mouseScroll(motion);
    requestFrame();
}

void OsgModelingView::keyPressEvent(QKeyEvent* _event) {
    if (_event->key() == Qt::Key_Space) {
        resetView();
        return;
    }

    if (m_state && _event->key() == Qt::Key_Escape) {
        m_state->cancelActiveInteraction();
        return;
    }
    if (m_state && (_event->key() == Qt::Key_Return || _event->key() == Qt::Key_Enter)) {
        m_state->acceptActiveInteraction();
        return;
    }

    if (eventQueue()) {
        eventQueue()->keyPress(_event->key());
    }
    requestFrame();
}

void OsgModelingView::keyReleaseEvent(QKeyEvent* _event) {
    if (eventQueue()) {
        eventQueue()->keyRelease(_event->key());
    }
    requestFrame();
}

void OsgModelingView::rebuildScene() {
    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    const bool restoreView = m_cameraManipulator.valid();
    if (restoreView) {
        m_cameraManipulator->getTransformation(eye, center, up);
    }

    m_benchmarkSceneNode = nullptr;
    if (!m_root) {
        m_root = new osg::Group;
        configureRootState();
    }

    m_root->removeChildren(0, m_root->getNumChildren());
    m_root->addChild(createWorkingPlane());

    if (m_state) {
        for (const SceneEntity& entity : m_state->entities()) {
            m_root->addChild(createEntityNode(entity));
        }
    }

    m_axisOverlayCamera = createAxisOverlay();
    m_root->addChild(m_axisOverlayCamera);

    if (m_viewer && m_viewer->getSceneData() != m_root.get()) {
        m_viewer->setSceneData(m_root);
    }

    if (restoreView) {
        m_cameraManipulator->setTransformation(eye, center, up);
    }

    requestFrame();
}

void OsgModelingView::initializeViewer() {
    if (m_initialized) {
        return;
    }

    const int pixelWidth = std::max(1, static_cast<int>(std::round(width() * devicePixelRatio())));
    const int pixelHeight = std::max(1, static_cast<int>(std::round(height() * devicePixelRatio())));

    m_graphicsWindow = new osgViewer::GraphicsWindowEmbedded(0, 0, pixelWidth, pixelHeight);

    m_camera = new osg::Camera;
    m_camera->setGraphicsContext(m_graphicsWindow);
    m_camera->setViewport(0, 0, pixelWidth, pixelHeight);
    m_camera->setClearColor(osg::Vec4(34.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.0f));
    m_camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_cameraManipulator = new osgGA::ViewManipulator;
    m_cameraManipulator->setAutoComputeHomePosition(false);
    m_cameraManipulator->setWheelZoomFactor(0.1);

    m_viewer = new osgViewer::Viewer;
    m_viewer->setCamera(m_camera);
    m_viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);
    m_viewer->setCameraManipulator(m_cameraManipulator, false);

    updateProjection();
    rebuildScene();
    resetView();

    m_initialized = true;
}

void OsgModelingView::configureRootState() {
    if (!m_root) {
        return;
    }

    osg::StateSet* stateSet = m_root->getOrCreateStateSet();
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA), osg::StateAttribute::ON);
#if defined(OPENTWIN_OSG_WASM)
    stateSet->setAttributeAndModes(createColorProgram(), osg::StateAttribute::ON);
#else
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
#endif
}

void OsgModelingView::updateProjection() {
    if (!m_camera) {
        return;
    }
    const double viewportWidth = std::max(1, width());
    const double viewportHeight = std::max(1, height());
    const double aspectRatio = viewportWidth / viewportHeight;
    m_camera->setProjectionMatrixAsPerspective(15.0, aspectRatio, 0.1, 10000.0);

    if (m_axisOverlayCamera) {
        const QPointF scaled = scaledPosition(QPointF(width(), height()));
        m_axisOverlayCamera->setViewport(0, 0, scaled.x(), scaled.y());
        m_axisOverlayCamera->setProjectionMatrix(osg::Matrix::ortho2D(0.0, scaled.x(), 0.0, scaled.y()));
    }
}

void OsgModelingView::updateAxisOverlay() {
    if (!m_axisOverlayGeometry || !m_cameraManipulator) {
        return;
    }

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    const QPointF scaled = scaledPosition(QPointF(width(), height()));
    const float originX = 46.0f;
    const float originY = 42.0f;
    const float length = 34.0f;
    const osg::Vec3 origin(originX, originY, 0.0f);

    const osg::Quat rotation = m_cameraManipulator->getRotation().inverse();
    const osg::Vec3 xAxis = rotation * osg::Vec3(1.0f, 0.0f, 0.0f);
    const osg::Vec3 yAxis = rotation * osg::Vec3(0.0f, 1.0f, 0.0f);
    const osg::Vec3 zAxis = rotation * osg::Vec3(0.0f, 0.0f, 1.0f);

    const auto endpoint = [&](const osg::Vec3& _axis) {
        return origin + osg::Vec3(_axis.x(), _axis.y(), 0.0f) * length;
    };

    Q_UNUSED(scaled);
    addLine(*vertices, *colors, origin, endpoint(xAxis), QColor(255, 58, 42));
    addLine(*vertices, *colors, origin, endpoint(yAxis), QColor(32, 209, 70));
    addLine(*vertices, *colors, origin, endpoint(zAxis), QColor(50, 98, 255));

    m_axisOverlayGeometry->setVertexArray(vertices);
    m_axisOverlayGeometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    m_axisOverlayGeometry->removePrimitiveSet(0, m_axisOverlayGeometry->getNumPrimitiveSets());
    m_axisOverlayGeometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));
}

void OsgModelingView::requestFrame() {
    if (isExposed()) {
        update();
    }
}

bool OsgModelingView::pickEntity(const QPointF& _position) {
    if (!m_state || !m_viewer || !m_camera) {
        return false;
    }

    if (pickEntityAtWindowPosition(scaledPosition(_position))) {
        return true;
    }

    const std::array<QPointF, 16> offsets = {{
        QPointF(-5.0, 0.0), QPointF(5.0, 0.0), QPointF(0.0, -5.0), QPointF(0.0, 5.0),
        QPointF(-5.0, -5.0), QPointF(5.0, -5.0), QPointF(-5.0, 5.0), QPointF(5.0, 5.0),
        QPointF(-10.0, 0.0), QPointF(10.0, 0.0), QPointF(0.0, -10.0), QPointF(0.0, 10.0),
        QPointF(-10.0, -10.0), QPointF(10.0, -10.0), QPointF(-10.0, 10.0), QPointF(10.0, 10.0)
    }};

    const QPointF pos = scaledPosition(_position);
    for (const QPointF& offset : offsets) {
        if (pickEntityAtWindowPosition(pos + offset)) {
            return true;
        }
    }

    return false;
}

bool OsgModelingView::pickEntityAtWindowPosition(const QPointF& _position) {
    osg::ref_ptr<osgUtil::LineSegmentIntersector> intersector =
        new osgUtil::LineSegmentIntersector(osgUtil::Intersector::WINDOW, _position.x(), _position.y());
    osgUtil::IntersectionVisitor visitor(intersector.get());
    m_camera->accept(visitor);

    if (!intersector->containsIntersections()) {
        return false;
    }

    const int id = entityIdFromIntersections(intersector->getIntersections());
    if (id > 0) {
        m_state->selectEntity(id);
        return true;
    }

    return false;
}

int OsgModelingView::entityIdFromIntersections(const osgUtil::LineSegmentIntersector::Intersections& _intersections) const {
    for (const osgUtil::LineSegmentIntersector::Intersection& hit : _intersections) {
        for (auto it = hit.nodePath.rbegin(); it != hit.nodePath.rend(); ++it) {
            bool ok = false;
            const int id = QString::fromStdString((*it)->getName()).toInt(&ok);
            if (ok && id > 0) {
                return id;
            }
        }
    }
    return -1;
}

osgGA::EventQueue* OsgModelingView::eventQueue() const {
    if (!m_graphicsWindow) {
        return nullptr;
    }
    return m_graphicsWindow->getEventQueue();
}

osg::Vec3d OsgModelingView::entityCenter(const SceneEntity& _entity) const {
    return osg::Vec3d(_entity.x, _entity.y, _entity.z + _entity.sizeZ * 0.5);
}

osg::Vec3d OsgModelingView::modelCenter() const {
    if (m_benchmarkSceneNode) {
        return m_benchmarkSceneNode->getBound().center();
    }
    if (!m_state || m_state->entities().isEmpty()) {
        return osg::Vec3d(0.0, 0.0, 0.0);
    }

    osg::Vec3d center;
    for (const SceneEntity& entity : m_state->entities()) {
        center += entityCenter(entity);
    }
    return center / static_cast<double>(m_state->entities().size());
}

double OsgModelingView::modelRadius() const {
    if (m_benchmarkSceneNode) {
        return std::max(1.0f, m_benchmarkSceneNode->getBound().radius());
    }
    if (!m_state || m_state->entities().isEmpty()) {
        return 6.0;
    }

    const osg::Vec3d center = modelCenter();
    double radius = 1.0;
    for (const SceneEntity& entity : m_state->entities()) {
        const osg::Vec3d delta = entityCenter(entity) - center;
        const double entityRadius = std::max({ entity.radius, entity.sizeX, entity.sizeY, entity.sizeZ }) * 0.85;
        radius = std::max(radius, delta.length() + entityRadius);
    }
    return radius;
}

osg::ref_ptr<osg::Group> OsgModelingView::createSceneRoot() const {
    osg::ref_ptr<osg::Group> root = new osg::Group;
    root->addChild(createWorkingPlane());

    if (m_state) {
        for (const SceneEntity& entity : m_state->entities()) {
            root->addChild(createEntityNode(entity));
        }
    }

    return root;
}

osg::ref_ptr<osg::Node> OsgModelingView::createWorkingPlane() const {
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    const QColor major(168, 168, 168, 165);
    const QColor minor(150, 150, 150, 95);

    for (int i = -12; i <= 12; ++i) {
        const QColor color = (i % 4 == 0) ? major : minor;
        addLine(*vertices, *colors, osg::Vec3(i, -12.0f, 0.0f), osg::Vec3(i, 12.0f, 0.0f), color);
        addLine(*vertices, *colors, osg::Vec3(-12.0f, i, 0.0f), osg::Vec3(12.0f, i, 0.0f), color);
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName("Working plane");
    geode->addDrawable(geometry);
    geode->getOrCreateStateSet()->setAttributeAndModes(new osg::LineWidth(1.0f), osg::StateAttribute::ON);
    return geode;
}

osg::ref_ptr<osg::Camera> OsgModelingView::createAxisOverlay() {
    const QPointF scaled = scaledPosition(QPointF(width(), height()));

    m_axisOverlayGeometry = new osg::Geometry;
    enableWebGeometry(*m_axisOverlayGeometry);
    m_axisOverlayGeometry->getOrCreateStateSet()->setAttributeAndModes(new osg::LineWidth(2.0f), osg::StateAttribute::ON);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName("Axis overlay");
    geode->addDrawable(m_axisOverlayGeometry);

    osg::ref_ptr<osg::Camera> camera = new osg::Camera;
    camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    camera->setRenderOrder(osg::Camera::POST_RENDER);
    camera->setClearMask(GL_DEPTH_BUFFER_BIT);
    camera->setAllowEventFocus(false);
    camera->setViewport(0, 0, scaled.x(), scaled.y());
    camera->setProjectionMatrix(osg::Matrix::ortho2D(0.0, scaled.x(), 0.0, scaled.y()));
    camera->setViewMatrix(osg::Matrix::identity());
    camera->addChild(geode);
    camera->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);

    return camera;
}

osg::ref_ptr<osg::Node> OsgModelingView::createEntityNode(const SceneEntity& _entity) const {
    osg::ref_ptr<osg::Geometry> geometry;
    switch (_entity.kind) {
    case ShapeKind::Cuboid:
    case ShapeKind::BooleanResult:
    case ShapeKind::ImportedStep:
        geometry = createCuboidGeometry(_entity);
        break;
    case ShapeKind::Cylinder:
        geometry = createCylinderGeometry(_entity);
        break;
    case ShapeKind::Sphere:
        geometry = createSphereGeometry(_entity);
        break;
    case ShapeKind::Torus:
        geometry = createTorusGeometry(_entity);
        break;
    case ShapeKind::Cone:
        geometry = createConeGeometry(_entity);
        break;
    case ShapeKind::Pyramid:
        geometry = createPyramidGeometry(_entity);
        break;
    }

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->setName(QString::number(_entity.id).toStdString());
    geode->addDrawable(geometry);
    return geode;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createCuboidGeometry(const SceneEntity& _entity) const {
    const float x0 = static_cast<float>(_entity.x - _entity.sizeX * 0.5);
    const float x1 = static_cast<float>(_entity.x + _entity.sizeX * 0.5);
    const float y0 = static_cast<float>(_entity.y - _entity.sizeY * 0.5);
    const float y1 = static_cast<float>(_entity.y + _entity.sizeY * 0.5);
    const float z0 = static_cast<float>(_entity.z);
    const float z1 = static_cast<float>(_entity.z + _entity.sizeZ);

    const std::array<osg::Vec3, 8> p = {
        osg::Vec3(x0, y0, z0), osg::Vec3(x1, y0, z0), osg::Vec3(x1, y1, z0), osg::Vec3(x0, y1, z0),
        osg::Vec3(x0, y0, z1), osg::Vec3(x1, y0, z1), osg::Vec3(x1, y1, z1), osg::Vec3(x0, y1, z1)
    };

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;
    const bool selected = m_state && _entity.id == m_state->selectedId();

    const auto quad = [&](int a, int b, int c, int d) {
        const QColor color = shadedColor(_entity.color, normalFromTriangle(p[a], p[b], p[c]), selected);
        addTriangle(*vertices, *colors, p[a], p[b], p[c], color);
        addTriangle(*vertices, *colors, p[a], p[c], p[d], color);
    };

    quad(0, 1, 2, 3);
    quad(4, 7, 6, 5);
    quad(0, 4, 5, 1);
    quad(1, 5, 6, 2);
    quad(2, 6, 7, 3);
    quad(3, 7, 4, 0);

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createCylinderGeometry(const SceneEntity& _entity, int _segments) const {
    const float radius = static_cast<float>(_entity.radius);
    const float z0 = static_cast<float>(_entity.z);
    const float z1 = static_cast<float>(_entity.z + _entity.sizeZ);
    const osg::Vec3 bottomCenter(static_cast<float>(_entity.x), static_cast<float>(_entity.y), z0);
    const osg::Vec3 topCenter(static_cast<float>(_entity.x), static_cast<float>(_entity.y), z1);
    const bool selected = m_state && _entity.id == m_state->selectedId();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    for (int i = 0; i < _segments; ++i) {
        const float a0 = 2.0f * static_cast<float>(Pi) * i / _segments;
        const float a1 = 2.0f * static_cast<float>(Pi) * (i + 1) / _segments;
        const osg::Vec3 b0(bottomCenter.x() + std::cos(a0) * radius, bottomCenter.y() + std::sin(a0) * radius, z0);
        const osg::Vec3 b1(bottomCenter.x() + std::cos(a1) * radius, bottomCenter.y() + std::sin(a1) * radius, z0);
        const osg::Vec3 t0(b0.x(), b0.y(), z1);
        const osg::Vec3 t1(b1.x(), b1.y(), z1);
        addTriangle(*vertices, *colors, b0, b1, t1, shadedColor(_entity.color, normalFromTriangle(b0, b1, t1), selected));
        addTriangle(*vertices, *colors, b0, t1, t0, shadedColor(_entity.color, normalFromTriangle(b0, t1, t0), selected));
        addTriangle(*vertices, *colors, topCenter, t0, t1, shadedColor(_entity.color, osg::Vec3(0.0f, 0.0f, 1.0f), selected));
        addTriangle(*vertices, *colors, bottomCenter, b1, b0, shadedColor(_entity.color, osg::Vec3(0.0f, 0.0f, -1.0f), selected));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createSphereGeometry(const SceneEntity& _entity, int _rings, int _segments) const {
    const osg::Vec3 center(static_cast<float>(_entity.x), static_cast<float>(_entity.y), static_cast<float>(_entity.z + _entity.radius));
    const float radius = static_cast<float>(_entity.radius);
    const bool selected = m_state && _entity.id == m_state->selectedId();

    const auto point = [&](int ring, int segment) {
        const float v = static_cast<float>(ring) / _rings;
        const float phi = static_cast<float>(Pi) * v;
        const float u = static_cast<float>(segment) / _segments;
        const float theta = 2.0f * static_cast<float>(Pi) * u;
        return center + osg::Vec3(std::cos(theta) * std::sin(phi), std::sin(theta) * std::sin(phi), std::cos(phi)) * radius;
    };

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    for (int ring = 0; ring < _rings; ++ring) {
        for (int segment = 0; segment < _segments; ++segment) {
            const osg::Vec3 p0 = point(ring, segment);
            const osg::Vec3 p1 = point(ring + 1, segment);
            const osg::Vec3 p2 = point(ring + 1, segment + 1);
            const osg::Vec3 p3 = point(ring, segment + 1);
            osg::Vec3 normal0 = p0 - center;
            osg::Vec3 normal3 = p3 - center;
            normal0.normalize();
            normal3.normalize();
            addTriangle(*vertices, *colors, p0, p1, p2, shadedColor(_entity.color, normal0, selected));
            addTriangle(*vertices, *colors, p0, p2, p3, shadedColor(_entity.color, normal3, selected));
        }
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createTorusGeometry(const SceneEntity& _entity, int _majorSegments, int _minorSegments) const {
    const osg::Vec3 center(static_cast<float>(_entity.x), static_cast<float>(_entity.y), static_cast<float>(_entity.z + _entity.secondaryRadius));
    const float majorRadius = static_cast<float>(_entity.radius);
    const float minorRadius = static_cast<float>(_entity.secondaryRadius);
    const bool selected = m_state && _entity.id == m_state->selectedId();

    const auto point = [&](int major, int minor) {
        const float u = 2.0f * static_cast<float>(Pi) * major / _majorSegments;
        const float v = 2.0f * static_cast<float>(Pi) * minor / _minorSegments;
        const float radial = majorRadius + minorRadius * std::cos(v);
        return center + osg::Vec3(radial * std::cos(u), radial * std::sin(u), minorRadius * std::sin(v));
    };

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    for (int major = 0; major < _majorSegments; ++major) {
        for (int minor = 0; minor < _minorSegments; ++minor) {
            const osg::Vec3 p0 = point(major, minor);
            const osg::Vec3 p1 = point(major + 1, minor);
            const osg::Vec3 p2 = point(major + 1, minor + 1);
            const osg::Vec3 p3 = point(major, minor + 1);
            const QColor color = shadedColor(_entity.color, normalFromTriangle(p0, p1, p2), selected);
            addTriangle(*vertices, *colors, p0, p1, p2, color);
            addTriangle(*vertices, *colors, p0, p2, p3, color);
        }
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createConeGeometry(const SceneEntity& _entity, int _segments) const {
    const float radius = static_cast<float>(_entity.radius);
    const float z0 = static_cast<float>(_entity.z);
    const float z1 = static_cast<float>(_entity.z + _entity.sizeZ);
    const osg::Vec3 center(static_cast<float>(_entity.x), static_cast<float>(_entity.y), z0);
    const osg::Vec3 apex(static_cast<float>(_entity.x), static_cast<float>(_entity.y), z1);
    const bool selected = m_state && _entity.id == m_state->selectedId();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    for (int i = 0; i < _segments; ++i) {
        const float a0 = 2.0f * static_cast<float>(Pi) * i / _segments;
        const float a1 = 2.0f * static_cast<float>(Pi) * (i + 1) / _segments;
        const osg::Vec3 b0(center.x() + std::cos(a0) * radius, center.y() + std::sin(a0) * radius, z0);
        const osg::Vec3 b1(center.x() + std::cos(a1) * radius, center.y() + std::sin(a1) * radius, z0);
        addTriangle(*vertices, *colors, b0, b1, apex, shadedColor(_entity.color, normalFromTriangle(b0, b1, apex), selected));
        addTriangle(*vertices, *colors, center, b1, b0, shadedColor(_entity.color, osg::Vec3(0.0f, 0.0f, -1.0f), selected));
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Geometry> OsgModelingView::createPyramidGeometry(const SceneEntity& _entity) const {
    const float x0 = static_cast<float>(_entity.x - _entity.sizeX * 0.5);
    const float x1 = static_cast<float>(_entity.x + _entity.sizeX * 0.5);
    const float y0 = static_cast<float>(_entity.y - _entity.sizeY * 0.5);
    const float y1 = static_cast<float>(_entity.y + _entity.sizeY * 0.5);
    const float z0 = static_cast<float>(_entity.z);
    const osg::Vec3 p0(x0, y0, z0);
    const osg::Vec3 p1(x1, y0, z0);
    const osg::Vec3 p2(x1, y1, z0);
    const osg::Vec3 p3(x0, y1, z0);
    const osg::Vec3 apex(static_cast<float>(_entity.x), static_cast<float>(_entity.y), static_cast<float>(_entity.z + _entity.sizeZ));
    const bool selected = m_state && _entity.id == m_state->selectedId();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array;
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array;

    const auto tri = [&](const osg::Vec3& a, const osg::Vec3& b, const osg::Vec3& c) {
        addTriangle(*vertices, *colors, a, b, c, shadedColor(_entity.color, normalFromTriangle(a, b, c), selected));
    };

    tri(p0, p1, p2);
    tri(p0, p2, p3);
    tri(p0, apex, p1);
    tri(p1, apex, p2);
    tri(p2, apex, p3);
    tri(p3, apex, p0);

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
    enableWebGeometry(*geometry);
    geometry->setName(QString::number(_entity.id).toStdString());
    geometry->setVertexArray(vertices);
    geometry->setColorArray(colors, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices->size())));
    return geometry;
}

osg::ref_ptr<osg::Program> OsgModelingView::createColorProgram() const {
    static const char* vertexShader =
        "#ifdef GL_ES\n"
        "precision highp float;\n"
        "#endif\n"
        "attribute vec4 osg_Vertex;\n"
        "attribute vec4 osg_Color;\n"
        "uniform mat4 osg_ModelViewProjectionMatrix;\n"
        "varying vec4 vertexColor;\n"
        "void main() {\n"
        "    vertexColor = osg_Color;\n"
        "    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;\n"
        "}\n";

    static const char* fragmentShader =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "varying vec4 vertexColor;\n"
        "void main() {\n"
        "    gl_FragColor = vertexColor;\n"
        "}\n";

    osg::ref_ptr<osg::Program> program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vertexShader));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragmentShader));
    return program;
}

void OsgModelingView::addLine(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _a, const osg::Vec3& _b, const QColor& _color) const {
    addVertex(_vertices, _colors, _a, _color);
    addVertex(_vertices, _colors, _b, _color);
}

void OsgModelingView::addTriangle(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _a, const osg::Vec3& _b, const osg::Vec3& _c, const QColor& _color) const {
    addVertex(_vertices, _colors, _a, _color);
    addVertex(_vertices, _colors, _b, _color);
    addVertex(_vertices, _colors, _c, _color);
}

void OsgModelingView::addVertex(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _point, const QColor& _color) const {
    _vertices.push_back(_point);
    _colors.push_back(colorToVec4(_color));
}

QColor OsgModelingView::shadedColor(const QColor& _base, const osg::Vec3& _normal, bool _selected) const {
    osg::Vec3 normal = _normal;
    if (normal.length2() <= 0.000001f) {
        normal = osg::Vec3(0.0f, 0.0f, 1.0f);
    }
    normal.normalize();

    osg::Vec3 lightDirection(-0.35f, -0.55f, 0.75f);
    lightDirection.normalize();
    const double intensity = 0.35 + 0.65 * std::max(0.0f, normal * lightDirection);

    QColor lit = QColor::fromRgbF(
        std::clamp(_base.redF() * intensity, 0.0, 1.0),
        std::clamp(_base.greenF() * intensity, 0.0, 1.0),
        std::clamp(_base.blueF() * intensity, 0.0, 1.0),
        1.0
    );

    if (_selected) {
        lit = mixColor(lit, QColor(0, 255, 0), 0.25);
    }

    return lit;
}

osg::Vec4 OsgModelingView::colorToVec4(const QColor& _color) const {
    return osg::Vec4(
        static_cast<float>(_color.redF()),
        static_cast<float>(_color.greenF()),
        static_cast<float>(_color.blueF()),
        static_cast<float>(_color.alphaF())
    );
}

int OsgModelingView::qtButtonToOsgButton(Qt::MouseButton _button) const {
    switch (_button) {
    case Qt::LeftButton:
        return 1;
    case Qt::MiddleButton:
        return 2;
    case Qt::RightButton:
        return 3;
    default:
        return 0;
    }
}

QPointF OsgModelingView::scaledPosition(const QPointF& _position) const {
    return _position * devicePixelRatio();
}

} // namespace prototype
