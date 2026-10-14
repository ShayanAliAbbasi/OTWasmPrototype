// @otlicense
// File: OsgModelingView.h
// @otlicense-end

#pragma once

#include <QColor>
#include <QOpenGLWindow>

#include <osg/Geometry>
#include <osg/Group>
#include <osg/Camera>
#include <osg/ref_ptr>
#include <osgUtil/LineSegmentIntersector>

class QByteArray;
namespace osgGA { class EventQueue; class ViewManipulator; }
namespace osgViewer { class GraphicsWindowEmbedded; class Viewer; }

namespace prototype {

class ModelingState;
struct SceneEntity;

class OsgModelingView : public QOpenGLWindow {
    Q_OBJECT

public:
    explicit OsgModelingView(QWindow* _parent = nullptr);
    ~OsgModelingView() override;

    void setState(ModelingState* _state);
    void requestBenchmarkFrame();
    void setBenchmarkSynchronizationEnabled(bool _enabled);
    bool loadBenchmarkOsgb(const QByteArray& _payload, double& _deserializeMilliseconds);
    QString glVendor() const { return m_glVendor; }
    QString glRenderer() const { return m_glRenderer; }
    QString glVersion() const { return m_glVersion; }

signals:
    void frameRendered(double _renderMilliseconds);

public slots:
    void resetView();

protected:
    void initializeGL() override;
    void resizeGL(int _width, int _height) override;
    void paintGL() override;
    void exposeEvent(QExposeEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void mouseDoubleClickEvent(QMouseEvent* _event) override;
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void wheelEvent(QWheelEvent* _event) override;
    void keyPressEvent(QKeyEvent* _event) override;
    void keyReleaseEvent(QKeyEvent* _event) override;

private slots:
    void rebuildScene();

private:
    void initializeViewer();
    void configureRootState();
    void updateProjection();
    void updateAxisOverlay();
    void requestFrame();
    bool pickEntity(const QPointF& _position);
    bool pickEntityAtWindowPosition(const QPointF& _position);
    int entityIdFromIntersections(const osgUtil::LineSegmentIntersector::Intersections& _intersections) const;
    osgGA::EventQueue* eventQueue() const;

    osg::Vec3d entityCenter(const SceneEntity& _entity) const;
    osg::Vec3d modelCenter() const;
    double modelRadius() const;

    osg::ref_ptr<osg::Group> createSceneRoot() const;
    osg::ref_ptr<osg::Node> createWorkingPlane() const;
    osg::ref_ptr<osg::Camera> createAxisOverlay();
    osg::ref_ptr<osg::Node> createEntityNode(const SceneEntity& _entity) const;
    osg::ref_ptr<osg::Geometry> createCuboidGeometry(const SceneEntity& _entity) const;
    osg::ref_ptr<osg::Geometry> createCylinderGeometry(const SceneEntity& _entity, int _segments = 40) const;
    osg::ref_ptr<osg::Geometry> createSphereGeometry(const SceneEntity& _entity, int _rings = 16, int _segments = 32) const;
    osg::ref_ptr<osg::Geometry> createTorusGeometry(const SceneEntity& _entity, int _majorSegments = 48, int _minorSegments = 12) const;
    osg::ref_ptr<osg::Geometry> createConeGeometry(const SceneEntity& _entity, int _segments = 40) const;
    osg::ref_ptr<osg::Geometry> createPyramidGeometry(const SceneEntity& _entity) const;
    osg::ref_ptr<osg::Program> createColorProgram() const;

    void addLine(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _a, const osg::Vec3& _b, const QColor& _color) const;
    void addTriangle(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _a, const osg::Vec3& _b, const osg::Vec3& _c, const QColor& _color) const;
    void addVertex(osg::Vec3Array& _vertices, osg::Vec4Array& _colors, const osg::Vec3& _point, const QColor& _color) const;
    QColor shadedColor(const QColor& _base, const osg::Vec3& _normal, bool _selected) const;
    osg::Vec4 colorToVec4(const QColor& _color) const;
    int qtButtonToOsgButton(Qt::MouseButton _button) const;
    QPointF scaledPosition(const QPointF& _position) const;

    ModelingState* m_state = nullptr;

    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> m_graphicsWindow;
    osg::ref_ptr<osgViewer::Viewer> m_viewer;
    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Node> m_benchmarkSceneNode;
    osg::ref_ptr<osg::Camera> m_axisOverlayCamera;
    osg::ref_ptr<osg::Geometry> m_axisOverlayGeometry;
    osg::ref_ptr<osgGA::ViewManipulator> m_cameraManipulator;

    bool m_initialized = false;
    bool m_leftButtonDown = false;
    bool m_middleButtonDown = false;
    bool m_rightButtonDown = false;
    bool m_benchmarkSynchronizationEnabled = false;
    QString m_glVendor;
    QString m_glRenderer;
    QString m_glVersion;
};

} // namespace prototype
