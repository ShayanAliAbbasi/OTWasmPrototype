// @otlicense
// File: ModelingWindow.cpp
// @otlicense-end

#include "ModelingWindow.h"

#include "PlotWidget.h"
#include "OsgModelingView.h"
#include "VersionTimeline.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSizePolicy>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>

namespace prototype {
namespace {

QString colorToText(const QColor& _color) {
    return QStringLiteral("%1; %2; %3").arg(_color.red()).arg(_color.green()).arg(_color.blue());
}

QString boolToText(bool _value) {
    return _value ? QStringLiteral("true") : QStringLiteral("false");
}

QString numberToText(double _value) {
    return QString::number(_value, 'f', 2);
}

} // namespace

ModelingWindow::ModelingWindow(QWidget* _parent)
    : QMainWindow(_parent),
      m_modelingService(m_state, this)
{
    applyStyle();
    setWindowTitle(QStringLiteral("dev - OpenTwin Modeling Prototype"));
    setWindowIcon(icon(QStringLiteral("Images/OpenTwinIcon")));
    resize(1440, 860);

    QWidget* root = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(createHeader());
    rootLayout->addWidget(createRibbon());

    QSplitter* horizontal = new QSplitter(Qt::Horizontal, root);
    horizontal->setChildrenCollapsible(false);
    horizontal->addWidget(createNavigationPanel());

    QSplitter* centerVertical = new QSplitter(Qt::Vertical, horizontal);
    centerVertical->setChildrenCollapsible(false);

    m_centerTabs = new QTabWidget(centerVertical);
    m_sceneView = new OsgModelingView;
    m_sceneView->setState(&m_state);
    m_sceneContainer = QWidget::createWindowContainer(m_sceneView, m_centerTabs);
    m_sceneContainer->setFocusPolicy(Qt::StrongFocus);
    m_sceneContainer->setMouseTracking(true);
    m_sceneContainer->setMinimumSize(520, 330);
    m_sceneContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_centerTabs->addTab(m_sceneContainer, QStringLiteral("3D"));
    m_plotWidget = new PlotWidget(m_centerTabs);
    m_centerTabs->addTab(m_plotWidget, QStringLiteral("Plot"));

    centerVertical->addWidget(m_centerTabs);
    centerVertical->addWidget(createBottomPanel());
    centerVertical->setStretchFactor(0, 5);
    centerVertical->setStretchFactor(1, 1);

    horizontal->addWidget(centerVertical);
    horizontal->addWidget(createRightPanel());
    horizontal->setStretchFactor(0, 0);
    horizontal->setStretchFactor(1, 1);
    horizontal->setStretchFactor(2, 0);
    horizontal->setSizes(QList<int>() << 290 << 860 << 320);

    rootLayout->addWidget(horizontal, 1);
    setCentralWidget(root);

    connect(&m_state, &ModelingState::sceneChanged, this, &ModelingWindow::rebuildNavigation);
    connect(&m_state, &ModelingState::selectionChanged, this, &ModelingWindow::rebuildNavigation);
    connect(&m_state, &ModelingState::selectionChanged, this, &ModelingWindow::rebuildProperties);
    connect(&m_state, &ModelingState::sceneChanged, this, &ModelingWindow::rebuildProperties);
    connect(&m_state, &ModelingState::selectionChanged, this, &ModelingWindow::updateActionState);
    connect(&m_state, &ModelingState::logMessage, this, &ModelingWindow::appendLogMessage);
    connect(&m_state, &ModelingState::versionGraphChanged, m_timeline, &VersionTimeline::setVersionGraph);
    connect(m_timeline, &VersionTimeline::versionActivateRequested, &m_state, &ModelingState::activateVersion);

    rebuildNavigation();
    rebuildProperties();
    updateActionState();
    m_timeline->setVersionGraph(m_state.versionGraph());
}

void ModelingWindow::configureBenchmarkViewport(int _width, int _height) {
    if (m_sceneContainer) {
        m_sceneContainer->setFixedSize(std::max(1, _width), std::max(1, _height));
    }
}

void ModelingWindow::runBenchmarkModelingOperations(int _operationCount) {
    const int operationCount = std::max(1, _operationCount);
    for (int i = 0; i < operationCount; ++i) {
        switch (i % 12) {
        case 0: m_modelingService.handleCreateCuboid(); break;
        case 1: m_modelingService.handleCreateCylinder(); break;
        case 2: m_modelingService.handleCreateSphere(); break;
        case 3: m_modelingService.handleCreateTorus(); break;
        case 4: m_modelingService.handleCreateCone(); break;
        case 5: m_modelingService.handleCreatePyramid(); break;
        case 6: m_modelingService.handleTransform(); break;
        case 7: m_modelingService.handleChamferEdges(); break;
        case 8: m_modelingService.handleBlendEdges(); break;
        case 9: m_modelingService.handleBooleanAdd(); break;
        case 10: m_modelingService.handleBooleanSubtract(); break;
        case 11: m_modelingService.handleBooleanIntersect(); break;
        }
    }
}

double ModelingWindow::runBenchmarkPlotIterations(int _iterationCount) {
    return m_plotWidget ? m_plotWidget->runBenchmarkIterations(_iterationCount) : 0.0;
}

void ModelingWindow::showSceneTab() {
    if (m_centerTabs) {
        m_centerTabs->setCurrentIndex(0);
    }
}

void ModelingWindow::showPlotTab() {
    if (m_centerTabs) {
        m_centerTabs->setCurrentIndex(1);
    }
}

void ModelingWindow::rebuildNavigation() {
    m_navigationTree->blockSignals(true);
    m_navigationTree->clear();

    const QList<QPair<QString, QString>> folders = {
        { QStringLiteral("Circuit Models"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Circuits"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Coordinate Systems"), QStringLiteral("Default/AxisCross") },
        { QStringLiteral("Data Categorization"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Data Processing"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Dataset"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Geometry"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Materials"), QStringLiteral("Default/MaterialVisible") },
        { QStringLiteral("Meshes"), QStringLiteral("Default/MeshModel") },
        { QStringLiteral("Python"), QStringLiteral("Default/python") },
        { QStringLiteral("Solvers"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Test"), QStringLiteral("Default/GeometryVisible") },
        { QStringLiteral("Units"), QStringLiteral("Default/Units") }
    };

    QTreeWidgetItem* geometryRoot = nullptr;
    for (const auto& folder : folders) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_navigationTree);
        item->setText(0, folder.first);
        item->setIcon(0, icon(folder.second));
        if (folder.first == QStringLiteral("Geometry")) {
            geometryRoot = item;
            item->setExpanded(true);
        }
    }

    if (geometryRoot) {
        for (const SceneEntity& entity : m_state.entities()) {
            QTreeWidgetItem* item = new QTreeWidgetItem(geometryRoot);
            item->setText(0, entity.name);
            item->setIcon(0, icon(QStringLiteral("Default/%1").arg(shapeKindIconName(entity.kind))));
            item->setData(0, Qt::UserRole, entity.id);
            item->setSelected(entity.id == m_state.selectedId());
        }
    }

    m_navigationTree->expandAll();
    m_navigationTree->blockSignals(false);
}

void ModelingWindow::rebuildProperties() {
    m_rebuildingProperties = true;
    m_propertiesTree->clear();
    m_propertiesTree->setHeaderLabels(QStringList() << QStringLiteral("Name") << QStringLiteral("Value"));

    const SceneEntity* entity = m_state.selectedEntity();
    if (!entity) {
        QTreeWidgetItem* empty = new QTreeWidgetItem(m_propertiesTree);
        empty->setText(0, QStringLiteral("No selection"));
        empty->setText(1, QStringLiteral(""));
        m_rebuildingProperties = false;
        return;
    }

    QTreeWidgetItem* root = new QTreeWidgetItem(m_propertiesTree);
    root->setText(0, entity->name);
    root->setText(1, shapeKindName(entity->kind));
    root->setExpanded(true);

    QTreeWidgetItem* group = new QTreeWidgetItem(root);
    group->setText(0, QStringLiteral("Group"));
    group->setExpanded(true);
    addProperty(group, QStringLiteral("name"), QStringLiteral("Name"), entity->name);
    addProperty(group, QStringLiteral("parentGroup"), QStringLiteral("Parent Group"), entity->parentGroup);
    addProperty(group, QStringLiteral("material"), QStringLiteral("Material"), entity->material);

    QTreeWidgetItem* appearance = new QTreeWidgetItem(root);
    appearance->setText(0, QStringLiteral("Appearance"));
    appearance->setExpanded(true);
    addProperty(appearance, QStringLiteral("color"), QStringLiteral("Color"), colorToText(entity->color));
    addProperty(appearance, QStringLiteral("surfaceType"), QStringLiteral("Type"), entity->surfaceType);
    addProperty(appearance, QStringLiteral("texture"), QStringLiteral("Texture"), entity->texture);

    QTreeWidgetItem* transform = new QTreeWidgetItem(root);
    transform->setText(0, QStringLiteral("Transformation"));
    transform->setExpanded(true);
    addProperty(transform, QStringLiteral("x"), QStringLiteral("Position X"), numberToText(entity->x));
    addProperty(transform, QStringLiteral("y"), QStringLiteral("Position Y"), numberToText(entity->y));
    addProperty(transform, QStringLiteral("z"), QStringLiteral("Position Z"), numberToText(entity->z));
    addProperty(transform, QStringLiteral("angleDeg"), QStringLiteral("Angle (deg.)"), numberToText(entity->angleDeg));

    QTreeWidgetItem* shape = new QTreeWidgetItem(root);
    shape->setText(0, QStringLiteral("Shape properties"));
    shape->setExpanded(true);
    addProperty(shape, QStringLiteral("sizeX"), QStringLiteral("Size X"), numberToText(entity->sizeX));
    addProperty(shape, QStringLiteral("sizeY"), QStringLiteral("Size Y"), numberToText(entity->sizeY));
    addProperty(shape, QStringLiteral("sizeZ"), QStringLiteral("Size Z"), numberToText(entity->sizeZ));
    addProperty(shape, QStringLiteral("radius"), QStringLiteral("Radius"), numberToText(entity->radius));
    addProperty(shape, QStringLiteral("secondaryRadius"), QStringLiteral("Secondary radius"), numberToText(entity->secondaryRadius));

    QTreeWidgetItem* mesh = new QTreeWidgetItem(root);
    mesh->setText(0, QStringLiteral("Mesh"));
    mesh->setExpanded(true);
    addProperty(mesh, QStringLiteral("considerForMeshing"), QStringLiteral("Consider for meshing"), boolToText(entity->considerForMeshing));
    addProperty(mesh, QStringLiteral("meshPriority"), QStringLiteral("Mesh priority"), QString::number(entity->meshPriority));
    addProperty(mesh, QStringLiteral("meshRefinement"), QStringLiteral("Mesh refinement"), entity->meshRefinement);
    addProperty(mesh, QStringLiteral("maximumEdgeLength"), QStringLiteral("Maximum edge length"), numberToText(entity->maximumEdgeLength));

    m_propertiesTree->expandAll();
    m_propertiesTree->resizeColumnToContents(0);
    m_rebuildingProperties = false;
}

void ModelingWindow::updateActionState() {
    const bool hasSelection = (m_state.selectedEntity() != nullptr);
    for (QAction* action : m_selectionActions) {
        action->setEnabled(hasSelection);
    }
}

void ModelingWindow::appendLogMessage(const QString& _message) {
    if (m_output) {
        m_output->appendPlainText(_message);
    }
}

void ModelingWindow::handleNavigationSelection() {
    const QList<QTreeWidgetItem*> selection = m_navigationTree->selectedItems();
    if (selection.isEmpty()) {
        return;
    }

    const int entityId = selection.first()->data(0, Qt::UserRole).toInt();
    if (entityId > 0) {
        m_state.selectEntity(entityId);
    }
}

void ModelingWindow::handlePropertyChanged(QTreeWidgetItem* _item, int _column) {
    if (m_rebuildingProperties || _column != 1) {
        return;
    }

    const QString key = _item->data(0, Qt::UserRole).toString();
    if (!key.isEmpty()) {
        m_state.setPropertyValue(key, _item->text(1));
    }
}

QWidget* ModelingWindow::createHeader() {
    QWidget* header = new QWidget(this);
    header->setObjectName(QStringLiteral("Header"));
    QHBoxLayout* layout = new QHBoxLayout(header);
    layout->setContentsMargins(4, 0, 8, 0);
    layout->setSpacing(8);

    QLabel* logo = new QLabel(header);
    logo->setPixmap(icon(QStringLiteral("Images/OpenTwinIcon")).pixmap(18, 18));
    layout->addWidget(logo);

    QTabBar* tabs = new QTabBar(header);
    tabs->setDrawBase(false);
    tabs->addTab(QStringLiteral("Start"));
    tabs->addTab(QStringLiteral("View"));
    tabs->addTab(QStringLiteral("Model"));
    tabs->addTab(QStringLiteral("Modeling"));
    tabs->addTab(QStringLiteral("Mesh"));
    tabs->addTab(QStringLiteral("PHREEC"));
    tabs->addTab(QStringLiteral("Import Parameterized Data"));
    tabs->addTab(QStringLiteral("GetDP"));
    tabs->addTab(QStringLiteral("Debug"));
    tabs->addTab(QStringLiteral("OpenEMS"));
    tabs->addTab(QStringLiteral("Data Processing"));
    tabs->addTab(QStringLiteral("Circuit Simulator"));
    tabs->addTab(QStringLiteral("CST Studio Suite Project"));
    tabs->addTab(QStringLiteral("LTSpice Project"));
    tabs->setCurrentIndex(3);
    layout->addWidget(tabs, 1);

    QLabel* status = new QLabel(QStringLiteral("Log Intensity:"), header);
    status->setObjectName(QStringLiteral("HeaderStatus"));
    layout->addWidget(status);

    return header;
}

QWidget* ModelingWindow::createRibbon() {
    QWidget* ribbon = new QWidget(this);
    ribbon->setObjectName(QStringLiteral("Ribbon"));
    QHBoxLayout* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(6, 7, 6, 7);
    layout->setSpacing(8);

    QAction* importStep = createCommand(QStringLiteral("Import STEP"), QStringLiteral("Default/Import"), QKeySequence(QStringLiteral("Ctrl+O")));
    connect(importStep, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleRequestImportSTEP);
    QAction* exportCad = createCommand(QStringLiteral("Export CAD"), QStringLiteral("Default/Export"));
    connect(exportCad, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleRequestExportCAD);
    layout->addWidget(createRibbonGroup(QStringLiteral("Import/Export"), QList<QAction*>() << importStep << exportCad));

    QAction* cuboid = createCommand(QStringLiteral("Cuboid"), QStringLiteral("Default/Cuboid"), QKeySequence(QStringLiteral("Ctrl+Alt+1")));
    connect(cuboid, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreateCuboid);
    QAction* cylinder = createCommand(QStringLiteral("Cylinder"), QStringLiteral("Default/Cylinder"), QKeySequence(QStringLiteral("Ctrl+Alt+2")));
    connect(cylinder, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreateCylinder);
    QAction* sphere = createCommand(QStringLiteral("Sphere"), QStringLiteral("Default/Sphere"), QKeySequence(QStringLiteral("Ctrl+Alt+4")));
    connect(sphere, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreateSphere);
    QAction* torus = createCommand(QStringLiteral("Torus"), QStringLiteral("Default/Torus"), QKeySequence(QStringLiteral("Ctrl+Alt+5")));
    connect(torus, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreateTorus);
    QAction* cone = createCommand(QStringLiteral("Cone"), QStringLiteral("Default/Cone"), QKeySequence(QStringLiteral("Ctrl+Alt+6")));
    connect(cone, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreateCone);
    QAction* pyramid = createCommand(QStringLiteral("Pyramid"), QStringLiteral("Default/Pyramid"), QKeySequence(QStringLiteral("Ctrl+Alt+7")));
    connect(pyramid, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleCreatePyramid);
    layout->addWidget(createRibbonGroup(QStringLiteral("Create"), QList<QAction*>() << cuboid << cylinder << sphere << torus << cone << pyramid));

    QAction* add = createCommand(QStringLiteral("Boolean Add"), QStringLiteral("Default/BooleanAdd"));
    connect(add, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleBooleanAdd);
    QAction* subtract = createCommand(QStringLiteral("Boolean Subtract"), QStringLiteral("Default/BooleanSubtract"));
    connect(subtract, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleBooleanSubtract);
    QAction* intersect = createCommand(QStringLiteral("Boolean Intersect"), QStringLiteral("Default/BooleanIntersect"));
    connect(intersect, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleBooleanIntersect);
    QAction* transform = createCommand(QStringLiteral("Transform"), QStringLiteral("Default/Transform"));
    connect(transform, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleTransform);
    QAction* chamfer = createCommand(QStringLiteral("Chamfer Edges"), QStringLiteral("Default/ChamferEdges"));
    connect(chamfer, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleChamferEdges);
    QAction* blend = createCommand(QStringLiteral("Blend Edges"), QStringLiteral("Default/BlendEdges"));
    connect(blend, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleBlendEdges);
    layout->addWidget(createRibbonGroup(QStringLiteral("Modify"), QList<QAction*>() << add << subtract << intersect << transform << chamfer << blend));
    m_selectionActions << exportCad << add << subtract << intersect << transform << chamfer << blend;

    QAction* create = createCommand(QStringLiteral("Create"), QStringLiteral("Default/AxisCross"));
    connect(create, &QAction::triggered, this, [this]() { appendLogMessage(QStringLiteral("Mock service call: create coordinate system.")); });
    QAction* activate = createCommand(QStringLiteral("Activate"), QStringLiteral("Default/AxisCross"));
    connect(activate, &QAction::triggered, this, [this]() { appendLogMessage(QStringLiteral("Mock service call: activate coordinate system.")); });
    layout->addWidget(createRibbonGroup(QStringLiteral("Coordinate Systems"), QList<QAction*>() << create << activate));

    QAction* removeFaces = createCommand(QStringLiteral("Remove Faces"), QStringLiteral("Default/RemoveFace"));
    connect(removeFaces, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleRemoveFaces);
    QAction* heal = createCommand(QStringLiteral("Heal"), QStringLiteral("Default/Healing"));
    connect(heal, &QAction::triggered, &m_modelingService, &ModelingServiceAdapter::handleHealing);
    layout->addWidget(createRibbonGroup(QStringLiteral("Repair"), QList<QAction*>() << removeFaces << heal));
    m_selectionActions << removeFaces << heal;

    layout->addStretch(1);
    return ribbon;
}

QWidget* ModelingWindow::createRibbonGroup(const QString& _title, const QList<QAction*>& _actions) {
    QFrame* frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("RibbonGroup"));
    QVBoxLayout* groupLayout = new QVBoxLayout(frame);
    groupLayout->setContentsMargins(6, 0, 6, 0);
    groupLayout->setSpacing(3);

