#pragma once

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QPixmap>
#include <memory>

#include "AppState.h"   // SharedInteractionState, MaterialParams
// AppState.h 内部已经包含 AppInterfaces.h，所以 UpdateFlags 也在里面

struct AppSession;              // 来自 AppController.h
class VolumeAnalysisService;    // 来自 AppService.h

class RenderPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RenderPanel(QWidget* parent = nullptr);
    ~RenderPanel() override;

    // 绑定当前会话（推荐：一次性把 sharedState + analysisService 都绑定）
    void setSession(const std::shared_ptr<AppSession>& session);

    // 也可拆开绑定
    void setSharedState(const std::shared_ptr<SharedInteractionState>& state);
    void setAnalysisService(const std::shared_ptr<VolumeAnalysisService>& analysis);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void syncFromState(UpdateFlags flags);
    /*void refreshHistogram(); */   // 生成/加载直方图图片
    void applyHistogramPixmap();// 把 histPixmap_ 按当前label尺寸缩放显示

private:
    // UI
    QLabel* histLabel_ = nullptr;

    QLabel* isoValueLabel_ = nullptr;
    QSlider* isoSlider_ = nullptr;

    QSlider* ambient_ = nullptr;
    QSlider* diffuse_ = nullptr;
    QSlider* specular_ = nullptr;
    QSlider* power_ = nullptr;
    QCheckBox* shadeOn_ = nullptr;

    // 业务对象
    std::shared_ptr<SharedInteractionState> state_;
    std::shared_ptr<VolumeAnalysisService> analysis_;

    // 观察者存活凭证（用于 SharedInteractionState::AddObserver）
    std::shared_ptr<void> lifeToken_;

    // UI 同步保护
    bool updatingUi_ = false;

    // iso 映射范围
    double rangeMin_ = 0.0;
    double rangeMax_ = 1.0;

    // 直方图缓存
    QString histCachePath_;
    QPixmap histPixmap_;
};
