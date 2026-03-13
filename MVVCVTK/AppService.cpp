#include "AppService.h"

#include "DataManager.h"
#include "StrategyFactory.h"
#include <algorithm>
#include <iostream>
#include <thread>

void AbstractAppService::SwitchStrategy(std::shared_ptr<AbstractVisualStrategy> newStrategy)
{
    if (!m_renderer || !m_renderWindow) {
        return;
    }

    if (m_currentStrategy) {
        m_currentStrategy->Detach(m_renderer);
    }

    m_currentStrategy = std::move(newStrategy);

    if (m_currentStrategy) {
        m_currentStrategy->Attach(m_renderer);
        m_currentStrategy->SetupCamera(m_renderer);
    }

    m_renderer->ResetCamera();
    m_isDirty = true;
}

MedicalVizService::MedicalVizService(
    std::shared_ptr<AbstractDataManager> dataMgr,
    std::shared_ptr<SharedInteractionState> state)
    : m_sharedState(std::move(state))
    , m_transformService(std::make_unique<VolumeTransformService>(m_sharedState))
    , m_cancelFlag(std::make_shared<std::atomic<bool>>(false))
{
    m_dataManager = std::move(dataMgr);
}

MedicalVizService::~MedicalVizService()
{
    if (m_cancelFlag) {
        m_cancelFlag->store(true);
    }

    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loadFuture.valid()) {
        m_loadFuture.wait();
    }
}

void MedicalVizService::Initialize(vtkSmartPointer<vtkRenderWindow> win, vtkSmartPointer<vtkRenderer> ren)
{
    AbstractAppService::Initialize(win, ren);
    if (!m_sharedState) {
        return;
    }

    std::weak_ptr<MedicalVizService> weakSelf =
        std::static_pointer_cast<MedicalVizService>(shared_from_this());

    m_sharedState->AddObserver(shared_from_this(), [weakSelf](UpdateFlags flags) {
        auto self = weakSelf.lock();
        if (!self) {
            return;
        }

        if (HasFlag(flags, UpdateFlags::DataReady)) {
            self->RequestClearStrategyCache();
            self->ResetCursorToCenter();
            self->m_pendingFlags.fetch_or(static_cast<int>(UpdateFlags::All));
            self->m_needsDataRefresh = true;
            return;
        }

        if (HasFlag(flags, UpdateFlags::LoadFailed)) {
            self->m_needsLoadFailed = true;
            self->m_isDirty = true;
            return;
        }

        if (HasFlag(flags, UpdateFlags::Background)) {
            self->m_pendingFlags.fetch_or(static_cast<int>(UpdateFlags::Background));
            self->MarkNeedsSync();
            return;
        }

        int oldValue = self->m_pendingFlags.load();
        while (!self->m_pendingFlags.compare_exchange_weak(
            oldValue,
            oldValue | static_cast<int>(flags))) {
        }
        self->MarkNeedsSync();
        self->RequestImmediateFrame(flags);
    });
}

void MedicalVizService::PreInit_SetVizMode(VizMode mode)
{
    m_pendingVizModeInt.store(static_cast<int>(mode));
}

void MedicalVizService::PreInit_SetMaterial(const MaterialParams& mat)
{
    if (m_sharedState) {
        m_sharedState->SetMaterial(mat);
    }
}

void MedicalVizService::PreInit_SetOpacity(double opacity)
{
    if (!m_sharedState) {
        return;
    }

    auto mat = m_sharedState->GetMaterial();
    mat.opacity = opacity;
    m_sharedState->SetMaterial(mat);
}

void MedicalVizService::PreInit_SetTransferFunction(const std::vector<TFNode>& nodes)
{
    if (m_sharedState) {
        m_sharedState->SetTFNodes(nodes);
    }
}

void MedicalVizService::PreInit_SetIsoThreshold(double val)
{
    if (m_sharedState) {
        m_sharedState->SetIsoValue(val);
    }
}

void MedicalVizService::PreInit_SetIsoRenderQuality(IsoRenderQuality quality)
{
    if (m_sharedState) {
        m_sharedState->SetIsoRenderQuality(quality);
    }
}

