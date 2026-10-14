// @otlicense
// File: ModelingState.cpp
// @otlicense-end

#include "ModelingState.h"

#include <QRegularExpression>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace prototype {
namespace {

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

QString incrementVersionName(const QString& _versionName) {
    if (_versionName.isEmpty()) {
        return QStringLiteral("1");
    }

    const int ix = _versionName.lastIndexOf(QLatin1Char('.'));
    const QString prefix = ix < 0 ? QString() : _versionName.left(ix + 1);
    return prefix + QString::number(versionNumber(_versionName) + 1);
}

} // namespace

QString shapeKindName(ShapeKind _kind) {
    switch (_kind) {
    case ShapeKind::Cuboid: return QStringLiteral("Cuboid");
    case ShapeKind::Cylinder: return QStringLiteral("Cylinder");
    case ShapeKind::Sphere: return QStringLiteral("Sphere");
    case ShapeKind::Torus: return QStringLiteral("Torus");
    case ShapeKind::Cone: return QStringLiteral("Cone");
    case ShapeKind::Pyramid: return QStringLiteral("Pyramid");
    case ShapeKind::BooleanResult: return QStringLiteral("Boolean");
    case ShapeKind::ImportedStep: return QStringLiteral("Imported STEP");
    }
    return QStringLiteral("Geometry");
}

QString shapeKindIconName(ShapeKind _kind) {
    switch (_kind) {
    case ShapeKind::Cuboid: return QStringLiteral("Cuboid");
    case ShapeKind::Cylinder: return QStringLiteral("Cylinder");
    case ShapeKind::Sphere: return QStringLiteral("Sphere");
    case ShapeKind::Torus: return QStringLiteral("Torus");
    case ShapeKind::Cone: return QStringLiteral("Cone");
    case ShapeKind::Pyramid: return QStringLiteral("Pyramid");
    case ShapeKind::BooleanResult: return QStringLiteral("BooleanAdd");
    case ShapeKind::ImportedStep: return QStringLiteral("Import");
    }
    return QStringLiteral("GeometryVisible");
}

ModelingState::ModelingState(QObject* _parent)
    : QObject(_parent)
{
    initializeVersionGraph();

    SceneEntity cylinder = createDefaultEntity(ShapeKind::Cylinder);
    cylinder.id = m_nextId++;
    cylinder.name = QStringLiteral("Cylinder");
    cylinder.x = -1.2;
    cylinder.y = -0.3;
    cylinder.z = 0.0;
    m_entities.push_back(cylinder);

    SceneEntity sphere = createDefaultEntity(ShapeKind::Sphere);
    sphere.id = m_nextId++;
    sphere.name = QStringLiteral("Sphere");
    sphere.x = 2.4;
    sphere.y = 1.1;
    sphere.z = 0.3;
    sphere.radius = 1.2;
    sphere.color = QColor(140, 145, 206);
    m_entities.push_back(sphere);

    m_selectedId = cylinder.id;
    emit logMessage(QStringLiteral("Loaded mock Modeling scene with 2 geometry entities."));
}

const SceneEntity* ModelingState::selectedEntity() const {
    for (const SceneEntity& entity : m_entities) {
        if (entity.id == m_selectedId) {
            return &entity;
        }
    }
    return nullptr;
}

SceneEntity* ModelingState::selectedEntity() {
    for (SceneEntity& entity : m_entities) {
        if (entity.id == m_selectedId) {
            return &entity;
        }
    }
    return nullptr;
}

