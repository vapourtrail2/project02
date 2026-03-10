#pragma once

#include "AppState.h"
#include <array>
#include <cstring>
#include <memory>
#include <utility>
#include <vtkMatrix4x4.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

class VolumeTransformService {
public:
    explicit VolumeTransformService(std::shared_ptr<SharedInteractionState> state)
        : m_sharedState(std::move(state))
    {
        m_cachedModelMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        m_cachedInverseModelMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
        m_cachedModelMatrix->Identity();
        m_cachedInverseModelMatrix->Identity();
    }

    void WorldToModel(const double worldPos[3], double modelPos[3]) const
    {
        double inPos[4] = { worldPos[0], worldPos[1], worldPos[2], 1.0 };
        double outPos[4] = { 0.0, 0.0, 0.0, 1.0 };
        m_cachedInverseModelMatrix->MultiplyPoint(inPos, outPos);
        if (outPos[3] != 0.0) {
            modelPos[0] = outPos[0] / outPos[3];
            modelPos[1] = outPos[1] / outPos[3];
            modelPos[2] = outPos[2] / outPos[3];
        }
    }

    void ModelToWorld(const double modelPos[3], double worldPos[3]) const
    {
        double inPos[4] = { modelPos[0], modelPos[1], modelPos[2], 1.0 };
        double outPos[4] = { 0.0, 0.0, 0.0, 1.0 };
        m_cachedModelMatrix->MultiplyPoint(inPos, outPos);
        if (outPos[3] != 0.0) {
            worldPos[0] = outPos[0] / outPos[3];
            worldPos[1] = outPos[1] / outPos[3];
            worldPos[2] = outPos[2] / outPos[3];
        }
    }

    void SyncModelMatrix(vtkMatrix4x4* mat)
    {
        if (!mat || !m_sharedState) {
            return;
        }

        std::array<double, 16> matData{};
        std::memcpy(matData.data(), mat->GetData(), 16 * sizeof(double));
        m_sharedState->SetModelMatrix(matData);

        m_cachedModelMatrix->DeepCopy(mat);
        m_cachedInverseModelMatrix->DeepCopy(mat);
        m_cachedInverseModelMatrix->Invert();
    }

    void TransformModel(double translate[3], double rotate[3], double scale[3])
    {
        if (!m_sharedState) {
            return;
        }

        auto transform = vtkSmartPointer<vtkTransform>::New();
        transform->PostMultiply();
        transform->RotateX(rotate[0]);
        transform->RotateY(rotate[1]);
        transform->RotateZ(rotate[2]);
        transform->Scale(scale[0], scale[1], scale[2]);
        transform->Translate(translate[0], translate[1], translate[2]);

        std::array<double, 16> matData{};
        std::memcpy(matData.data(), transform->GetMatrix()->GetData(), 16 * sizeof(double));
        m_sharedState->SetModelMatrix(matData);

        m_cachedModelMatrix->DeepCopy(transform->GetMatrix());
        m_cachedInverseModelMatrix->DeepCopy(transform->GetMatrix());
        m_cachedInverseModelMatrix->Invert();
    }

    void ResetModelTransform()
    {
        if (!m_sharedState) {
            return;
        }

        const std::array<double, 16> identity = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        m_sharedState->SetModelMatrix(identity);
        m_cachedModelMatrix->Identity();
        m_cachedInverseModelMatrix->Identity();
    }

    const vtkMatrix4x4* GetCachedModelMatrix() const { return m_cachedModelMatrix; }
    const vtkMatrix4x4* GetCachedInverseModelMatrix() const { return m_cachedInverseModelMatrix; }

private:
    std::shared_ptr<SharedInteractionState> m_sharedState;
    vtkSmartPointer<vtkMatrix4x4> m_cachedModelMatrix;
    vtkSmartPointer<vtkMatrix4x4> m_cachedInverseModelMatrix;
};
