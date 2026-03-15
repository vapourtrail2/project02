#pragma once
#include <QWidget>
#include <QPointer>

#include <memory>
#include "AppService.h"
#include "DataManager.h"
#include "AppState.h"
#include "c_ui/qt/QtRenderContext.h"

class ReconstructPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReconstructPage(QWidget* parent = nullptr);
    void initWithData(
        std::shared_ptr<AbstractDataManager> data,
        std::shared_ptr<SharedInteractionState> state);

private:
    void buildUi();
    void applyPrimary3DMode(VizMode mode);
    void refreshViews();

    QPointer<QWidget> viewAxial_;
    QPointer<QWidget> viewSagittal_;
    QPointer<QWidget> viewCoronal_;
    QPointer<QWidget> viewReserved_;

    std::shared_ptr<AbstractDataManager> m_dataMgr;
    std::shared_ptr<SharedInteractionState> m_sharedState;
    std::shared_ptr<void> m_lifeToken;

    std::shared_ptr<MedicalVizService> m_svcAxial;
    std::shared_ptr<QtRenderContext>   m_ctxAxial;
    std::shared_ptr<MedicalVizService> m_svcCoronal;
    std::shared_ptr<QtRenderContext>   m_ctxCoronal;
    std::shared_ptr<MedicalVizService> m_svcSagittal;
    std::shared_ptr<QtRenderContext>   m_ctxSagittal;
    std::shared_ptr<MedicalVizService> m_svc3D;
    std::shared_ptr<QtRenderContext>   m_ctx3D;
    VizMode m_current3DMode = static_cast<VizMode>(-1);
};