void ModelingState::resetBenchmarkScene(int _entityCount) {
    const int entityCount = std::max(1, _entityCount);
    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(entityCount))));
    const double spacing = 3.2;
    const double offset = static_cast<double>(columns - 1) * spacing * 0.5;
    const ShapeKind kinds[] = {
        ShapeKind::Cuboid,
        ShapeKind::Cylinder,
        ShapeKind::Sphere,
        ShapeKind::Torus,
        ShapeKind::Cone,
        ShapeKind::Pyramid
    };

    m_entities.clear();
    m_entities.reserve(entityCount);
    m_nextId = 1;
    m_selectedId = -1;
    m_version = 1;
    initializeVersionGraph();

    for (int i = 0; i < entityCount; ++i) {
        SceneEntity entity = createDefaultEntity(kinds[i % 6]);
        entity.id = m_nextId++;
        entity.name = QStringLiteral("Benchmark_%1_%2")
            .arg(shapeKindName(entity.kind))
            .arg(entity.id, 3, 10, QLatin1Char('0'));
        entity.x = static_cast<double>(i % columns) * spacing - offset;
        entity.y = static_cast<double>(i / columns) * spacing - offset;
        entity.z = static_cast<double>(i % 3) * 0.15;
        m_entities.push_back(entity);
    }

    m_selectedId = m_entities.front().id;
    emit sceneChanged();
    emitSelectionState();
    emit versionGraphChanged(m_versionGraph);
}

void ModelingState::createPrimitive(ShapeKind _kind) {
    SceneEntity entity = createDefaultEntity(_kind);
    entity.id = m_nextId++;
    entity.name = uniqueName(shapeKindName(_kind));

    const int placementIndex = m_entities.size();
    entity.x = -2.0 + static_cast<double>(placementIndex % 5) * 1.8;
    entity.y = 0.8 + static_cast<double>(placementIndex / 5) * 1.8;

    appendEntity(entity, QStringLiteral("create primitive: %1").arg(entity.name));
}

void ModelingState::importStepMock() {
    SceneEntity entity = createDefaultEntity(ShapeKind::ImportedStep);
    entity.id = m_nextId++;
    entity.name = uniqueName(QStringLiteral("Imported_STEP"));
    entity.sizeX = 4.2;
    entity.sizeY = 2.4;
    entity.sizeZ = 1.4;
    entity.x = 1.7;
    entity.y = -1.6;
    entity.color = QColor(118, 185, 162);

    appendEntity(entity, QStringLiteral("mock import STEP file: %1").arg(entity.name));
}

void ModelingState::exportCadMock() {
    const SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Export CAD requested, but no geometry is selected."));
        return;
    }
    emit logMessage(QStringLiteral("Mock service call: export CAD for %1.").arg(entity->name));
}

void ModelingState::selectEntity(int _id) {
    if (m_selectedId == _id) {
        return;
    }
    m_selectedId = _id;
    emitSelectionState();
}

void ModelingState::clearSelection() {
    if (m_selectedId < 0) {
        return;
    }
    m_selectedId = -1;
    emitSelectionState();
}

void ModelingState::transformSelected() {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Transform requested, but no geometry is selected."));
        return;
    }

    entity->x += 0.8;
    entity->y += 0.45;
    entity->angleDeg = std::fmod(entity->angleDeg + 15.0, 360.0);
    touchVersion(QStringLiteral("transform %1").arg(entity->name));
}

void ModelingState::booleanOperation(const QString& _operationName) {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("%1 requested, but no base geometry is selected.").arg(_operationName));
        return;
    }

    QString operationName = _operationName;
    operationName.remove(QLatin1Char(' '));

    SceneEntity result = *entity;
    result.id = m_nextId++;
    result.name = uniqueName(QStringLiteral("%1_Result").arg(operationName));
    result.kind = ShapeKind::BooleanResult;
    result.color = QColor(96, 199, 132);
    result.x += 1.1;
    result.y += 0.6;
    appendEntity(result, QStringLiteral("mock %1 with base %2").arg(_operationName, entity->name));
}

void ModelingState::chamferSelected() {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Chamfer Edges requested, but no geometry is selected."));
        return;
    }
    entity->surfaceType = QStringLiteral("Chamfered");
    touchVersion(QStringLiteral("mock chamfer edges on %1").arg(entity->name));
}

