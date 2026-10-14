// @otlicense
// File: VersionTimeline.cpp
// @otlicense-end

#include "VersionTimeline.h"

#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsSimpleTextItem>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace prototype {
namespace {

constexpr qreal VersionHeight = 50.0;
constexpr qreal VersionWidth = 118.0;
constexpr qreal VerticalSpacing = 30.0;
constexpr qreal HorizontalSpacing = 60.0;
constexpr qreal MinimumZoomScale = 0.35;
constexpr qreal MaximumZoomScale = 3.0;

QString versionBranchName(const QString& _versionName) {
    const int ix = _versionName.lastIndexOf(QLatin1Char('.'));
    return ix < 0 ? QString() : _versionName.left(ix);
}

QString versionBranchNodeName(const QString& _versionName) {
    const int ix = _versionName.lastIndexOf(QLatin1Char('.'));
    if (ix < 0) {
        return QString();
    }

    const int ix2 = _versionName.lastIndexOf(QLatin1Char('.'), ix - 1);
    return ix2 < 0 ? QString() : _versionName.left(ix2);
}

int versionNumber(const QString& _versionName) {
    const int ix = _versionName.lastIndexOf(QLatin1Char('.'));
    bool ok = false;
    const int value = (ix < 0 ? _versionName : _versionName.mid(ix + 1)).toInt(&ok);
    return ok ? value : 0;
}

QRectF textRect(const QRectF& _rect, qreal _top, qreal _height) {
    return QRectF(_rect.left() + 10.0, _rect.top() + _top, _rect.width() - 20.0, _height);
}

QString elidedText(const QString& _text, const QFont& _font, qreal _width) {
    return QFontMetrics(_font).elidedText(_text, Qt::ElideRight, qMax(1, qRound(_width)));
}

} // namespace

VersionTimeline::VersionTimeline(QWidget* _parent)
    : QGraphicsView(_parent)
{
    setMinimumHeight(118);
    setScene(&m_scene);
    setRenderHint(QPainter::Antialiasing, true);
    setBackgroundBrush(QColor(36, 36, 36));
    setFrameShape(QFrame::NoFrame);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
}

void VersionTimeline::setVersionGraph(const VersionGraphData& _graph) {
    m_graph = _graph;
    rebuildScene();
}

void VersionTimeline::mouseDoubleClickEvent(QMouseEvent* _event) {
    const QString versionName = versionAt(_event->pos());
    if (!versionName.isEmpty()) {
        emit versionActivateRequested(versionName);
        _event->accept();
        return;
    }

    QGraphicsView::mouseDoubleClickEvent(_event);
}

void VersionTimeline::resizeEvent(QResizeEvent* _event) {
    QGraphicsView::resizeEvent(_event);
    centerOnActiveVersion();
}

void VersionTimeline::wheelEvent(QWheelEvent* _event) {
    const int angle = _event->angleDelta().y();
    if (angle == 0) {
        QGraphicsView::wheelEvent(_event);
        return;
    }

    const qreal factor = angle > 0 ? 1.1 : 0.9;
    const qreal nextScale = m_zoomScale * factor;
    if (nextScale < MinimumZoomScale || nextScale > MaximumZoomScale) {
        _event->accept();
        return;
    }

    const ViewportAnchor anchor = transformationAnchor();
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(factor, factor);
    setTransformationAnchor(anchor);

    m_zoomScale = nextScale;
    _event->accept();
}

