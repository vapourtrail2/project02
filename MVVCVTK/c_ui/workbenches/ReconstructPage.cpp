#include "ReconstructPage.h"

#include <QGridLayout>
#include <QWidget>

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
    auto* grid = new QGridLayout(this);
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
    m_dataMgr = std::move(data);
    m_sharedState = std::move(state);

    if (!m_dataMgr || !m_sharedState || !m_dataMgr->GetVtkImage()) {
        return;
    }

    auto getVtkWidget = [](const QPointer<QWidget>& widget) -> QVTKOpenGLNativeWidget* {
        return qobject_cast<QVTKOpenGLNativeWidget*>(widget.data());
    };

    m_svcAxial = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxAxial = std::make_shared<QtRenderContext>();
    m_ctxAxial->SetQtWidget(getVtkWidget(viewAxial_));
    m_ctxAxial->BindService(m_svcAxial);
    m_svcAxial->ShowSlice(VizMode::SliceAxial);
    m_ctxAxial->SetInteractionMode(VizMode::SliceAxial);

    m_svcCoronal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxCoronal = std::make_shared<QtRenderContext>();
    m_ctxCoronal->SetQtWidget(getVtkWidget(viewCoronal_));
    m_ctxCoronal->BindService(m_svcCoronal);
    m_svcCoronal->ShowSlice(VizMode::SliceCoronal);
    m_ctxCoronal->SetInteractionMode(VizMode::SliceCoronal);

    m_svcSagittal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxSagittal = std::make_shared<QtRenderContext>();
    m_ctxSagittal->SetQtWidget(getVtkWidget(viewSagittal_));
    m_ctxSagittal->BindService(m_svcSagittal);
    m_svcSagittal->ShowSlice(VizMode::SliceSagittal);
    m_ctxSagittal->SetInteractionMode(VizMode::SliceSagittal);

    m_svc3D = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctx3D = std::make_shared<QtRenderContext>();
    m_ctx3D->SetQtWidget(getVtkWidget(viewReserved_));
    m_ctx3D->BindService(m_svc3D);
    m_svc3D->Show3DPlanes(VizMode::CompositeIsoSurface);
    m_ctx3D->SetInteractionMode(VizMode::CompositeIsoSurface);

    if (m_svcAxial) m_svcAxial->OnStateChanged();
    if (m_svcCoronal) m_svcCoronal->OnStateChanged();
    if (m_svcSagittal) m_svcSagittal->OnStateChanged();
    if (m_svc3D) m_svc3D->OnStateChanged();

    if (m_svcAxial) m_svcAxial->ProcessPendingUpdates();
    if (m_svcCoronal) m_svcCoronal->ProcessPendingUpdates();
    if (m_svcSagittal) m_svcSagittal->ProcessPendingUpdates();
    if (m_svc3D) m_svc3D->ProcessPendingUpdates();

    if (m_ctxAxial) m_ctxAxial->Render();
    if (m_ctxCoronal) m_ctxCoronal->Render();
    if (m_ctxSagittal) m_ctxSagittal->Render();
    if (m_ctx3D) m_ctx3D->Render();

    if (m_ctxAxial) m_ctxAxial->Start();
    if (m_ctxCoronal) m_ctxCoronal->Start();
    if (m_ctxSagittal) m_ctxSagittal->Start();
    if (m_ctx3D) m_ctx3D->Start();
}