void ModelingState::blendSelected() {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Blend Edges requested, but no geometry is selected."));
        return;
    }
    entity->surfaceType = QStringLiteral("Blended");
    touchVersion(QStringLiteral("mock blend edges on %1").arg(entity->name));
}

void ModelingState::removeFacesSelected() {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Remove Faces requested, but no geometry is selected."));
        return;
    }
    entity->meshPriority += 1;
    touchVersion(QStringLiteral("mock remove faces on %1").arg(entity->name));
}

void ModelingState::healSelected() {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        emit logMessage(QStringLiteral("Heal requested, but no geometry is selected."));
        return;
    }
    entity->meshRefinement = QStringLiteral("Repaired");
    touchVersion(QStringLiteral("mock heal geometry %1").arg(entity->name));
}

void ModelingState::setPropertyValue(const QString& _propertyKey, const QString& _value) {
    SceneEntity* entity = selectedEntity();
    if (!entity) {
        return;
    }

    bool ok = false;
    const double numericValue = _value.toDouble(&ok);

    if (_propertyKey == QStringLiteral("name")) {
        entity->name = _value.trimmed().isEmpty() ? entity->name : _value.trimmed();
    }
    else if (_propertyKey == QStringLiteral("parentGroup")) {
        entity->parentGroup = _value.trimmed().isEmpty() ? QStringLiteral("Geometry") : _value.trimmed();
    }
    else if (_propertyKey == QStringLiteral("material")) {
        entity->material = _value;
    }
    else if (_propertyKey == QStringLiteral("surfaceType")) {
        entity->surfaceType = _value;
    }
    else if (_propertyKey == QStringLiteral("texture")) {
        entity->texture = _value;
    }
    else if (_propertyKey == QStringLiteral("color")) {
        const QRegularExpression separator(QStringLiteral("[,; ]+"));
        const QStringList channels = _value.split(separator, Qt::SkipEmptyParts);
        if (channels.size() >= 3) {
            bool rOk = false;
            bool gOk = false;
            bool bOk = false;
            const int r = channels.at(0).toInt(&rOk);
            const int g = channels.at(1).toInt(&gOk);
            const int b = channels.at(2).toInt(&bOk);
            if (rOk && gOk && bOk) {
                entity->color = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
            }
        }
    }
    else if (_propertyKey == QStringLiteral("x") && ok) { entity->x = numericValue; }
    else if (_propertyKey == QStringLiteral("y") && ok) { entity->y = numericValue; }
    else if (_propertyKey == QStringLiteral("z") && ok) { entity->z = numericValue; }
    else if (_propertyKey == QStringLiteral("sizeX") && ok) { entity->sizeX = qMax(0.1, numericValue); }
    else if (_propertyKey == QStringLiteral("sizeY") && ok) { entity->sizeY = qMax(0.1, numericValue); }
    else if (_propertyKey == QStringLiteral("sizeZ") && ok) { entity->sizeZ = qMax(0.1, numericValue); }
    else if (_propertyKey == QStringLiteral("radius") && ok) { entity->radius = qMax(0.1, numericValue); }
    else if (_propertyKey == QStringLiteral("secondaryRadius") && ok) { entity->secondaryRadius = qMax(0.05, numericValue); }
    else if (_propertyKey == QStringLiteral("angleDeg") && ok) { entity->angleDeg = numericValue; }
    else if (_propertyKey == QStringLiteral("considerForMeshing")) {
        entity->considerForMeshing = (_value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || _value == QStringLiteral("1"));
    }
    else if (_propertyKey == QStringLiteral("meshPriority")) {
        int priority = _value.toInt(&ok);
        if (ok) {
            entity->meshPriority = priority;
        }
    }
    else if (_propertyKey == QStringLiteral("meshRefinement")) {
        entity->meshRefinement = _value;
    }
    else if (_propertyKey == QStringLiteral("maximumEdgeLength") && ok) {
        entity->maximumEdgeLength = qMax(0.0, numericValue);
    }
    else {
        return;
    }

    touchVersion(QStringLiteral("edit property %1 on %2").arg(_propertyKey, entity->name));
}