    QWidget* buttons = new QWidget(frame);
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(5);

    for (QAction* action : _actions) {
        QToolButton* button = new QToolButton(buttons);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(34, 34));
        button->setFixedSize(86, 72);
        buttonLayout->addWidget(button);
    }

    QLabel* label = new QLabel(_title, frame);
    label->setAlignment(Qt::AlignCenter);
    label->setObjectName(QStringLiteral("RibbonGroupTitle"));

    groupLayout->addWidget(buttons);
    groupLayout->addWidget(label);
    return frame;
}

QAction* ModelingWindow::createCommand(const QString& _text, const QString& _iconName, const QKeySequence& _shortcut) {
    QAction* action = new QAction(icon(_iconName), _text, this);
    if (!_shortcut.isEmpty()) {
        action->setShortcut(_shortcut);
        addAction(action);
    }
    return action;
}

QWidget* ModelingWindow::createNavigationPanel() {
    QWidget* panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("SidePanel"));
    panel->setMinimumWidth(260);
    panel->setMaximumWidth(380);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel* title = new QLabel(QStringLiteral("Project navigation"), panel);
    title->setObjectName(QStringLiteral("PanelTitle"));
    layout->addWidget(title);

    QLineEdit* search = new QLineEdit(panel);
    search->setPlaceholderText(QStringLiteral("Search..."));
    layout->addWidget(search);

    m_navigationTree = new QTreeWidget(panel);
    m_navigationTree->setHeaderHidden(true);
    m_navigationTree->setAnimated(false);
    connect(m_navigationTree, &QTreeWidget::itemSelectionChanged, this, &ModelingWindow::handleNavigationSelection);
    layout->addWidget(m_navigationTree, 1);

    return panel;
}

