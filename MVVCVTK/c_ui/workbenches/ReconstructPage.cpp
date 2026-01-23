#include "ReconstructPage.h"
#include <QGridLayout>
#include <QWidget>
#include "c_ui/macro/VtkMacros.h"


#include <QVTKOpenGLNativeWidget.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>


ReconstructPage::ReconstructPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ReconstructPage::buildUi()
{
    auto grid = new QGridLayout(this);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    viewAxial_ = new QVTKOpenGLNativeWidget(this);
    viewSagittal_ = new QVTKOpenGLNativeWidget(this);
    viewCoronal_ = new QVTKOpenGLNativeWidget(this);
    viewReserved_ = new QVTKOpenGLNativeWidget(this);

    grid->addWidget(viewAxial_, 0, 0);
    grid->addWidget(viewSagittal_, 1, 0);
    grid->addWidget(viewCoronal_, 0, 1);
    grid->addWidget(viewReserved_, 1, 1);
}

void ReconstructPage::initWithData(
    std::shared_ptr<AbstractDataManager> data,
    std::shared_ptr<SharedInteractionState> state)
{
    m_dataMgr = std::dynamic_pointer_cast<RawVolumeDataManager>(data);
    m_sharedState = state;

    if (!m_dataMgr || !m_sharedState) return;

    // 辅助函数
    auto getVtkW = [](QPointer<QWidget> w) -> QVTKOpenGLNativeWidget* {
        return qobject_cast<QVTKOpenGLNativeWidget*>(w.data());
        };

    // 设置一些全局初始参数 
    if (m_dataMgr->GetVtkImage()) {
        double range[2];
        m_dataMgr->GetVtkImage()->GetScalarRange(range);
        // 设置默认 ISO 阈值
        m_sharedState->SetIsoValue(range[0] + (range[1] - range[0]) * 0.3);
    }

    // 1. 初始化 Axial
    m_svcAxial = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxAxial = std::make_shared<QtRenderContext>();
    m_ctxAxial->SetQtWidget(getVtkW(viewAxial_));
    m_ctxAxial->BindService(m_svcAxial);
    m_svcAxial->ShowSlice(VizMode::SliceAxial);
    m_ctxAxial->SetInteractionMode(VizMode::SliceAxial);

    // 2. 初始化 Coronal
    m_svcCoronal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxCoronal = std::make_shared<QtRenderContext>();
    m_ctxCoronal->SetQtWidget(getVtkW(viewCoronal_));
    m_ctxCoronal->BindService(m_svcCoronal);
    m_svcCoronal->ShowSlice(VizMode::SliceCoronal);
    m_ctxCoronal->SetInteractionMode(VizMode::SliceCoronal);

    // 3. 初始化 Sagittal
    m_svcSagittal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxSagittal = std::make_shared<QtRenderContext>();
    m_ctxSagittal->SetQtWidget(getVtkW(viewSagittal_));
    m_ctxSagittal->BindService(m_svcSagittal);
    m_svcSagittal->ShowSlice(VizMode::SliceSagittal);
    m_ctxSagittal->SetInteractionMode(VizMode::SliceSagittal);

    // 4. 初始化 3D
    m_svc3D = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctx3D = std::make_shared<QtRenderContext>();
    m_ctx3D->SetQtWidget(getVtkW(viewReserved_));
    m_ctx3D->BindService(m_svc3D);

    // 设置初始材质参数
    m_svc3D->SetLuxParams(0.3, 0.6, 0.2, 15.0);
    m_svc3D->Show3DPlanes(VizMode::CompositeIsoSurface);
    m_ctx3D->SetInteractionMode(VizMode::CompositeIsoSurface);

    if (m_svcAxial) m_svcAxial->OnStateChanged();
    if (m_svcCoronal) m_svcCoronal->OnStateChanged();
    if (m_svcSagittal) m_svcSagittal->OnStateChanged();
    if (m_svc3D) m_svc3D->OnStateChanged();

    // 强制先处理一次挂起的更新
    if (m_svcAxial) m_svcAxial->ProcessPendingUpdates();
    if (m_svcCoronal) m_svcCoronal->ProcessPendingUpdates();
    if (m_svcSagittal) m_svcSagittal->ProcessPendingUpdates();
    if (m_svc3D) m_svc3D->ProcessPendingUpdates();

    // 触发各个窗口的首次渲染
    if (m_ctxAxial) m_ctxAxial->Render();
    if (m_ctxCoronal) m_ctxCoronal->Render();
    if (m_ctxSagittal) m_ctxSagittal->Render();
    if (m_ctx3D) m_ctx3D->Render();

    m_ctxAxial->Start();
    m_ctxCoronal->Start();
    m_ctxSagittal->Start();
    m_ctx3D->Start();
}