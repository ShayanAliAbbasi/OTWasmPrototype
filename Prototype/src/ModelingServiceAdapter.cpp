// @otlicense
// File: ModelingServiceAdapter.cpp
// @otlicense-end

#include "ModelingServiceAdapter.h"

#include "ModelingState.h"

namespace prototype {

ModelingServiceAdapter::ModelingServiceAdapter(ModelingState& _state, QObject* _parent)
    : QObject(_parent),
      m_state(_state)
{
}

void ModelingServiceAdapter::handleRequestImportSTEP() {
    m_state.importStepMock();
}

void ModelingServiceAdapter::handleRequestExportCAD() {
    m_state.exportCadMock();
}

void ModelingServiceAdapter::handleCreateCuboid() {
    m_state.createPrimitive(ShapeKind::Cuboid);
}

void ModelingServiceAdapter::handleCreateCylinder() {
    m_state.createPrimitive(ShapeKind::Cylinder);
}

void ModelingServiceAdapter::handleCreateSphere() {
    m_state.createPrimitive(ShapeKind::Sphere);
}

void ModelingServiceAdapter::handleCreateTorus() {
    m_state.createPrimitive(ShapeKind::Torus);
}

void ModelingServiceAdapter::handleCreateCone() {
    m_state.createPrimitive(ShapeKind::Cone);
}

void ModelingServiceAdapter::handleCreatePyramid() {
    m_state.createPrimitive(ShapeKind::Pyramid);
}

void ModelingServiceAdapter::handleBooleanAdd() {
    m_state.booleanOperation(QStringLiteral("Boolean Add"));
}

void ModelingServiceAdapter::handleBooleanSubtract() {
    m_state.booleanOperation(QStringLiteral("Boolean Subtract"));
}

void ModelingServiceAdapter::handleBooleanIntersect() {
    m_state.booleanOperation(QStringLiteral("Boolean Intersect"));
}

void ModelingServiceAdapter::handleTransform() {
    m_state.transformSelected();
}

void ModelingServiceAdapter::handleChamferEdges() {
    m_state.chamferSelected();
}

void ModelingServiceAdapter::handleBlendEdges() {
    m_state.blendSelected();
}

void ModelingServiceAdapter::handleRemoveFaces() {
    m_state.removeFacesSelected();
}

void ModelingServiceAdapter::handleHealing() {
    m_state.healSelected();
}

} // namespace prototype
