#pragma once
#include <QWidget>
#include <QPointer>

#include <memory>
#include "AppService.h"    
#include "DataManager.h"  
#include "AppState.h"      
#include "c_ui/qt/QtRenderContext.h" 

//四视图工作区 
class ReconstructPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReconstructPage(QWidget* parent = nullptr);
    void initWithData(
        std::shared_ptr<AbstractDataManager> data,
        std::shared_ptr<SharedInteractionState> state);

private:
    // 构建页面布局
    void buildUi();

private:
    QPointer<QWidget> viewAxial_;
    QPointer<QWidget> viewSagittal_;
    QPointer<QWidget> viewCoronal_;
    QPointer<QWidget> viewReserved_;

    std::shared_ptr<RawVolumeDataManager> m_dataMgr;
    std::shared_ptr<SharedInteractionState> m_sharedState;

    // 轴状位
    std::shared_ptr<MedicalVizService> m_svcAxial;
    std::shared_ptr<QtRenderContext>   m_ctxAxial;
    // 冠状位
    std::shared_ptr<MedicalVizService> m_svcCoronal;
    std::shared_ptr<QtRenderContext>   m_ctxCoronal;
    // 矢状位
    std::shared_ptr<MedicalVizService> m_svcSagittal;
    std::shared_ptr<QtRenderContext>   m_ctxSagittal;
    // 3D视图
    std::shared_ptr<MedicalVizService> m_svc3D;
    std::shared_ptr<QtRenderContext>   m_ctx3D;
};