void MedicalVizService::PreInit_SetBackground(const BackgroundColor& bg)
{
    if (!m_sharedState) {
        return;
    }

    m_sharedState->SetBackground(bg);
    if (m_renderer) {
        m_renderer->SetBackground(bg.r, bg.g, bg.b);
    }
}

void MedicalVizService::PreInit_SetWindowLevel(double ww, double wc)
{
    if (m_sharedState) {
        m_sharedState->SetWindowLevel(ww, wc);
    }
}

void MedicalVizService::PreInit_CommitConfig(const PreInitConfig& cfg)
{
    m_pendingVizModeInt.store(static_cast<int>(cfg.vizMode));
    if (m_sharedState) {
        m_sharedState->CommitPreInitConfig(cfg);
    }
    if (cfg.hasBgColor && m_renderer) {
        m_renderer->SetBackground(cfg.bgColor.r, cfg.bgColor.g, cfg.bgColor.b);
    }
}

void MedicalVizService::LoadFileAsync(
    const std::string& path,
    std::function<void(bool success)> onComplete)//閿熸枻鎷烽敓鏂ゆ嫹涓�閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷?閿熼ズ纭锋嫹閿熸埅鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹閿熸枻鎷烽敓鏂ゆ嫹
{
    if (!m_sharedState || !m_dataManager) {
        if (onComplete) {
            onComplete(false);
        }
        return;
    }

    if (m_sharedState->GetLoadState() == LoadState::Loading) {
        std::cerr << "[LoadFileAsync] Already loading, ignoring duplicate call.\n";
        return;
    }

    m_cancelFlag->store(false);
    m_sharedState->SetLoadState(LoadState::Loading);

    auto dataMgr = m_dataManager;
    auto sharedState = m_sharedState;
    auto cancelFlag = m_cancelFlag;

    std::packaged_task<void()> task([dataMgr, sharedState, path, onComplete, cancelFlag]() mutable {
        if (cancelFlag->load()) {
            sharedState->SetLoadState(LoadState::Idle);
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        const bool ok = dataMgr->LoadData(path);

        if (cancelFlag->load()) {
            sharedState->SetLoadState(LoadState::Idle);
            if (onComplete) {
                onComplete(false);
            }
            return;
        }

        if (ok) {
            auto img = dataMgr->GetVtkImage();
            if (img) {
                double range[2] = { 0.0, 0.0 };
                img->GetScalarRange(range);
                sharedState->NotifyDataReady(range[0], range[1]);
            } else {
                sharedState->NotifyLoadFailed();
            }
        } else {
            sharedState->NotifyLoadFailed();
        }

        if (onComplete) {
            onComplete(ok);
        }
    });

    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        m_loadFuture = task.get_future();
    }

    std::thread(std::move(task)).detach();
}

bool MedicalVizService::SetFromBufferAsync(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    std::function<void(bool success)> onComplete)
{
    if (m_sharedState->GetLoadState() == LoadState::Loading) {
        std::cerr << "[SetFromBufferAsync] Already loading, ignoring.\n";
        return false;
    }

    m_sharedState->SetLoadState(LoadState::Loading);

    auto dataMgr = m_dataManager;
    auto sharedState = m_sharedState;

    std::packaged_task<void()> task(
        [dataMgr, sharedState, data, dims, spacing, origin, onComplete]() mutable
        {
            // 在后台线程
            bool ok = dataMgr->SetFromBuffer(data, dims, spacing, origin);

            // ── 结果通知：只写 SharedState，不碰任何 VTK 渲染对象 ────
            // DataReady/LoadFailed → Observer → m_needsDataRefresh
            // 主线程 ProcessPendingUpdates → PostData_RebuildPipeline
            if (ok) {
                auto img = dataMgr->GetVtkImage();
                if (img) {
                    double range[2];
                    img->GetScalarRange(range);
                    sharedState->NotifyDataReady(range[0], range[1]);
                }
                else {
                    sharedState->NotifyLoadFailed();
                }
            }
            else {
                sharedState->NotifyLoadFailed();
            }
            if (onComplete) onComplete(ok);
        });

    {
        std::lock_guard<std::mutex> lk(m_loadMutex);
        m_loadFuture = task.get_future();
    }

    std::thread(std::move(task)).detach();
	return true;
}

