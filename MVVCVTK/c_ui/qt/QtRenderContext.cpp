#include "c_ui/qt/QtRenderContext.h"
#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/3DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/TimeUpdateHandler.h"
#include <memory>
#include <string>
#include <vtkCommand.h>
#include <vtkProp3D.h>
#include <vtkRenderWindow.h>

QtRenderContext::QtRenderContext()
{
    m_eventCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_eventCallback->SetCallback(AbstractRenderContext::DispatchVTKEvent);
    m_eventCallback->SetClientData(this);
    m_picker = vtkSmartPointer<vtkPropPicker>::New();
}

QtRenderContext::~QtRenderContext()
{
    TeardownObservers();
    DetachRendererFromWindow();
}

void QtRenderContext::DetachRendererFromWindow()
{
    if (m_renderWindow && m_renderer) {
        m_renderWindow->RemoveRenderer(m_renderer);
    }
}

void QtRenderContext::SetQtWidget(QVTKOpenGLNativeWidget* widget)
{
    if (!widget) {
        return;
    }

    vtkRenderWindowInteractor* incomingInteractor = widget->interactor();
    if (m_interactor && incomingInteractor && m_interactor.GetPointer() != incomingInteractor) {
        TeardownObservers();
    }

    DetachRendererFromWindow();

    if (!widget->renderWindow()) {
        auto rw = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        widget->setRenderWindow(rw);
    }
    m_renderWindow = widget->renderWindow();

    if (!m_renderer) {
        m_renderer = vtkSmartPointer<vtkRenderer>::New();
    }
    m_renderWindow->AddRenderer(m_renderer);

    m_interactor = widget->interactor();
    if (m_interactor && m_renderWindow->GetInteractor() != m_interactor) {
        m_renderWindow->SetInteractor(m_interactor);
        m_interactor->SetRenderWindow(m_renderWindow);
    }

    if (m_interactor && !m_interactor->GetInitialized()) {
        m_interactor->Initialize();
    }

    if (m_interactor && !m_distanceWidget) {
        m_distanceWidget = vtkSmartPointer<vtkDistanceWidget>::New();
        m_distanceWidget->SetInteractor(m_interactor);
        m_distanceWidget->CreateDefaultRepresentation();
        m_distanceWidget->SetPriority(1.0);
    }

    if (m_interactor && !m_angleWidget) {
        m_angleWidget = vtkSmartPointer<vtkAngleWidget>::New();
        m_angleWidget->SetInteractor(m_interactor);
        m_angleWidget->CreateDefaultRepresentation();
        m_angleWidget->SetPriority(1.0);
    }

    SetupObservers();
    BuildInteractionRouter();
    SetToolMode(m_toolMode);

    if (m_renderWindow) {
        m_renderWindow->Render();
    }
}

void QtRenderContext::BindService(std::shared_ptr<AbstractAppService> service)
{
    AbstractRenderContext::BindService(service);
    m_interactiveService = std::dynamic_pointer_cast<AbstractInteractiveService>(service);

    BuildInteractionRouter();
    SetupObservers();
    SetToolMode(m_toolMode);
}

void QtRenderContext::SetupObservers()
{
    if (!m_interactor || m_observerInstalled) {
        return;
    }

    m_interactor->RemoveObserver(m_eventCallback);

    m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::RightButtonPressEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::RightButtonReleaseEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseMoveEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::KeyPressEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::InteractionEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::ExitEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::TimerEvent, m_eventCallback, 1.0);

    if (m_timerId != -1) {
        m_interactor->DestroyTimer(m_timerId);
    }
    m_timerId = m_interactor->CreateRepeatingTimer(33);
    if (m_timerId == -1) {
        return;
    }

    m_observerInstalled = true;
}

void QtRenderContext::TeardownObservers()
{
    if (!m_interactor) {
        return;
    }

    if (m_timerId != -1) {
        m_interactor->DestroyTimer(m_timerId);
        m_timerId = -1;
    }

    if (m_eventCallback) {
        m_interactor->RemoveObserver(m_eventCallback);
    }

    m_observerInstalled = false;
}

void QtRenderContext::BuildInteractionRouter()
{
    m_interactionRouter.Clear();
    if (!m_interactiveService) {
        return;
    }

    m_interactionRouter.Add(std::make_unique<TimeUpdateHandler>(m_interactiveService.get(), m_renderWindow.GetPointer()));
    m_interactionRouter.Add(std::make_unique<Viewer2DHandler>(m_interactiveService.get(), m_picker.GetPointer(), m_renderer.GetPointer()));
    m_interactionRouter.Add(std::make_unique<Viewer3DHandler>(m_interactiveService.get(), m_picker.GetPointer(), m_renderer.GetPointer()));
}