QWidget* ModelingWindow::createRightPanel() {
    QTabWidget* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("RightTabs"));
    tabs->setMinimumWidth(300);
    tabs->setMaximumWidth(430);

    QTreeWidget* picker = new QTreeWidget(tabs);
    picker->setHeaderLabels(QStringList() << QStringLiteral("Name") << QStringLiteral("Value"));
    QTreeWidgetItem* group = new QTreeWidgetItem(picker);
    group->setText(0, QStringLiteral("Modeling"));
    group->setExpanded(true);
    const QStringList blocks = {
        QStringLiteral("Primitive: Cuboid"),
        QStringLiteral("Primitive: Cylinder"),
        QStringLiteral("Primitive: Sphere"),
        QStringLiteral("Boolean operation"),
        QStringLiteral("Transform mode"),
        QStringLiteral("Repair operation")
    };
    for (const QString& block : blocks) {
        QTreeWidgetItem* item = new QTreeWidgetItem(group);
        item->setText(0, block);
        item->setText(1, QStringLiteral("mock"));
    }
    picker->expandAll();
    tabs->addTab(picker, QStringLiteral("Block Picker"));

    m_propertiesTree = new QTreeWidget(tabs);
    m_propertiesTree->setAlternatingRowColors(false);
    m_propertiesTree->setRootIsDecorated(true);
    m_propertiesTree->header()->setStretchLastSection(true);
    connect(m_propertiesTree, &QTreeWidget::itemChanged, this, &ModelingWindow::handlePropertyChanged);
    tabs->addTab(m_propertiesTree, QStringLiteral("Properties"));
    tabs->setCurrentWidget(m_propertiesTree);

    return tabs;
}