LoadState MedicalVizService::GetLoadState() const
{
    return m_sharedState ? m_sharedState->GetLoadState() : LoadState::Idle;
}

void MedicalVizService::CancelLoad()
{
    if (m_cancelFlag) {
        m_cancelFlag->store(true);
    }
}

void MedicalVizService::ProcessPendingUpdates()
{
    if (auto* rawMgr = dynamic_cast<RawVolumeDataManager*>(m_dataManager.get())) {
        if (rawMgr->ConsumeReconImage()) {
            // 走和文件加载成功相同的后处理路径
            auto img = m_dataManager->GetVtkImage();
            if (img) {
                double range[2];
                img->GetScalarRange(range);
                m_sharedState->NotifyDataReady(range[0], range[1]);
            }
        }
    }
    if (m_needsCacheClear.exchange(false)) {
        ExecuteClearStrategyCache();
    }

    if (m_needsLoadFailed.exchange(false)) {
        PostData_HandleLoadFailed();
        return;
    }

    if (m_needsDataRefresh.exchange(false)) {
        PostData_RebuildPipeline();
        return;
    }

    PostData_SyncStateToStrategy();
}

void MedicalVizService::PostData_RebuildPipeline()
{
    const VizMode mode = static_cast<VizMode>(m_pendingVizModeInt.load());
    auto strategy = GetOrCreateStrategy(mode);
    if (!strategy) {
        std::cerr << "[RebuildPipeline] StrategyFactory returned null.\n";
        return;
    }

    auto img = m_dataManager ? m_dataManager->GetVtkImage() : nullptr;
    if (!img) {
        std::cerr << "[RebuildPipeline] DataManager has no valid image.\n";
        return;
    }

    int dims[3] = { 0, 0, 0 };
    img->GetDimensions(dims);
    if (dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0) {
        std::cerr << "[RebuildPipeline] Image has zero dimension.\n";
        return;
    }

    strategy->SetInputData(img);
    SwitchStrategy(strategy);
    MarkNeedsSync();
}

void MedicalVizService::PostData_SyncStateToStrategy()
{
    if (!m_needsSync.load() || !m_currentStrategy || !m_sharedState) {
        return;
    }

    const int flagsInt = m_pendingFlags.exchange(0);
    const UpdateFlags flags = static_cast<UpdateFlags>(flagsInt);
    if (flags == UpdateFlags::None) {
        m_needsSync = false;
        return;
    }

    if (HasFlag(flags, UpdateFlags::Interaction) && m_renderWindow) {
        const bool interacting = m_sharedState->IsInteracting();
        m_renderWindow->SetDesiredUpdateRate(interacting ? 15.0 : 0.001);
    }

    if (HasFlag(flags, UpdateFlags::Background) && m_renderer) {
        const auto bg = m_sharedState->GetBackground();
        m_renderer->SetBackground(bg.r, bg.g, bg.b);
    }

    RenderParams params = BuildRenderParams(flags);
    m_currentStrategy->UpdateVisuals(params, flags);

    m_isDirty = true;
    m_needsSync = false;
}

void MedicalVizService::PostData_HandleLoadFailed()
{
    ExecuteClearStrategyCache();
    m_needsDataRefresh = false;
    m_pendingFlags = 0;
    m_isDirty = true;
}

