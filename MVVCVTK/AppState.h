#pragma once

#include "AppTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

using ObserverCallback = std::function<void(UpdateFlags)>;

struct ObserverEntry {
    std::weak_ptr<void> owner;
    ObserverCallback callback;
};

class SharedInteractionState {
public:
    SharedInteractionState()
    {
        m_nodes = {
            { 0.00, 0.0, 0.00, 0.00, 0.00 },
            { 0.35, 0.0, 0.75, 0.75, 0.75 },
            { 0.60, 0.6, 0.85, 0.85, 0.85 },
            { 1.00, 1.0, 0.95, 0.95, 0.95 },
        };
    }

    void NotifyDataReady(double rangeMin, double rangeMax)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_dataRange[0] = rangeMin;
            m_dataRange[1] = rangeMax;
            m_windowLevel.windowWidth = std::max(rangeMax - rangeMin, 1e-6);
            m_windowLevel.windowCenter = (rangeMin + rangeMax) * 0.5;
            m_loadState = LoadState::Succeeded;
        }
        NotifyObservers(UpdateFlags::DataReady);
    }

    void NotifyLoadFailed()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_loadState = LoadState::Failed;
        }
        NotifyObservers(UpdateFlags::LoadFailed);
    }

    void SetLoadState(LoadState state)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_loadState = state;
    }

    LoadState GetLoadState() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loadState;
    }

    void CommitPreInitConfig(const PreInitConfig& cfg)
    {
        UpdateFlags flags = UpdateFlags::None;
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_material.ambient != cfg.material.ambient ||
                m_material.diffuse != cfg.material.diffuse ||
                m_material.specular != cfg.material.specular ||
                m_material.specularPower != cfg.material.specularPower ||
                m_material.opacity != cfg.material.opacity ||
                m_material.shadeOn != cfg.material.shadeOn) {
                m_material = cfg.material;
                flags |= UpdateFlags::Material;
            }

            if (cfg.hasTF) {
                m_nodes = cfg.tfNodes;
                flags |= UpdateFlags::TF;
            }

            if (cfg.hasIso && std::abs(m_isoValue - cfg.isoThreshold) > 1e-6) {
                m_isoValue = cfg.isoThreshold;
                flags |= UpdateFlags::IsoValue;
            }

            if (cfg.hasIsoQuality && m_isoRenderQuality != cfg.isoRenderQuality) {
                m_isoRenderQuality = cfg.isoRenderQuality;
                flags |= UpdateFlags::IsoQuality;
            }

            if (cfg.hasPrimary3DMode && m_primary3DMode != cfg.primary3DMode) {
                m_primary3DMode = cfg.primary3DMode;
                flags |= UpdateFlags::RenderMode;
            }

            if (cfg.hasBgColor &&
                (std::abs(m_background.r - cfg.bgColor.r) > 1e-6 ||
                 std::abs(m_background.g - cfg.bgColor.g) > 1e-6 ||
                 std::abs(m_background.b - cfg.bgColor.b) > 1e-6)) {
                m_background = cfg.bgColor;
                flags |= UpdateFlags::Background;
            }

            if (cfg.hasWindowLevel &&
                (std::abs(m_windowLevel.windowWidth - cfg.windowLevel.windowWidth) > 1e-6 ||
                 std::abs(m_windowLevel.windowCenter - cfg.windowLevel.windowCenter) > 1e-6)) {
                m_windowLevel = cfg.windowLevel;
                flags |= UpdateFlags::WindowLevel;
            }
        }

        if (flags != UpdateFlags::None) {
            NotifyObservers(flags);
        }
    }

    void SetModelMatrix(const std::array<double, 16>& mat)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_modelMatrix = mat;
        }
        NotifyObservers(UpdateFlags::Transform);
    }

    std::array<double, 16> GetModelMatrix() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_modelMatrix;
    }

    void SetScalarRange(double minValue, double maxValue)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_dataRange[0] = minValue;
            m_dataRange[1] = maxValue;
        }
        NotifyObservers(UpdateFlags::TF);
    }

    std::array<double, 2> GetDataRange() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_dataRange[0], m_dataRange[1] };
    }

    void SetTFNodes(const std::vector<TFNode>& nodes)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_nodes = nodes;
        }
        NotifyObservers(UpdateFlags::TF);
    }

    void GetTFNodes(std::vector<TFNode>& dest) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        dest = m_nodes;
    }

    std::vector<TFNode> GetTFNodes() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_nodes;
    }

    void SetIsoValue(double val)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (std::abs(m_isoValue - val) > 1e-6) {
                m_isoValue = val;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::IsoValue);
        }
    }

    double GetIsoValue() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isoValue;
    }

    void SetIsoRenderQuality(IsoRenderQuality quality)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_isoRenderQuality != quality) {
                m_isoRenderQuality = quality;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::IsoQuality);
        }
    }

    IsoRenderQuality GetIsoRenderQuality() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isoRenderQuality;
    }

    void SetPrimary3DMode(VizMode mode)
    {
        if (mode != VizMode::CompositeVolume && mode != VizMode::CompositeIsoSurface) {
            return;
        }

        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_primary3DMode != mode) {
                m_primary3DMode = mode;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::RenderMode);
        }
    }

    VizMode GetPrimary3DMode() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_primary3DMode;
    }

    void SetMaterial(const MaterialParams& mat)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_material = mat;
        }
        NotifyObservers(UpdateFlags::Material);
    }

    MaterialParams GetMaterial() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_material;
    }

    void SetBackground(const BackgroundColor& bg)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (std::abs(m_background.r - bg.r) > 1e-6 ||
                std::abs(m_background.g - bg.g) > 1e-6 ||
                std::abs(m_background.b - bg.b) > 1e-6) {
                m_background = bg;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::Background);
        }
    }

    BackgroundColor GetBackground() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_background;
    }

    void SetWindowLevel(double ww, double wc)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (std::abs(m_windowLevel.windowWidth - ww) > 1e-6 ||
                std::abs(m_windowLevel.windowCenter - wc) > 1e-6) {
                m_windowLevel.windowWidth = ww;
                m_windowLevel.windowCenter = wc;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::WindowLevel);
        }
    }

    WindowLevelParams GetWindowLevel() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_windowLevel;
    }

    void SetInteracting(bool val)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_isInteracting != val) {
                m_isInteracting = val;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::Interaction);
        }
    }

    bool IsInteracting() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isInteracting;
    }

    void SetCursorPosition(int x, int y, int z)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_cursorPos[0] != x || m_cursorPos[1] != y || m_cursorPos[2] != z) {
                m_cursorPos[0] = x;
                m_cursorPos[1] = y;
                m_cursorPos[2] = z;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::Cursor);
        }
    }

    void UpdateAxis(int axisIndex, int delta, int maxDim)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            int nextValue = m_cursorPos[axisIndex] + delta;
            nextValue = nextValue < 0 ? 0 : (nextValue >= maxDim ? maxDim - 1 : nextValue);
            if (m_cursorPos[axisIndex] != nextValue) {
                m_cursorPos[axisIndex] = nextValue;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::Cursor);
        }
    }

    std::array<int, 3> GetCursorPosition() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_cursorPos[0], m_cursorPos[1], m_cursorPos[2] };
    }

    void SetElementVisible(std::uint32_t flagBit, bool show)
    {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const std::uint32_t oldMask = m_visibilityMask;
            if (show) {
                m_visibilityMask |= flagBit;
            }
            else {
                m_visibilityMask &= ~flagBit;
            }
            changed = (m_visibilityMask != oldMask);
        }
        if (changed) {
            NotifyObservers(UpdateFlags::Visibility);
        }
    }

    std::uint32_t GetVisibilityMask() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_visibilityMask;
    }

    void AddObserver(std::shared_ptr<void> owner, ObserverCallback cb)
    {
        if (!owner || !cb) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        m_observers.erase(
            std::remove_if(
                m_observers.begin(),
                m_observers.end(),
                [](const ObserverEntry& entry) { return entry.owner.expired(); }),
            m_observers.end());
        m_observers.push_back({ std::move(owner), std::move(cb) });
    }