void QtRenderContext::Start()
{
    if (m_renderWindow) {
        m_renderWindow->Render();
    }
    if (m_interactor && !m_interactor->GetInitialized()) {
        m_interactor->Initialize();
    }
}

void QtRenderContext::SetInteractionMode(VizMode mode)
{
    m_currentMode = mode;
    if (!m_interactor || m_toolMode == ToolMode::ModelTransform) {
        return;
    }

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

void QtRenderContext::SetToolMode(ToolMode mode)
{
    if (m_toolMode == ToolMode::ModelTransform && m_interactiveService) {
        if (vtkProp3D* mainProp = m_interactiveService->GetMainProp()) {
            mainProp->SetPickable(false);
        }
    }

    m_toolMode = mode;

    if (m_distanceWidget) {
        (mode == ToolMode::DistanceMeasure) ? m_distanceWidget->On() : m_distanceWidget->Off();
    }
    if (m_angleWidget) {
        (mode == ToolMode::AngleMeasure) ? m_angleWidget->On() : m_angleWidget->Off();
    }

    if (!m_interactor) {
        return;
    }

    if (mode == ToolMode::ModelTransform) {
        if (m_interactiveService) {
            if (vtkProp3D* mainProp = m_interactiveService->GetMainProp()) {
                mainProp->SetPickable(true);
            }
        }
        auto style = vtkSmartPointer<vtkInteractorStyleTrackballActor>::New();
        m_interactor->SetInteractorStyle(style);
    }
    else {
        SetInteractionMode(m_currentMode);
    }

    if (m_interactiveService) {
        m_interactiveService->SetDirty(true);
    }
}

void QtRenderContext::HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData)
{
    (void)callData;

    if (eventId == vtkCommand::ExitEvent) {
        if (m_interactor && m_timerId != -1) {
            m_interactor->DestroyTimer(m_timerId);
            m_timerId = -1;
        }
        return;
    }

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren || !m_interactiveService) {
        return;
    }

    if (eventId == vtkCommand::KeyPressEvent) {
        const char key = iren->GetKeyCode();
        const std::string keySym = iren->GetKeySym() ? iren->GetKeySym() : "";

        if (key == 'm' || key == 'M') {
            SetToolMode(m_toolMode == ToolMode::ModelTransform ? ToolMode::Navigation : ToolMode::ModelTransform);
            return;
        }
        if (key == 'd' || key == 'D') {
            SetToolMode(ToolMode::DistanceMeasure);
            return;
        }
        if (key == 'a' || key == 'A') {
            SetToolMode(ToolMode::AngleMeasure);
            return;
        }
        if (keySym == "Escape") {
            SetToolMode(ToolMode::Navigation);
            return;
        }
    }

    if (m_toolMode == ToolMode::ModelTransform && eventId == vtkCommand::InteractionEvent) {
        if (vtkProp3D* prop = m_interactiveService->GetMainProp()) {
            if (prop->GetUserMatrix()) {
                m_interactiveService->SyncModelMatrix(prop->GetUserMatrix());
                m_interactiveService->MarkDirty();
            }
        }
        return;
    }

    if (m_toolMode == ToolMode::DistanceMeasure || m_toolMode == ToolMode::AngleMeasure) {
        if (eventId == vtkCommand::LeftButtonPressEvent
            || eventId == vtkCommand::LeftButtonReleaseEvent
            || eventId == vtkCommand::MouseMoveEvent) {
            return;
        }
    }

    InteractionEvent event;
    event.vtkEventId = eventId;
    event.iren = iren;

    int* eventPos = iren->GetEventPosition();
    if (eventPos) {
        event.x = eventPos[0];
        event.y = eventPos[1];
    }

    event.shift = iren->GetShiftKey() != 0;
    event.ctrl = iren->GetControlKey() != 0;
    event.alt = iren->GetAltKey() != 0;
    event.keyCode = iren->GetKeyCode();
    event.keySym = iren->GetKeySym() ? iren->GetKeySym() : "";
    event.vizMode = m_currentMode;
    event.toolMode = m_toolMode;

    const RouterDispatchMode dispatchMode =
        (eventId == vtkCommand::TimerEvent) ? RouterDispatchMode::Broadcast : RouterDispatchMode::FirstMatch;

    const InteractionResult result = m_interactionRouter.Dispatch(event, dispatchMode);
    if (result.abortVtk && m_eventCallback) {
        m_eventCallback->SetAbortFlag(1);
    }
}
