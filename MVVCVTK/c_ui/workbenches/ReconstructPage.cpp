#include "ReconstructPage.h"

#include <QGridLayout>
#include <QMetaObject>
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
	// 这里的reset是为了提前断开与旧数据和状态的关联，确保在后续赋新值时不会有旧数据触发的回调干扰界面更新。通过先reset，再赋新值，可以保证界面在整个过程中保持一致性和稳定性。
	m_lifeToken.reset();
    m_ctxAxial.reset();
    m_ctxCoronal.reset();
    m_ctxSagittal.reset();
    m_ctx3D.reset();

	m_svc3D.reset();
	m_svcSagittal.reset();
	m_svcCoronal.reset();
	m_svcAxial.reset();
    
    m_dataMgr = std::move(data);
    m_sharedState = std::move(state);
    m_lifeToken = std::make_shared<int>(1);

	if (!m_dataMgr || !m_sharedState) {//openFile() 会先创建 session、发 sessionChanged，但文件还在后台读。此时 GetVtkImage() 很可能是空 不需要!m_datamgr->GetVtkImage() 
        return;
    }

    auto getVtkWidget = [](const QPointer<QWidget>& widget) -> QVTKOpenGLNativeWidget* {
        return qobject_cast<QVTKOpenGLNativeWidget*>(widget.data());
    };

    m_svcAxial = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxAxial = std::make_shared<QtRenderContext>();
    m_ctxAxial->SetQtWidget(getVtkWidget(viewAxial_));
    m_ctxAxial->SetServiceBound(m_svcAxial);
    m_svcAxial->SetVizMode(VizMode::SliceTop_down);
    m_ctxAxial->SetCameraStyleByVizMode(VizMode::SliceTop_down);
    m_ctxAxial->ToggleOrientationAxes(true);

    m_svcCoronal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxCoronal = std::make_shared<QtRenderContext>();
    m_ctxCoronal->SetQtWidget(getVtkWidget(viewCoronal_));
    m_ctxCoronal->SetServiceBound(m_svcCoronal);
    m_svcCoronal->SetVizMode(VizMode::SliceFront_back);
    m_ctxCoronal->SetCameraStyleByVizMode(VizMode::SliceFront_back);
    m_ctxCoronal->ToggleOrientationAxes(true);

    m_svcSagittal = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctxSagittal = std::make_shared<QtRenderContext>();
    m_ctxSagittal->SetQtWidget(getVtkWidget(viewSagittal_));
    m_ctxSagittal->SetServiceBound(m_svcSagittal);
    m_svcSagittal->SetVizMode(VizMode::SliceLeft_right);
    m_ctxSagittal->SetCameraStyleByVizMode(VizMode::SliceLeft_right);
    m_ctxSagittal->ToggleOrientationAxes(true);

    m_svc3D = std::make_shared<MedicalVizService>(m_dataMgr, m_sharedState);
    m_ctx3D = std::make_shared<QtRenderContext>();
    m_ctx3D->SetQtWidget(getVtkWidget(viewReserved_));
    m_ctx3D->SetServiceBound(m_svc3D);
    m_ctx3D->ToggleOrientationAxes(true);
    m_svc3D->SetVizMode(m_current3DMode);
	m_ctx3D->SetCameraStyleByVizMode(m_current3DMode);
    
    if (m_dataMgr) {
        if (m_dataMgr->GetVtkImage()!= nullptr) {
			auto img = m_dataMgr->GetVtkImage();
            double range[2];
			double spacing[3];
            img->GetScalarRange(range);
			img->GetSpacing(spacing);
            m_sharedState->SetDataReady(range[0], range[1],
                { spacing[0], spacing[1], spacing[2] });
        }
    }
    refreshViews();

    if (m_ctxAxial) m_ctxAxial->SetStarted();
    if (m_ctxCoronal) m_ctxCoronal->SetStarted();
    if (m_ctxSagittal) m_ctxSagittal->SetStarted();
    if (m_ctx3D) m_ctx3D->SetStarted();
}

void ReconstructPage::refreshViews()
{
    if (m_svcAxial) m_svcAxial->SetPendingUpdatesProcessed();
    if (m_svcCoronal) m_svcCoronal->SetPendingUpdatesProcessed();
    if (m_svcSagittal) m_svcSagittal->SetPendingUpdatesProcessed();
    if (m_svc3D) m_svc3D->SetPendingUpdatesProcessed();

    if (m_ctxAxial) m_ctxAxial->SetRendered();
    if (m_ctxCoronal) m_ctxCoronal->SetRendered();
    if (m_ctxSagittal) m_ctxSagittal->SetRendered();
    if (m_ctx3D) m_ctx3D->SetRendered();
}



void ReconstructPage::setToolMode(ToolMode mode)
{
    if (m_ctxAxial)
        m_ctxAxial->SetToolMode(mode);

    if (m_ctxCoronal)
		m_ctxCoronal->SetToolMode(mode);

	if (m_ctxSagittal)
		m_ctxSagittal->SetToolMode(mode);
}

void ReconstructPage::setPrimary3DMode(VizMode mode)
{
    if (mode != VizMode::CompositeVolume && mode != VizMode::CompositeIsoSurface) {
        return;
    }

	m_current3DMode = mode;
    
    if (!m_svc3D || !m_ctx3D) {
		return;
    }

    applyPrimary3DMode(mode);
}

void ReconstructPage::applyPrimary3DMode(VizMode mode)
{   
    m_svc3D->SetVizMode(mode);
    m_ctx3D->SetCameraStyleByVizMode(mode);

    request3DRebuildFromCurrentImage();

    m_svc3D->SetPendingUpdatesProcessed();
    m_ctx3D->SetRendered();
}

void ReconstructPage::request3DRebuildFromCurrentImage()
{
    if (!m_dataMgr || !m_sharedState) {
		return;
    }

	auto img = m_dataMgr->GetVtkImage();
    if (!img) {
		return;
    }

    const auto w1 = m_sharedState->GetWindowLevel();
	const auto cursor = m_sharedState->GetCursorWorld();
    const auto rawCurosr = m_sharedState->GetCursorRawWorld();
    const int cursorAxis = m_sharedState->GetCursorAxis();

    double range[2];
    double spacing[3];

	img->GetScalarRange(range);
    img->GetSpacing(spacing);

    m_sharedState->SetDataReady(range[0], range[1],
		{ spacing[0], spacing[1], spacing[2] });

	m_sharedState->SetWindowLevel(w1.windowWidth,w1.windowCenter);
	m_sharedState->SetCursorWorld(cursor[0], cursor[1],cursor[2]);
	m_sharedState->SetCursorRawWorld(rawCurosr[0], rawCurosr[1], rawCurosr[2]);
	m_sharedState->SetCursorAxis(cursorAxis);
}