QWidget* ModelingWindow::createBottomPanel() {
    QTabWidget* tabs = new QTabWidget(this);
    tabs->setMinimumHeight(150);

    QWidget* outputPage = new QWidget(tabs);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputPage);
    outputLayout->setContentsMargins(8, 8, 8, 8);
    QLineEdit* find = new QLineEdit(outputPage);
    find->setPlaceholderText(QStringLiteral("Find..."));
    outputLayout->addWidget(find);
    m_output = new QPlainTextEdit(outputPage);
    m_output->setReadOnly(true);
    outputLayout->addWidget(m_output, 1);
    tabs->addTab(outputPage, QStringLiteral("Output"));

    m_timeline = new VersionTimeline(tabs);
    tabs->addTab(m_timeline, QStringLiteral("Versions"));
    tabs->setCurrentWidget(m_timeline);

    return tabs;
}

void ModelingWindow::addPropertyGroup(QTreeWidgetItem* _root, const QString& _name) {
    QTreeWidgetItem* group = new QTreeWidgetItem(_root);
    group->setText(0, _name);
    group->setExpanded(true);
}

void ModelingWindow::addProperty(QTreeWidgetItem* _group, const QString& _key, const QString& _name, const QString& _value, bool _editable) {
    QTreeWidgetItem* item = new QTreeWidgetItem(_group);
    item->setText(0, _name);
    item->setText(1, _value);
    item->setData(0, Qt::UserRole, _key);
    if (_editable) {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

QIcon ModelingWindow::icon(const QString& _iconName) const {
    return QIcon(QStringLiteral(":/icons/%1.png").arg(_iconName));
}

void ModelingWindow::applyStyle() {
    qApp->setStyle(QStringLiteral("Fusion"));
    qApp->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget {
            background: #242424;
            color: #f2f2f2;
            font-size: 12px;
        }
        QWidget#Header {
            background: #151515;
            border-bottom: 1px solid #3a3a3a;
            min-height: 27px;
        }
        QLabel#HeaderStatus {
            color: #f4f4f4;
            padding-right: 6px;
        }
        QTabBar::tab {
            background: transparent;
            color: #f4f4f4;
            padding: 5px 8px;
            border: 1px solid transparent;
        }
        QTabBar::tab:selected {
            border: 1px solid #d7d7d7;
            border-bottom-color: #242424;
            background: #1e1e1e;
        }
        QWidget#Ribbon {
            background: #262626;
            border-bottom: 1px solid #595959;
        }
        QFrame#RibbonGroup {
            border-right: 1px solid #545454;
        }
        QLabel#RibbonGroupTitle {
            color: #ffffff;
            font-size: 11px;
        }
        QToolButton {
            border: 1px solid transparent;
            padding: 2px;
            color: #ffffff;
        }
        QToolButton:hover {
            background: #333c40;
            border: 1px solid #00d7ff;
        }
        QToolButton:disabled {
            color: #7d7d7d;
        }
        QWidget#SidePanel, QTabWidget#RightTabs {
            background: #252525;
        }
        QLabel#PanelTitle {
            background: #202020;
            border-top: 1px solid #7a7a7a;
            border-bottom: 1px solid #7a7a7a;
            padding: 4px 6px;
            font-weight: bold;
        }
        QLineEdit {
            background: #2a2a2a;
            border: 1px solid #4d4d4d;
            color: #f3f3f3;
            min-height: 24px;
            padding: 2px 6px;
        }
        QTreeWidget, QPlainTextEdit {
            background: #242424;
            alternate-background-color: #2f2f2f;
            border: 1px solid #3f3f3f;
            color: #ffffff;
            selection-background-color: #00e000;
            selection-color: #000000;
        }
        QHeaderView::section {
            background: #3f3f3f;
            color: #ffffff;
            border: none;
            padding: 4px;
        }
        QSplitter::handle {
            background: #4b4b4b;
        }
        QTabWidget::pane {
            border: 1px solid #555555;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #262626;
            width: 12px;
            height: 12px;
        }
        QScrollBar::handle {
            background: #555555;
            min-height: 24px;
        }
    )"));
}

} // namespace prototype
