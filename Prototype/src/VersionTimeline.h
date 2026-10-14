// @otlicense
// File: VersionTimeline.h
// @otlicense-end

#pragma once

#include "ModelingState.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMap>
#include <QRectF>

namespace prototype {

class VersionTimeline : public QGraphicsView {
    Q_OBJECT

public:
    explicit VersionTimeline(QWidget* _parent = nullptr);

public slots:
    void setVersionGraph(const VersionGraphData& _graph);

signals:
    void versionActivateRequested(const QString& _versionName);

protected:
    void mouseDoubleClickEvent(QMouseEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;
    void wheelEvent(QWheelEvent* _event) override;

private:
    struct VersionItemGeometry {
        QRectF rect;
        QString parentVersion;
        bool onActiveBranch = false;
    };

    void rebuildScene();
    void centerOnActiveVersion();
    bool versionIsOnActivePath(const QString& _versionName) const;
    const VersionGraphNode* findVersionNode(const QString& _versionName) const;
    const VersionGraphNode* findParentNode(const VersionGraphNode& _node) const;
    QString versionAt(const QPoint& _viewPosition) const;

    QGraphicsScene m_scene;
    VersionGraphData m_graph;
    QMap<QString, VersionItemGeometry> m_itemGeometry;
    qreal m_zoomScale = 1.0;
};

} // namespace prototype
