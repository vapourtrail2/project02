#pragma once
#include "AppInterfaces.h"
#include <vtkActor.h>
#include <vtkVolume.h>
#include <vtkImageSlice.h>
#include <vtkImageResliceMapper.h>
#include <vtkLineSource.h>
#include <vtkPlane.h>
#include <vtkPlaneSource.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyDataMapper.h>
#include <vtkCubeAxesActor.h>
#include <vtkFlyingEdges3D.h>

class IsoSurfaceStrategy : public AbstractVisualStrategy {
public:
    IsoSurfaceStrategy();

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetupCamera(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;
    vtkProp3D* GetMainProp() override;

private:
    vtkSmartPointer<vtkActor> m_actor;
    vtkSmartPointer<vtkCubeAxesActor> m_cubeAxes;
    vtkSmartPointer<vtkImageData> m_sourceImage;
    vtkSmartPointer<vtkFlyingEdges3D> m_isoExtractor;
    vtkSmartPointer<vtkPolyDataMapper> m_isoMapper;

    double m_lastIsoValue = 0.0;
    bool m_hasLastIsoValue = false;
};

class VolumeStrategy : public AbstractVisualStrategy {
public:
    VolumeStrategy();

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetupCamera(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;
    vtkProp3D* GetMainProp() override;

private:
    vtkSmartPointer<vtkCubeAxesActor> m_cubeAxes;
    vtkSmartPointer<vtkVolume> m_volume;
};

class SliceStrategy : public AbstractVisualStrategy {
public:
    SliceStrategy(Orientation orient);

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetupCamera(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;
    int GetNavigationAxis() const override { return (int)GetOrientation(); }

private:
    void SetSliceIndex(int delta);
    void SetOrientation(Orientation orient);
    void UpdateCrosshair(int x, int y, int z);
    void ApplyColorMap(vtkSmartPointer<vtkColorTransferFunction> ctf);
    void UpdatePlanePosition();
    Orientation GetOrientation() const { return m_orientation; }

    vtkSmartPointer<vtkImageSlice> m_slice;
    vtkSmartPointer<vtkImageResliceMapper> m_mapper;
    Orientation m_orientation;

    int m_currentIndex = 0;
    int m_maxIndex = 0;

    vtkSmartPointer<vtkActor> m_vLineActor;
    vtkSmartPointer<vtkActor> m_hLineActor;
    vtkSmartPointer<vtkLineSource> m_vLineSource;
    vtkSmartPointer<vtkLineSource> m_hLineSource;

    vtkSmartPointer<vtkColorTransferFunction> m_lut;
};

class MultiSliceStrategy : public AbstractVisualStrategy {
public:
    MultiSliceStrategy();

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;

private:
    void UpdateAllPositions(int x, int y, int z);

    vtkSmartPointer<vtkImageSlice> m_slices[3];
    vtkSmartPointer<vtkImageResliceMapper> m_mappers[3];
    int m_indices[3] = { 0, 0, 0 };
};

class ColoredPlanesStrategy : public AbstractVisualStrategy {
public:
    ColoredPlanesStrategy();

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;
    int GetPlaneAxis(vtkActor* actor) override;

private:
    void UpdateAllPositions(int x, int y, int z);

    vtkSmartPointer<vtkActor> m_planeActors[3];
    vtkSmartPointer<vtkPlaneSource> m_planeSources[3];
    vtkSmartPointer<vtkImageData> m_imageData;
};

class CompositeStrategy : public AbstractVisualStrategy {
public:
    CompositeStrategy(VizMode mode);

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void Attach(vtkSmartPointer<vtkRenderer> renderer) override;
    void Detach(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetupCamera(vtkSmartPointer<vtkRenderer> renderer) override;
    void UpdateVisuals(const RenderParams& params, UpdateFlags flags) override;
    int GetPlaneAxis(vtkActor* actor) override;
    vtkProp3D* GetMainProp() override;

private:
    std::shared_ptr<AbstractVisualStrategy> GetMainStrategy() { return m_mainStrategy; }

    std::shared_ptr<AbstractVisualStrategy> m_mainStrategy;
    std::shared_ptr<AbstractVisualStrategy> m_referencePlanes;
    VizMode m_mode;
};
