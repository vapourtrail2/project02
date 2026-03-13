#pragma once

#include "AppInterfaces.h"
#include "AppState.h"
#include "VolumeTransformService.h"
#include <atomic>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>

class MedicalVizService
    : public AbstractInteractiveService
    , public IPreInitService
    , public IDataLoaderService
    , public std::enable_shared_from_this<MedicalVizService>
{
public:
    MedicalVizService(
        std::shared_ptr<AbstractDataManager> dataMgr,
        std::shared_ptr<SharedInteractionState> state);
    ~MedicalVizService();

    void Initialize(vtkSmartPointer<vtkRenderWindow> win, vtkSmartPointer<vtkRenderer> ren) override;

    void PreInit_SetVizMode(VizMode mode) override;
    void PreInit_SetMaterial(const MaterialParams& mat) override;
    void PreInit_SetOpacity(double opacity) override;
    void PreInit_SetTransferFunction(const std::vector<TFNode>& nodes) override;
    void PreInit_SetIsoThreshold(double val) override;
    void PreInit_SetIsoRenderQuality(IsoRenderQuality quality) override;
    void PreInit_SetBackground(const BackgroundColor& bg) override;
    void PreInit_SetWindowLevel(double ww, double wc) override;
    void PreInit_CommitConfig(const PreInitConfig& cfg) override;

    void LoadFileAsync(
        const std::string& path,
        std::function<void(bool success)> onComplete = nullptr) override;
    // 重建注入异步接口：将 SetFromBuffer 的耗时操作投递到后台线程
    bool SetFromBufferAsync(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        std::function<void(bool success)> onComplete = nullptr);
    LoadState GetLoadState() const override;
    void CancelLoad() override;

    void ProcessPendingUpdates() override;

    void UpdateInteraction(int delta) override;
    void UpdateInteractionAxis(int axis, int delta) override;
    void SyncCursorToWorldPosition(double worldPos[3], int axis = -1) override;
    std::array<int, 3> GetCursorPosition() override;
    std::array<double, 3> GetCursorWorldPosition() override;
    void SetInteracting(bool val) override;
    int GetPlaneAxis(vtkActor* actor) override;
    vtkProp3D* GetMainProp() override;
    void SyncModelMatrix(vtkMatrix4x4* mat) override;
    void SetElementVisible(std::uint32_t flagBit, bool show) override;
    void AdjustWindowLevel(double deltaWW, double deltaWC) override;
    void SetIsoRenderQuality(IsoRenderQuality quality) override;

    void TransformModel(double translate[3], double rotate[3], double scale[3]);
    void ResetModelTransform();
    void WorldToModel(const double worldPos[3], double modelPos[3]);
    void ModelToWorld(const double modelPos[3], double worldPos[3]);

    std::shared_ptr<SharedInteractionState> GetSharedState() const { return m_sharedState; }

    // Compatibility surface used by the current Qt frontend.
    void LoadFile(const std::string& path);
    void ShowVolume();
    void ShowIsoSurface();
    void ShowSlice(VizMode sliceMode);
    void Show3DPlanes(VizMode renderMode);
    void OnStateChanged();
    void SetLuxParams(double ambient, double diffuse, double specular, double power, bool shadeOn = false);
    void SetOpacity(double opacity);
    void SetIsoThreshold(double val);
    void SetTransferFunction(const std::vector<TFNode>& nodes);

private:
    void PostData_RebuildPipeline();
    void PostData_SyncStateToStrategy();
    void PostData_HandleLoadFailed();
    RenderParams BuildRenderParams(UpdateFlags flags) const;
    void RequestImmediateFrame(UpdateFlags flags);
    std::shared_ptr<AbstractVisualStrategy> GetOrCreateStrategy(VizMode mode);
    void RequestClearStrategyCache();
    void ExecuteClearStrategyCache();
    void ResetCursorToCenter();
    void MarkNeedsSync();
    void ActivateModeImmediate(VizMode mode);

private:
    std::map<VizMode, std::shared_ptr<AbstractVisualStrategy>> m_strategyCache;
    std::shared_ptr<SharedInteractionState> m_sharedState;
    std::unique_ptr<VolumeTransformService> m_transformService;

    std::atomic<int> m_pendingVizModeInt{ static_cast<int>(VizMode::IsoSurface) };
    std::atomic<bool> m_needsDataRefresh{ false };
    std::atomic<bool> m_needsCacheClear{ false };
    std::atomic<bool> m_needsLoadFailed{ false };
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;

    std::future<void> m_loadFuture;
    mutable std::mutex m_loadMutex;
};




