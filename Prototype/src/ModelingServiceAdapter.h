// @otlicense
// File: ModelingServiceAdapter.h
// @otlicense-end

#pragma once

#include <QObject>

namespace prototype {

class ModelingState;

class ModelingServiceAdapter : public QObject {
    Q_OBJECT

public:
    explicit ModelingServiceAdapter(ModelingState& _state, QObject* _parent = nullptr);

public slots:
    void handleRequestImportSTEP();
    void handleRequestExportCAD();

    void handleCreateCuboid();
    void handleCreateCylinder();
    void handleCreateSphere();
    void handleCreateTorus();
    void handleCreateCone();
    void handleCreatePyramid();

    void handleBooleanAdd();
    void handleBooleanSubtract();
    void handleBooleanIntersect();
    void handleTransform();
    void handleChamferEdges();
    void handleBlendEdges();

    void handleRemoveFaces();
    void handleHealing();

private:
    ModelingState& m_state;
};

} // namespace prototype