RenderParams MedicalVizService::BuildRenderParams(UpdateFlags flags) const
{
    RenderParams params;
    if (!m_sharedState) {
        return params;
    }

    const auto range = m_sharedState->GetDataRange();
    params.scalarRange[0] = range[0];
    params.scalarRange[1] = range[1];

    if (HasFlag(flags, UpdateFlags::Cursor)) {
        const auto pos = m_sharedState->GetCursorPosition();
        params.cursor = { pos[0], pos[1], pos[2] };
    }
    if (HasFlag(flags, UpdateFlags::TF)) {
        m_sharedState->GetTFNodes(params.tfNodes);
    }
    if (HasFlag(flags, UpdateFlags::WindowLevel)) {
        params.windowLevel = m_sharedState->GetWindowLevel();
    }
    if (HasFlag(flags, UpdateFlags::Material)) {
        params.material = m_sharedState->GetMaterial();
    }
    if (HasFlag(flags, UpdateFlags::IsoValue) || HasFlag(flags, UpdateFlags::IsoQuality)) {
        params.isoValue = m_sharedState->GetIsoValue();
    }
    if (HasFlag(flags, UpdateFlags::IsoQuality) || HasFlag(flags, UpdateFlags::IsoValue)) {
        params.isoRenderQuality = m_sharedState->GetIsoRenderQuality();
    }
    if (HasFlag(flags, UpdateFlags::RenderMode)) {
        params.primary3DMode = m_sharedState->GetPrimary3DMode();
    }
    if (HasFlag(flags, UpdateFlags::Transform)) {
        params.modelMatrix = m_sharedState->GetModelMatrix();
    }
    if (HasFlag(flags, UpdateFlags::Background)) {
        params.background = m_sharedState->GetBackground();
    }
    if (HasFlag(flags, UpdateFlags::Visibility)) {
        params.visibilityMask = m_sharedState->GetVisibilityMask();
    }

    return params;
}

void MedicalVizService::RequestImmediateFrame(UpdateFlags flags)
{
    constexpr int kImmediateFlags =
        static_cast<int>(UpdateFlags::Cursor)
        | static_cast<int>(UpdateFlags::Interaction)
        | static_cast<int>(UpdateFlags::WindowLevel)
        | static_cast<int>(UpdateFlags::IsoValue)
        | static_cast<int>(UpdateFlags::IsoQuality);

    if ((static_cast<int>(flags) & kImmediateFlags) == 0) {
        return;
    }

    ProcessPendingUpdates();
    if (!m_isDirty.load()) {
        return;
    }

    if (PresentFrame()) {
        m_isDirty = false;
    }
}

std::shared_ptr<AbstractVisualStrategy> MedicalVizService::GetOrCreateStrategy(VizMode mode)
{
    const auto it = m_strategyCache.find(mode);
    if (it != m_strategyCache.end()) {
        return it->second;
    }

    auto strategy = StrategyFactory::CreateStrategy(mode);
    if (strategy) {
        m_strategyCache[mode] = strategy;
    }
    return strategy;
}

void MedicalVizService::RequestClearStrategyCache()
{
    m_needsCacheClear = true;
}

void MedicalVizService::ExecuteClearStrategyCache()
{
    if (m_currentStrategy && m_renderer) {
        m_currentStrategy->Detach(m_renderer);
        m_currentStrategy = nullptr;
    }
    m_strategyCache.clear();
}

void MedicalVizService::ResetCursorToCenter()
{
    if (!m_dataManager || !m_sharedState) {
        return;
    }

    auto img = m_dataManager->GetVtkImage();
    if (!img) {
        return;
    }

    int dims[3] = { 0, 0, 0 };
    img->GetDimensions(dims);
    m_sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
}

void MedicalVizService::MarkNeedsSync()
{
    m_needsSync = true;
    m_isDirty = true;
}

void MedicalVizService::UpdateInteraction(int delta)
{
    VizMode mode = static_cast<VizMode>(m_pendingVizModeInt.load());
    int axis = 2;
    if (mode == VizMode::SliceCoronal) {
        axis = 1;
    } else if (mode == VizMode::SliceSagittal) {
        axis = 0;
    } else if (m_currentStrategy && m_currentStrategy->GetNavigationAxis() != -1) {
        axis = m_currentStrategy->GetNavigationAxis();
    }

    UpdateInteractionAxis(axis, delta);
}

void MedicalVizService::UpdateInteractionAxis(int axis, int delta)
{
    if (!m_sharedState || axis < 0 || axis > 2 || delta == 0) {
        return;
    }

    auto pos = m_sharedState->GetCursorPosition();
    pos[axis] += delta;

    if (m_dataManager) {
        auto img = m_dataManager->GetVtkImage();
        if (img) {
            int dims[3] = { 0, 0, 0 };
            img->GetDimensions(dims);
            pos[axis] = std::max(0, std::min(pos[axis], dims[axis] - 1));
        }
    }

    m_sharedState->SetCursorPosition(pos[0], pos[1], pos[2]);
    MarkNeedsSync();
}