void ModelingState::activateVersion(const QString& _versionName) {
    if (_versionName == m_versionGraph.activeVersionName) {
        return;
    }

    const VersionGraphNode* node = findVersionNode(_versionName);
    if (!node) {
        emit logMessage(QStringLiteral("Version activation requested for unknown version %1.").arg(_versionName));
        return;
    }

    m_versionGraph.activeVersionName = node->name;
    m_versionGraph.activeBranchName = versionBranchName(node->name);
    emit logMessage(QStringLiteral("Activated model version %1. Future model changes will continue or branch from this version.").arg(node->name));
    emit versionGraphChanged(m_versionGraph);
}

void ModelingState::cancelActiveInteraction() {
    emit logMessage(QStringLiteral("Viewer command: ESC pressed. Mock rubberband/manipulator command cancelled."));
}

void ModelingState::acceptActiveInteraction() {
    emit logMessage(QStringLiteral("Viewer command: RETURN pressed. Mock rubberband/manipulator command accepted."));
}

SceneEntity ModelingState::createDefaultEntity(ShapeKind _kind) const {
    SceneEntity entity;
    entity.kind = _kind;
    entity.name = shapeKindName(_kind);

    switch (_kind) {
    case ShapeKind::Cuboid:
        entity.sizeX = 3.4;
        entity.sizeY = 2.5;
        entity.sizeZ = 1.7;
        entity.color = QColor(77, 183, 234);
        break;
    case ShapeKind::Cylinder:
        entity.radius = 1.1;
        entity.sizeZ = 1.8;
        entity.color = QColor(153, 153, 242);
        break;
    case ShapeKind::Sphere:
        entity.radius = 1.1;
        entity.sizeZ = 2.2;
        entity.color = QColor(116, 125, 221);
        break;
    case ShapeKind::Torus:
        entity.radius = 1.2;
        entity.secondaryRadius = 0.35;
        entity.color = QColor(91, 126, 206);
        break;
    case ShapeKind::Cone:
        entity.radius = 1.2;
        entity.sizeZ = 2.2;
        entity.color = QColor(128, 206, 214);
        break;
    case ShapeKind::Pyramid:
        entity.sizeX = 2.8;
        entity.sizeY = 2.8;
        entity.sizeZ = 2.0;
        entity.color = QColor(73, 135, 164);
        break;
    case ShapeKind::BooleanResult:
        entity.color = QColor(96, 199, 132);
        break;
    case ShapeKind::ImportedStep:
        entity.color = QColor(118, 185, 162);
        break;
    }

    return entity;
}

QString ModelingState::uniqueName(const QString& _baseName) const {
    QString candidate = _baseName;
    int suffix = 1;
    bool exists = true;
    while (exists) {
        exists = false;
        for (const SceneEntity& entity : m_entities) {
            if (entity.name == candidate) {
                exists = true;
                candidate = QStringLiteral("%1_%2").arg(_baseName).arg(suffix++);
                break;
            }
        }
    }
    return candidate;
}

void ModelingState::appendEntity(const SceneEntity& _entity, const QString& _operationText) {
    m_entities.push_back(_entity);
    m_selectedId = _entity.id;
    touchVersion(_operationText);
    emitSelectionState();
}

void ModelingState::touchVersion(const QString& _operationText) {
    const QString parentVersion = m_versionGraph.activeVersionName;
    const QString newVersionName = nextVersionNameFromActive();

    VersionGraphNode node;
    node.name = newVersionName;
    node.label = _operationText;
    node.description = QStringLiteral("Mock model state after %1").arg(_operationText);
    node.parentVersion = parentVersion;
    insertVersionNode(node);

    m_versionGraph.activeVersionName = newVersionName;
    m_versionGraph.activeBranchName = versionBranchName(newVersionName);

    ++m_version;
    emit logMessage(QStringLiteral("Version %1: %2.").arg(newVersionName, _operationText));
    emit versionChanged(m_version);
    emit versionGraphChanged(m_versionGraph);
    emit sceneChanged();
}

