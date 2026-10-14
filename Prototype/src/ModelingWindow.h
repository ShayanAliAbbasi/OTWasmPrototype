// @otlicense
// File: ModelingWindow.h
// @otlicense-end

#pragma once

#include "ModelingServiceAdapter.h"
#include "ModelingState.h"

#include <QMainWindow>
#include <QMap>

class QAction;
class QPlainTextEdit;
class QSplitter;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace prototype {

class OsgModelingView;
class PlotWidget;
class VersionTimeline;

class ModelingWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ModelingWindow(QWidget* _parent = nullptr);

    ModelingState& modelingState() { return m_state; }
    OsgModelingView* sceneView() const { return m_sceneView; }
    void configureBenchmarkViewport(int _width, int _height);
    void runBenchmarkModelingOperations(int _operationCount);
    double runBenchmarkPlotIterations(int _iterationCount);
    void showSceneTab();
    void showPlotTab();

private slots:
    void rebuildNavigation();
    void rebuildProperties();
    void updateActionState();
    void appendLogMessage(const QString& _message);
    void handleNavigationSelection();
    void handlePropertyChanged(QTreeWidgetItem* _item, int _column);

private:
    QWidget* createHeader();
    QWidget* createRibbon();
    QWidget* createRibbonGroup(const QString& _title, const QList<QAction*>& _actions);
    QAction* createCommand(const QString& _text, const QString& _iconName, const QKeySequence& _shortcut = QKeySequence());
    QWidget* createNavigationPanel();
    QWidget* createRightPanel();
    QWidget* createBottomPanel();
    void addPropertyGroup(QTreeWidgetItem* _root, const QString& _name);
    void addProperty(QTreeWidgetItem* _group, const QString& _key, const QString& _name, const QString& _value, bool _editable = true);
    QIcon icon(const QString& _iconName) const;
    void applyStyle();

    ModelingState m_state;
    ModelingServiceAdapter m_modelingService;
    OsgModelingView* m_sceneView = nullptr;
    QWidget* m_sceneContainer = nullptr;
    PlotWidget* m_plotWidget = nullptr;
    QTabWidget* m_centerTabs = nullptr;
    QTreeWidget* m_navigationTree = nullptr;
    QTreeWidget* m_propertiesTree = nullptr;
    QPlainTextEdit* m_output = nullptr;
    VersionTimeline* m_timeline = nullptr;
    bool m_rebuildingProperties = false;
    QVector<QAction*> m_selectionActions;
};

} // namespace prototype