void VersionTimeline::rebuildScene() {
    m_scene.clear();
    m_itemGeometry.clear();

    if (m_graph.branches.isEmpty()) {
        return;
    }

    m_scene.setBackgroundBrush(QColor(36, 36, 36));

    for (int row = 0; row < m_graph.branches.size(); ++row) {
        const VersionGraphBranch& branch = m_graph.branches.at(row);
        for (const VersionGraphNode& node : branch) {
            const VersionGraphNode* parent = findParentNode(node);
            qreal x = 0.0;
            if (parent && m_itemGeometry.contains(parent->name)) {
                x = m_itemGeometry.value(parent->name).rect.right() + HorizontalSpacing;
            }
            const qreal y = row * (VersionHeight + VerticalSpacing);

            VersionItemGeometry geometry;
            geometry.rect = QRectF(x, y, VersionWidth, VersionHeight);
            geometry.parentVersion = parent ? parent->name : QString();
            geometry.onActiveBranch = versionIsOnActivePath(node.name);
            m_itemGeometry.insert(node.name, geometry);
        }
    }

    for (const VersionGraphBranch& branch : m_graph.branches) {
        for (const VersionGraphNode& node : branch) {
            const VersionItemGeometry geometry = m_itemGeometry.value(node.name);
            if (!geometry.parentVersion.isEmpty() && m_itemGeometry.contains(geometry.parentVersion)) {
                const QRectF parentRect = m_itemGeometry.value(geometry.parentVersion).rect;
                const QPointF start(parentRect.right(), parentRect.center().y());
                const QPointF end(geometry.rect.left(), geometry.rect.center().y());
                const qreal controlOffset = qMax(24.0, (end.x() - start.x()) * 0.45);

                QPainterPath path(start);
                path.cubicTo(
                    QPointF(start.x() + controlOffset, start.y()),
                    QPointF(end.x() - controlOffset, end.y()),
                    end
                );

                QPen connectionPen(geometry.onActiveBranch ? QColor(0, 220, 40) : QColor(115, 115, 115), geometry.onActiveBranch ? 2.0 : 1.2);
                if (node.directParentIsHidden) {
                    connectionPen.setStyle(Qt::DashDotDotLine);
                }

                QGraphicsPathItem* pathItem = m_scene.addPath(path, connectionPen);
                pathItem->setZValue(0.0);
            }
        }
    }

    for (const VersionGraphBranch& branch : m_graph.branches) {
        for (const VersionGraphNode& node : branch) {
            const VersionItemGeometry geometry = m_itemGeometry.value(node.name);
            const bool active = node.name == m_graph.activeVersionName;
            const QRectF rect = geometry.rect;

            QPen outline(active ? QColor(245, 245, 245) : QColor(126, 126, 126), active ? 2.0 : 1.0);
            if (active) {
                outline.setStyle(Qt::DotLine);
            }

            QGraphicsRectItem* item = m_scene.addRect(rect, outline, active ? QColor(245, 245, 245) : QColor(65, 65, 65));
            item->setData(0, node.name);
            item->setToolTip(node.description);
            item->setZValue(1.0);

            const QColor textColor = active ? QColor(36, 36, 36) : QColor(245, 245, 245);
            const QString label = node.label.isEmpty() ? node.name : node.label;
            if (node.label.isEmpty()) {
                const QFont labelFont(QStringLiteral("Arial"), 9, QFont::Bold);
                QGraphicsSimpleTextItem* labelItem = m_scene.addSimpleText(elidedText(label, labelFont, rect.width() - 20.0), labelFont);
                labelItem->setBrush(textColor);
                labelItem->setPos(textRect(rect, 14.0, 22.0).topLeft());
                labelItem->setData(0, node.name);
                labelItem->setZValue(2.0);
            }
            else {
                const QFont nameFont(QStringLiteral("Arial"), 7, QFont::Bold);
                const QFont labelFont(QStringLiteral("Arial"), 8);
                QGraphicsSimpleTextItem* nameItem = m_scene.addSimpleText(elidedText(node.name, nameFont, rect.width() - 20.0), nameFont);
                nameItem->setBrush(textColor);
                nameItem->setPos(textRect(rect, 4.0, 14.0).topLeft());
                nameItem->setData(0, node.name);
                nameItem->setZValue(2.0);

                QGraphicsSimpleTextItem* labelItem = m_scene.addSimpleText(elidedText(label, labelFont, rect.width() - 20.0), labelFont);
                labelItem->setBrush(textColor);
                labelItem->setPos(textRect(rect, 22.0, 20.0).topLeft());
                labelItem->setData(0, node.name);
                labelItem->setZValue(2.0);
            }
        }
    }

    QRectF sceneRect = m_scene.itemsBoundingRect().adjusted(-10.0, -10.0, 10.0, 10.0);
    if (sceneRect.width() < viewport()->width()) {
        sceneRect.setWidth(viewport()->width());
    }
    if (sceneRect.height() < viewport()->height()) {
        sceneRect.setHeight(viewport()->height());
    }
    m_scene.setSceneRect(sceneRect);

    centerOnActiveVersion();
}

void VersionTimeline::centerOnActiveVersion() {
    if (m_graph.activeVersionName.isEmpty() || !m_itemGeometry.contains(m_graph.activeVersionName)) {
        return;
    }

    centerOn(m_itemGeometry.value(m_graph.activeVersionName).rect.center());
}

bool VersionTimeline::versionIsOnActivePath(const QString& _versionName) const {
    if (_versionName == m_graph.activeVersionName) {
        return true;
    }

    const VersionGraphNode* node = findVersionNode(m_graph.activeVersionName);
    while (node && !node->parentVersion.isEmpty()) {
        if (node->parentVersion == _versionName) {
            return true;
        }
        node = findVersionNode(node->parentVersion);
    }

    return false;
}

const VersionGraphNode* VersionTimeline::findVersionNode(const QString& _versionName) const {
    for (const VersionGraphBranch& branch : m_graph.branches) {
        for (const VersionGraphNode& node : branch) {
            if (node.name == _versionName) {
                return &node;
            }
        }
    }
    return nullptr;
}

const VersionGraphNode* VersionTimeline::findParentNode(const VersionGraphNode& _node) const {
    if (!_node.parentVersion.isEmpty()) {
        return findVersionNode(_node.parentVersion);
    }

    const QString branchNodeName = versionBranchNodeName(_node.name);
    if (branchNodeName.isEmpty()) {
        return nullptr;
    }

    const QString parentBranchName = versionBranchName(branchNodeName);
    const int branchNodeNumber = versionNumber(branchNodeName);
    const VersionGraphNode* parentNode = nullptr;
    for (const VersionGraphBranch& branch : m_graph.branches) {
        if (branch.isEmpty() || versionBranchName(branch.front().name) != parentBranchName) {
            continue;
        }

        for (const VersionGraphNode& node : branch) {
            if (versionNumber(node.name) > branchNodeNumber) {
                break;
            }
            parentNode = &node;
        }
        break;
    }

    return parentNode;
}

QString VersionTimeline::versionAt(const QPoint& _viewPosition) const {
    const QPointF scenePosition = mapToScene(_viewPosition);
    for (auto it = m_itemGeometry.constBegin(); it != m_itemGeometry.constEnd(); ++it) {
        if (it.value().rect.contains(scenePosition)) {
            return it.key();
        }
    }
    return QString();
}

} // namespace prototype