void ModelingState::emitSelectionState() {
    const SceneEntity* entity = selectedEntity();
    if (entity) {
        emit logMessage(QStringLiteral("Selected %1.").arg(entity->name));
    }
    emit selectionChanged();
    emit sceneChanged();
}

void ModelingState::initializeVersionGraph() {
    VersionGraphNode initialVersion;
    initialVersion.name = QStringLiteral("1");
    initialVersion.label = QStringLiteral("Initial");
    initialVersion.description = QStringLiteral("Initial mock model state");
    initialVersion.parentVersion = QString();

    VersionGraphBranch mainBranch;
    mainBranch.push_back(initialVersion);

    m_versionGraph.branches.clear();
    m_versionGraph.branches.push_back(mainBranch);
    m_versionGraph.activeVersionName = initialVersion.name;
    m_versionGraph.activeBranchName = QString();
}

VersionGraphBranch* ModelingState::findBranch(const QString& _branchName) {
    for (VersionGraphBranch& branch : m_versionGraph.branches) {
        if (!branch.isEmpty() && versionBranchName(branch.front().name) == _branchName) {
            return &branch;
        }
    }
    return nullptr;
}

const VersionGraphBranch* ModelingState::findBranch(const QString& _branchName) const {
    for (const VersionGraphBranch& branch : m_versionGraph.branches) {
        if (!branch.isEmpty() && versionBranchName(branch.front().name) == _branchName) {
            return &branch;
        }
    }
    return nullptr;
}

VersionGraphNode* ModelingState::findVersionNode(const QString& _versionName) {
    for (VersionGraphBranch& branch : m_versionGraph.branches) {
        for (VersionGraphNode& node : branch) {
            if (node.name == _versionName) {
                return &node;
            }
        }
    }
    return nullptr;
}

const VersionGraphNode* ModelingState::findVersionNode(const QString& _versionName) const {
    for (const VersionGraphBranch& branch : m_versionGraph.branches) {
        for (const VersionGraphNode& node : branch) {
            if (node.name == _versionName) {
                return &node;
            }
        }
    }
    return nullptr;
}

QString ModelingState::nextVersionNameFromActive() const {
    if (activeVersionIsEndOfBranch()) {
        return incrementVersionName(m_versionGraph.activeVersionName);
    }

    const int branchNumber = branchesCountFromNode(m_versionGraph.activeVersionName) + 1;
    return QStringLiteral("%1.%2.1").arg(m_versionGraph.activeVersionName).arg(branchNumber);
}

bool ModelingState::activeVersionIsEndOfBranch() const {
    const VersionGraphBranch* branch = findBranch(m_versionGraph.activeBranchName);
    return branch && !branch->isEmpty() && branch->back().name == m_versionGraph.activeVersionName;
}

int ModelingState::branchesCountFromNode(const QString& _versionName) const {
    int count = 0;
    for (const VersionGraphBranch& branch : m_versionGraph.branches) {
        if (!branch.isEmpty() && versionBranchNodeName(branch.front().name) == _versionName) {
            ++count;
        }
    }
    return count;
}

void ModelingState::insertVersionNode(const VersionGraphNode& _node) {
    const QString branchName = versionBranchName(_node.name);
    VersionGraphBranch* branch = findBranch(branchName);
    if (branch) {
        auto it = std::find_if(branch->begin(), branch->end(), [&_node](const VersionGraphNode& _existing) {
            return versionNumber(_existing.name) > versionNumber(_node.name);
        });
        branch->insert(it, _node);
    }
    else {
        VersionGraphBranch newBranch;
        newBranch.push_back(_node);
        m_versionGraph.branches.push_back(newBranch);
    }
}

} // namespace prototype
