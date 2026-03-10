#pragma once

#include "AppTypes.h"
#include <vtkActor.h>
#include <vtkColorTransferFunction.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPolyData.h>
#include <vtkProp3D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class AbstractDataManager {
public:
    virtual ~AbstractDataManager() = default;
    virtual bool LoadData(const std::string& filePath) = 0;
    virtual vtkSmartPointer<vtkImageData> GetVtkImage() const = 0;

    LoadState GetLoadState() const
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        return m_loadState;
    }

    bool IsLoading() const
    {
        return m_isLoading.load();
    }

protected:
    mutable std::mutex m_stateMutex;
    LoadState m_loadState{ LoadState::Idle };
    std::atomic<bool> m_isLoading{ false };
};

template <typename InputT, typename OutputT>
class AbstractDataConverter {
public:
    virtual ~AbstractDataConverter() = default;
    virtual vtkSmartPointer<OutputT> Process(vtkSmartPointer<InputT> input) = 0;
    virtual void SetParameter(const std::string& key, double value) {}
};

class AbstractVisualStrategy {
public:
    virtual ~AbstractVisualStrategy() = default;

    virtual void SetInputData(vtkSmartPointer<vtkDataObject> data) = 0;
    virtual void Attach(vtkSmartPointer<vtkRenderer> renderer) = 0;
    virtual void Detach(vtkSmartPointer<vtkRenderer> renderer) = 0;
    virtual void SetupCamera(vtkSmartPointer<vtkRenderer> renderer) {}
    virtual void UpdateVisuals(const RenderParams& params, UpdateFlags flags = UpdateFlags::All) {}
    virtual int GetPlaneAxis(vtkActor* actor) { return -1; }
    virtual int GetNavigationAxis() const { return -1; }
    virtual vtkProp3D* GetMainProp() { return nullptr; }
};

class AbstractAppService {
protected:
    std::shared_ptr<AbstractDataManager> m_dataManager;
    std::shared_ptr<AbstractVisualStrategy> m_currentStrategy;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
    std::atomic<bool> m_isDirty{ false };
    std::atomic<bool> m_needsSync{ false };
    std::atomic<int> m_pendingFlags{ static_cast<int>(UpdateFlags::All) };

public:
    virtual ~AbstractAppService() = default;

    virtual void Initialize(vtkSmartPointer<vtkRenderWindow> win, vtkSmartPointer<vtkRenderer> ren)
    {
        m_renderWindow = win;
        m_renderer = ren;
    }

    virtual void ProcessPendingUpdates() {}

    bool IsDirty() const { return m_isDirty; }
    void SetDirty(bool val) { m_isDirty = val; }
    void MarkDirty() { m_isDirty = true; }

    std::shared_ptr<AbstractDataManager> GetDataManager()
    {
        return m_dataManager;
    }

    void SwitchStrategy(std::shared_ptr<AbstractVisualStrategy> newStrategy);
};

class IPreInitService {
public:
    virtual ~IPreInitService() = default;

    virtual void PreInit_SetVizMode(VizMode mode) = 0;
    virtual void PreInit_SetMaterial(const MaterialParams& mat) = 0;
    virtual void PreInit_SetOpacity(double opacity) = 0;
    virtual void PreInit_SetTransferFunction(const std::vector<TFNode>& nodes) = 0;
    virtual void PreInit_SetIsoThreshold(double val) = 0;
    virtual void PreInit_SetBackground(const BackgroundColor& bg) = 0;
    virtual void PreInit_SetWindowLevel(double ww, double wc) = 0;
    virtual void PreInit_CommitConfig(const PreInitConfig& cfg) = 0;
};

class IDataLoaderService {
public:
    virtual ~IDataLoaderService() = default;

    virtual void LoadFileAsync(
        const std::string& path,
        std::function<void(bool success)> onComplete = nullptr) = 0;
    virtual LoadState GetLoadState() const = 0;
    virtual void CancelLoad() {}
};

class AbstractRenderContext {
protected:
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkSmartPointer<vtkRenderWindow> m_renderWindow;
    std::shared_ptr<AbstractAppService> m_service;

public:
    virtual ~AbstractRenderContext() = default;

    AbstractRenderContext()
    {
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
        m_renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        m_renderWindow->AddRenderer(m_renderer);
    }

    virtual void BindService(std::shared_ptr<AbstractAppService> service)
    {
        m_service = service;
        if (m_service) {
            m_service->Initialize(m_renderWindow, m_renderer);
        }
    }

    virtual void Render()
    {
        if (m_renderWindow) {
            m_renderWindow->Render();
        }
    }

    virtual void ResetCamera()
    {
        if (m_renderer) {
            m_renderer->ResetCamera();
        }
    }

    virtual void SetInteractionMode(VizMode mode) = 0;
    virtual void Start() = 0;

public:
    virtual void SetWindowSize(int width, int height)
    {
        if (m_renderWindow) {
            m_renderWindow->SetSize(width, height);
        }
    }

    virtual void SetWindowPosition(int x, int y)
    {
        if (m_renderWindow) {
            m_renderWindow->SetPosition(x, y);
        }
    }

    virtual void SetWindowTitle(const std::string& title)
    {
        if (m_renderWindow) {
            m_renderWindow->SetWindowName(title.c_str());
        }
    }

protected:
    static void DispatchVTKEvent(
        vtkObject* caller,
        long unsigned int eventId,
        void* clientData,
        void* callData)
    {
        auto* context = static_cast<AbstractRenderContext*>(clientData);
        if (context) {
            context->HandleVTKEvent(caller, eventId, callData);
        }
    }

    virtual void HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData) {}
};

class AbstractInteractiveService : public AbstractAppService {
public:
    virtual ~AbstractInteractiveService() = default;

    virtual void UpdateInteraction(int value) {}
    virtual int GetPlaneAxis(vtkActor* actor) { return -1; }
    virtual void SyncCursorToWorldPosition(double worldPos[3])
    {
        (void)worldPos;
    }
    virtual void SyncCursorToWorldPosition(double worldPos[3], int axis)
    {
        (void)axis;
        SyncCursorToWorldPosition(worldPos);
    }
    virtual std::array<int, 3> GetCursorPosition() { return { 0, 0, 0 }; }
    virtual void SetInteracting(bool val) { (void)val; }
    virtual vtkProp3D* GetMainProp() { return nullptr; }
    virtual void SyncModelMatrix(vtkMatrix4x4* mat) { (void)mat; }
    virtual void SetElementVisible(std::uint32_t flagBit, bool show)
    {
        (void)flagBit;
        (void)show;
    }
    virtual void AdjustWindowLevel(double deltaWW, double deltaWC)
    {
        (void)deltaWW;
        (void)deltaWC;
    }
};
