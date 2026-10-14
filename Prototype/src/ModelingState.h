// @otlicense
// File: ModelingState.h
//
// License:
// Copyright 2025 by OpenTwin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// @otlicense-end

#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QVector>

namespace prototype {

enum class ShapeKind {
    Cuboid,
    Cylinder,
    Sphere,
    Torus,
    Cone,
    Pyramid,
    BooleanResult,
    ImportedStep
};

QString shapeKindName(ShapeKind _kind);
QString shapeKindIconName(ShapeKind _kind);

struct SceneEntity {
    int id = 0;
    QString name;
    ShapeKind kind = ShapeKind::Cuboid;
    QColor color = QColor(153, 153, 242);
    QString parentGroup = QStringLiteral("Geometry");
    QString material = QStringLiteral("Aluminium");
    QString surfaceType = QStringLiteral("Rough");
    QString texture = QStringLiteral("None");
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double sizeX = 3.0;
    double sizeY = 3.0;
    double sizeZ = 3.0;
    double radius = 1.5;
    double secondaryRadius = 0.45;
    double angleDeg = 0.0;
    bool considerForMeshing = true;
    int meshPriority = 0;
    QString meshRefinement = QStringLiteral("Global setting");
    double maximumEdgeLength = 0.0;
};

struct VersionGraphNode {
    QString name;
    QString label;
    QString description;
    QString parentVersion;
    bool directParentIsHidden = false;
};

using VersionGraphBranch = QVector<VersionGraphNode>;

struct VersionGraphData {
    QString activeVersionName;
    QString activeBranchName;
    QVector<VersionGraphBranch> branches;
};

class ModelingState : public QObject {
    Q_OBJECT

public:
    explicit ModelingState(QObject* _parent = nullptr);

    const QVector<SceneEntity>& entities() const { return m_entities; }
    int selectedId() const { return m_selectedId; }
    int version() const { return m_version; }
    const VersionGraphData& versionGraph() const { return m_versionGraph; }

    const SceneEntity* selectedEntity() const;
    SceneEntity* selectedEntity();

public slots:
    void resetBenchmarkScene(int _entityCount);
    void createPrimitive(ShapeKind _kind);
    void importStepMock();
    void exportCadMock();
    void selectEntity(int _id);
    void clearSelection();
    void transformSelected();
    void booleanOperation(const QString& _operationName);
    void chamferSelected();
    void blendSelected();
    void removeFacesSelected();
    void healSelected();
    void setPropertyValue(const QString& _propertyKey, const QString& _value);
    void activateVersion(const QString& _versionName);
    void cancelActiveInteraction();
    void acceptActiveInteraction();

signals:
    void sceneChanged();
    void selectionChanged();
    void logMessage(const QString& _message);
    void versionChanged(int _version);
    void versionGraphChanged(const VersionGraphData& _graph);

private:
    SceneEntity createDefaultEntity(ShapeKind _kind) const;
    QString uniqueName(const QString& _baseName) const;
    void appendEntity(const SceneEntity& _entity, const QString& _operationText);
    void touchVersion(const QString& _operationText);
    void emitSelectionState();
    void initializeVersionGraph();
    VersionGraphBranch* findBranch(const QString& _branchName);
    const VersionGraphBranch* findBranch(const QString& _branchName) const;
    VersionGraphNode* findVersionNode(const QString& _versionName);
    const VersionGraphNode* findVersionNode(const QString& _versionName) const;
    QString nextVersionNameFromActive() const;
    bool activeVersionIsEndOfBranch() const;
    int branchesCountFromNode(const QString& _versionName) const;
    void insertVersionNode(const VersionGraphNode& _node);

    QVector<SceneEntity> m_entities;
    VersionGraphData m_versionGraph;
    int m_nextId = 1;
    int m_selectedId = -1;
    int m_version = 1;
};

} // namespace prototype
