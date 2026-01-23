#include "c_ui/qt/QtRenderContext.h"
#include "AppService.h"
#include <vtkRenderWindow.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCamera.h>

QtRenderContext::QtRenderContext()
{
    m_eventCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_eventCallback->SetCallback(AbstractRenderContext::DispatchVTKEvent);
    m_eventCallback->SetClientData(this);
    m_picker = vtkSmartPointer<vtkPropPicker>::New();
}

void QtRenderContext::SetQtWidget(QVTKOpenGLNativeWidget* widget)
{
    if (!widget) return;

    //确保存在
    if (!widget->renderWindow()) {
		auto rw = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        widget->setRenderWindow(rw);
    }
    m_renderWindow = widget->renderWindow();    

    //renderer
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderWindow->AddRenderer(m_renderer);

	//用自己的 interactor
    m_interactor = widget->interactor();

	//把interactor和 renderwindow 关联起来
    if (m_interactor && m_renderWindow->GetInteractor() != m_interactor) {
		m_renderWindow->SetInteractor(m_interactor);
		m_interactor->SetRenderWindow(m_renderWindow);
    }

    if (m_interactor && !m_interactor->GetInitialized())
    {
		m_interactor->Initialize();
    }

    if (m_renderWindow) {
        m_renderWindow->Render();
    }
}

void QtRenderContext::BindService(std::shared_ptr<AbstractAppService> service)
{
    // 调用基类绑定
    AbstractRenderContext::BindService(service);
    // 转换为 InteractiveService 以便访问交互接口
    m_interactiveService = std::dynamic_pointer_cast<AbstractInteractiveService>(service);

    SetupObservers();
}

void QtRenderContext::SetupObservers()
{
    if (!m_interactor || m_observerInstalled) return;

    m_interactor->RemoveObserver(m_eventCallback);

    m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseMoveEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::TimerEvent, m_eventCallback, 1.0);

    if (m_timerId != -1) m_interactor->DestroyTimer(m_timerId);
    m_timerId = m_interactor->CreateRepeatingTimer(33);

    if (m_timerId == -1) {
        return;
    }
    m_observerInstalled = true;
}


void QtRenderContext::Start()
{
    if (m_renderWindow) m_renderWindow->Render();
    if (m_interactor && !m_interactor->GetInitialized()) {
        m_interactor->Initialize();
    }
}

void QtRenderContext::SetInteractionMode(VizMode mode)
{
    m_currentMode = mode;
    if (!m_interactor) return;

    if (mode == VizMode::SliceAxial || mode == VizMode::SliceCoronal || mode == VizMode::SliceSagittal) {
        auto style = vtkSmartPointer<vtkInteractorStyleImage>::New();
        style->SetInteractionModeToImage2D();
        m_interactor->SetInteractorStyle(style);
    }
    else {
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
        m_interactor->SetInteractorStyle(style);
    }
}

void QtRenderContext::HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData)
{
    if (!m_interactiveService) return; 

    vtkRenderWindowInteractor* iren = static_cast<vtkRenderWindowInteractor*>(caller);
    int* eventPos = iren->GetEventPosition();

    // 1. 心跳逻辑：处理后端的懒更新 
    if (eventId == vtkCommand::TimerEvent) {
        // 让 Service 处理挂起的数据变更 如tf改变
        m_interactiveService->ProcessPendingUpdates();

        // 如果 Service 标记数据脏了，触发 Qt 窗口重绘
        if (m_interactiveService->IsDirty()) {
           if (m_renderWindow /*&& m_renderWindow->GetMapped()*/) {
                m_renderWindow->Render();
           }
            m_interactiveService->SetDirty(false);
        }
        return;
    }

    // 2. 2D 切片交互逻辑
    if (m_currentMode == VizMode::SliceAxial ||
        m_currentMode == VizMode::SliceCoronal ||
        m_currentMode == VizMode::SliceSagittal)
    {
        // 滚轮切片
        if (eventId == vtkCommand::MouseWheelForwardEvent || eventId == vtkCommand::MouseWheelBackwardEvent)
        {
            int delta = (eventId == vtkCommand::MouseWheelForwardEvent) ? 1 : -1;
            m_interactiveService->UpdateInteraction(delta);
            m_interactiveService->SetDirty(true); // 标记脏，下一帧Timer会渲染
            m_eventCallback->SetAbortFlag(1);
            return;
        }

        // Shift + 左键拖动十字线
        if (eventId == vtkCommand::LeftButtonPressEvent)
        {
            if (iren->GetShiftKey()) {
                m_enableDragCrosshair = true;
                m_interactiveService->SetInteracting(true); // 告诉后端正在交互（可能降采样）
                m_eventCallback->SetAbortFlag(1);
            }
        }
        else if (eventId == vtkCommand::LeftButtonReleaseEvent)
        {
            if (m_enableDragCrosshair) {
                m_enableDragCrosshair = false;
                m_interactiveService->SetInteracting(false);
            }
        }
        else if (eventId == vtkCommand::MouseMoveEvent)
        {
            if (m_enableDragCrosshair)
            {
                // 直接将拾取的世界坐标传给 Service，不再自己在 Context 算索引
                m_picker->Pick(eventPos[0], eventPos[1], 0, m_renderer);
                double* worldPos = m_picker->GetPickPosition();

                m_interactiveService->SyncCursorToWorldPosition(worldPos);
                m_eventCallback->SetAbortFlag(1);
            }
        }
    }
 
    // 3. 3D 视图交互逻辑 (拖动切片平面)
    else if (m_currentMode == VizMode::CompositeVolume || m_currentMode == VizMode::CompositeIsoSurface)
    {
        if (eventId == vtkCommand::LeftButtonPressEvent)
        {
            // 尝试拾取
            if (m_picker->Pick(eventPos[0], eventPos[1], 0, m_renderer)) {
                vtkActor* actor = m_picker->GetActor();
                int axis = m_interactiveService->GetPlaneAxis(actor);

                if (axis != -1) {
                    m_isDragging = true;
                    m_dragAxis = axis;
                    m_interactiveService->SetInteracting(true);
                    m_eventCallback->SetAbortFlag(1); // 阻止转动相机
                }
            }
        }
        else if (eventId == vtkCommand::LeftButtonReleaseEvent)
        {
            if (m_isDragging) {
                m_interactiveService->SetInteracting(false);
                // 强制刷新一次高质量渲染
                m_interactiveService->MarkDirty();
            }
            m_isDragging = false;
            m_dragAxis = -1;
        }
        else if (eventId == vtkCommand::MouseMoveEvent)
        {
            if (m_isDragging && m_dragAxis != -1)
            {
                m_picker->Pick(eventPos[0], eventPos[1], 0, m_renderer);
                double* worldPos = m_picker->GetPickPosition();

                // 调用新的同步接口
                m_interactiveService->SyncCursorToWorldPosition(worldPos);
                m_eventCallback->SetAbortFlag(1);
            }
        }
    }
}