void MedicalVizService::SyncCursorToWorldPosition(double worldPos[3], int axis)
{
    if (!m_dataManager || !m_sharedState) {
        return;
    }

    auto img = m_dataManager->GetVtkImage();
    if (!img) {
        return;
    }

    double spacing[3] = { 1.0, 1.0, 1.0 };
    double origin[3] = { 0.0, 0.0, 0.0 };
    int dims[3] = { 0, 0, 0 };
    img->GetSpacing(spacing);
    img->GetOrigin(origin);
    img->GetDimensions(dims);

    auto clamp = [](int value, int lo, int hi) {
        return std::max(lo, std::min(value, hi));
    };

    auto pos = m_sharedState->GetCursorPosition();
  
    if (axis != 0) {
        pos[0] = clamp(static_cast<int>(std::round((worldPos[0] - origin[0]) / spacing[0])), 0, dims[0] - 1);
    }
    if (axis != 1) {
        pos[1] = clamp(static_cast<int>(std::round((worldPos[1] - origin[1]) / spacing[1])), 0, dims[1] - 1);
    }
    if (axis != 2) {
        pos[2] = clamp(static_cast<int>(std::round((worldPos[2] - origin[2]) / spacing[2])), 0, dims[2] - 1);
    }


    m_sharedState->SetCursorPosition(pos[0], pos[1], pos[2]);
}

std::array<int, 3> MedicalVizService::GetCursorPosition()
{
    return m_sharedState ? m_sharedState->GetCursorPosition() : std::array<int, 3>{ 0, 0, 0 };
}

std::array<double, 3> MedicalVizService::GetCursorWorldPosition()
{
    if (!m_dataManager || !m_sharedState) {
        return { 0.0, 0.0, 0.0 };
    }

    auto img = m_dataManager->GetVtkImage();
    if (!img) {
        return { 0.0, 0.0, 0.0 };
    }

    double spacing[3] = { 1.0, 1.0, 1.0 };
    double origin[3] = { 0.0, 0.0, 0.0 };
    img->GetSpacing(spacing);
    img->GetOrigin(origin);

    const auto pos = m_sharedState->GetCursorPosition();
    return {
        origin[0] + pos[0] * spacing[0],
        origin[1] + pos[1] * spacing[1],
        origin[2] + pos[2] * spacing[2]
    };
}

void MedicalVizService::SetInteracting(bool val)
{
    if (m_sharedState) {
        m_sharedState->SetInteracting(val);
    }
}

int MedicalVizService::GetPlaneAxis(vtkActor* actor)
{
    return m_currentStrategy ? m_currentStrategy->GetPlaneAxis(actor) : -1;
}

vtkProp3D* MedicalVizService::GetMainProp()
{
    return m_currentStrategy ? m_currentStrategy->GetMainProp() : nullptr;
}

void MedicalVizService::SyncModelMatrix(vtkMatrix4x4* mat)
{
    if (m_transformService) {
        m_transformService->SyncModelMatrix(mat);
    }
}

void MedicalVizService::SetElementVisible(std::uint32_t flagBit, bool show)
{
    if (m_sharedState) {
        m_sharedState->SetElementVisible(flagBit, show);
    }
}

void MedicalVizService::AdjustWindowLevel(double deltaWW, double deltaWC)
{
    if (!m_sharedState) {
        return;
    }

    const auto current = m_sharedState->GetWindowLevel();
    const auto range = m_sharedState->GetDataRange();
    const double dataSpan = range[1] - range[0];

    const double scaledWW = (dataSpan > 0.0) ? deltaWW * dataSpan * 0.005 : deltaWW;
    const double scaledWC = (dataSpan > 0.0) ? deltaWC * dataSpan * 0.003 : deltaWC;

    constexpr double kMinWW = 1.0;
    const double newWW = std::max(kMinWW, current.windowWidth + scaledWW);
    const double newWC = (dataSpan > 0.0)
        ? std::max(range[0], std::min(range[1], current.windowCenter + scaledWC))
        : current.windowCenter + scaledWC;

    m_sharedState->SetWindowLevel(newWW, newWC);
    MarkNeedsSync();
}

