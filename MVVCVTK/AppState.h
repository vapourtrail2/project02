#pragma once
#include "AppInterfaces.h"
#include <vector>
#include <functional>
#include <memory>
#include <array>
#include <cmath>
#include <mutex>

// 定义控制节点结构
struct RenderNode {
    double position; // 0.0 - 1.0 (归一化位置) 阈值
    double opacity;  // 0.0 - 1.0 透明度
    double r, g, b;  // 颜色 lux
};

// 定义观察者回调类型
using ObserverCallback = std::function<void(UpdateFlags)>;

// 内部结构体，保存自有指针和回调
struct ObserverEntry {
    std::weak_ptr<void> owner;      // 存活凭证 (不增加引用计数)
    ObserverCallback callback; // 执行逻辑
};

class SharedInteractionState {
private:
    // 统一保护状态数据与观察者列表，支持跨线程读写
    mutable std::mutex m_mutex;
    // 线程模型约束：观察者回调在触发 NotifyObservers 的同一线程执行
    // 若观察者需要操作 UI，应自行切换到 UI 线程。
    //全局共享的当前切片坐标
    int m_cursorPosition[3] = { 0, 0, 0 };
    // 观察者列表：存放所有需要刷新的窗口的回调函数
    std::vector<ObserverEntry> m_observers;
    double m_isoValue = 0.0;   // 等值面提取阈值
    MaterialParams m_material; // 材质与光照状态
    // --- 渲染状态数据 ---
    std::vector<TFNode> m_nodes;
    double m_dataRange[2] = { 0.0, 255.0 }; // 数据标量范围
    bool m_isInteracting = false; // 状态接口
public:
    SharedInteractionState() {
        // 初始化默认的4个节点
        // Point 0: Min
        m_nodes.push_back({ 0.0, 0.0, 0.0, 0.0, 0.0 });
        // Point 1: Low-Mid
        m_nodes.push_back({ 0.35, 0.0, 0.75, 0.75, 0.75 });
        // Point 2: High-Mid
        m_nodes.push_back({ 0.60, 0.6, 0.85, 0.85, 0.85 });
        // Point 3: Max
        m_nodes.push_back({ 1.0, 1.0, 0.95, 0.95, 0.95 });
    }

    // 设置数据范围 (用于将归一化节点映射到真实标量)
    void SetScalarRange(double min, double max) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dataRange[0] = min;
        m_dataRange[1] = max;
    }

    std::array<double, 2> GetDataRange() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_dataRange[0], m_dataRange[1] };
    }


    // 修改节点参数
    void SetTFNodes(const std::vector<TFNode>& nodes) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_nodes = nodes;
        }
        NotifyObservers(UpdateFlags::TF);
    }
    // 获取节点列表
    std::vector<TFNode> GetTFNodes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_nodes;
    }

    // 阈值接口
    void SetIsoValue(double val) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (std::abs(m_isoValue - val) > 0.0001) {
                m_isoValue = val;
                changed = true;
            }
        }
        if (changed) {
            NotifyObservers(UpdateFlags::IsoValue);
        }
    }
    double GetIsoValue() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isoValue;
    }

    // 材质接口
    void SetMaterial(const MaterialParams& mat) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_material = mat;
        }
        NotifyObservers(UpdateFlags::Material);
    }
    MaterialParams GetMaterial() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_material;
    }

    // 修改状态接口
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

    bool IsInteracting() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isInteracting;
    }

    // 设置位置，并通知所有人
    void SetCursorPosition(int x, int y, int z) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_cursorPosition[0] == x && m_cursorPosition[1] == y && m_cursorPosition[2] == z) {
                return;
            }
            m_cursorPosition[0] = x;
            m_cursorPosition[1] = y;
            m_cursorPosition[2] = z;
            changed = true;
        }

        if (changed) {
            NotifyObservers(UpdateFlags::Cursor);
        }
    }

    // 更新某个轴
    void UpdateAxis(int axisIndex, int delta, int maxDim) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_cursorPosition[axisIndex] += delta;
            if (m_cursorPosition[axisIndex] < 0) m_cursorPosition[axisIndex] = 0;
            if (m_cursorPosition[axisIndex] >= maxDim) m_cursorPosition[axisIndex] = maxDim - 1;
        }

        NotifyObservers(UpdateFlags::Cursor);
    }

    std::array<int, 3> GetCursorPosition() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return { m_cursorPosition[0], m_cursorPosition[1], m_cursorPosition[2] };
    }

    // 注册观察者
    void AddObserver(std::shared_ptr<void> owner, ObserverCallback cb) {
        if (!owner) return;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_observers.push_back({ owner, cb });
    }

private:
    void NotifyObservers(UpdateFlags flags) {// 数据改变时 遍历列表通知所有窗口
        std::vector<ObserverCallback> callbacks;
        // 锁内清理失效观察者并收集可执行回调；锁外执行，避免回调重入造成死锁
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto it = m_observers.begin(); it != m_observers.end();) {
                if (it->owner.expired())//expired()表示弱指针所指向的对象已经被销毁
                {
                    // erase 返回下一个有效的迭代器，不要再自增
                    it = m_observers.erase(it);
                }   
                else
                {   
                    if (it->callback) {
                        callbacks.push_back(it->callback);
                    }
                    ++it;
                }
            }
        }
        for (const auto& cb : callbacks) {
            cb(flags);
        }
    }
};