private:
    void NotifyObservers(UpdateFlags flags)
    {
		std::vector<ObserverCallback> callbacks;//把有效的回调函数全部拷贝到这里  这个是本地数组 用来存这次要通知的回调函数
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_observers.begin(); it != m_observers.end();) {
				if (it->owner.expired()) {//it指向当前元素 ，当观察者的owner已经过期
                    it = m_observers.erase(it);
					//erase会返回下一个元素的迭代器，所以不需要++it
					//ABCD 删掉B 后，erase返回C的迭代器，下一轮循环it指向C
                }
                else {
                    callbacks.push_back(it->callback);
                    ++it;
                }
            }
        }

        for (const auto& callback : callbacks) {
            if (callback) {
				callback(flags);//根据类型调用回调函数
            }
        }
    }

    //当这些状态变化 通知所有观察者
    mutable std::mutex m_mutex;
    int m_cursorPos[3] = { 0, 0, 0 };
    double m_isoValue = 0.0;
    IsoRenderQuality m_isoRenderQuality = IsoRenderQuality::Fast;
    VizMode m_primary3DMode = VizMode::CompositeIsoSurface;
    MaterialParams m_material;
    BackgroundColor m_background;
    WindowLevelParams m_windowLevel;
    std::vector<TFNode> m_nodes;
    double m_dataRange[2] = { 0.0, 255.0 };
    bool m_isInteracting = false;
    LoadState m_loadState = LoadState::Idle;
    std::array<double, 16> m_modelMatrix = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    std::uint32_t m_visibilityMask =
        VisFlags::ClipPlanes |
        VisFlags::Crosshair |
        VisFlags::RulerAxes;
    std::vector<ObserverEntry> m_observers;
};