void MedicalVizService::TransformModel(double translate[3], double rotate[3], double scale[3])
{
    if (m_transformService) {
        m_transformService->TransformModel(translate, rotate, scale);
    }
}

void MedicalVizService::ResetModelTransform()
{
    if (m_transformService) {
        m_transformService->ResetModelTransform();
    }
}

void MedicalVizService::WorldToModel(const double worldPos[3], double modelPos[3])
{
    if (m_transformService) {
        m_transformService->WorldToModel(worldPos, modelPos);
    }
}

void MedicalVizService::ModelToWorld(const double modelPos[3], double worldPos[3])
{
    if (m_transformService) {
        m_transformService->ModelToWorld(modelPos, worldPos);
    }
}

void MedicalVizService::LoadFile(const std::string& path)
{
    if (!m_dataManager || !m_sharedState) {
        return;
    }

    if (!m_dataManager->LoadData(path)) {
        m_sharedState->NotifyLoadFailed();
        m_needsLoadFailed = true;
        return;
    }

    auto img = m_dataManager->GetVtkImage();
    if (!img) {
        m_sharedState->NotifyLoadFailed();
        m_needsLoadFailed = true;
        return;
    }

    double range[2] = { 0.0, 0.0 };
    img->GetScalarRange(range);
    m_sharedState->NotifyDataReady(range[0], range[1]);
    ShowIsoSurface();
}

void MedicalVizService::ActivateModeImmediate(VizMode mode)
{
    m_pendingVizModeInt.store(static_cast<int>(mode));

    if (!m_dataManager) {
        return;
    }

    auto img = m_dataManager->GetVtkImage();
    if (!img) {
        return;
    }

    auto strategy = GetOrCreateStrategy(mode);
    if (!strategy) {
        return;
    }

    strategy->SetInputData(img);
    SwitchStrategy(strategy);
    constexpr int kStrategyBootstrapFlags =
        static_cast<int>(UpdateFlags::Cursor)
        | static_cast<int>(UpdateFlags::TF)
        | static_cast<int>(UpdateFlags::IsoValue)
        | static_cast<int>(UpdateFlags::Material)
        | static_cast<int>(UpdateFlags::Transform)
        | static_cast<int>(UpdateFlags::Background)
        | static_cast<int>(UpdateFlags::Visibility)
        | static_cast<int>(UpdateFlags::WindowLevel)
        | static_cast<int>(UpdateFlags::IsoQuality)
        | static_cast<int>(UpdateFlags::RenderMode);
    m_pendingFlags.fetch_or(kStrategyBootstrapFlags);
    MarkNeedsSync();
}

void MedicalVizService::ShowVolume()
{
    ActivateModeImmediate(VizMode::Volume);
}

void MedicalVizService::ShowIsoSurface()
{
    ActivateModeImmediate(VizMode::IsoSurface);
}

void MedicalVizService::ShowSlice(VizMode sliceMode)
{
    ActivateModeImmediate(sliceMode);
}

void MedicalVizService::Show3DPlanes(VizMode renderMode)
{
    ActivateModeImmediate(renderMode);
}

void MedicalVizService::OnStateChanged()
{
    MarkNeedsSync();
}

void MedicalVizService::SetLuxParams(double ambient, double diffuse, double specular, double power, bool shadeOn)
{
    if (!m_sharedState) {
        return;
    }

    auto mat = m_sharedState->GetMaterial();
    mat.ambient = ambient;
    mat.diffuse = diffuse;
    mat.specular = specular;
    mat.specularPower = power;
    mat.shadeOn = shadeOn;
    m_sharedState->SetMaterial(mat);
}

void MedicalVizService::SetOpacity(double opacity)
{
    PreInit_SetOpacity(opacity);
}

void MedicalVizService::SetIsoThreshold(double val)
{
    PreInit_SetIsoThreshold(val);
}

void MedicalVizService::SetIsoRenderQuality(IsoRenderQuality quality)
{
    PreInit_SetIsoRenderQuality(quality);
}

void MedicalVizService::SetTransferFunction(const std::vector<TFNode>& nodes)
{
    PreInit_SetTransferFunction(nodes);
}







