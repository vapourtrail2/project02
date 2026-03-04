#include "c_ui/qt/QtRenderContext.h"
#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/3DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/TimeUpdateHandler.h"
#include <memory>
#include <vtkCommand.h>
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

    if (!widget->renderWindow()) {
        auto rw = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
        widget->setRenderWindow(rw);
    }
    m_renderWindow = widget->renderWindow();

    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_renderWindow->AddRenderer(m_renderer);

    m_interactor = widget->interactor();
    if (m_interactor && m_renderWindow->GetInteractor() != m_interactor) {
        m_renderWindow->SetInteractor(m_interactor);
        m_interactor->SetRenderWindow(m_renderWindow);
    }

    if (m_interactor && !m_interactor->GetInitialized()) {
        m_interactor->Initialize();
    }

    SetupObservers();
    BuildInteractionRouter();

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
}

void QtRenderContext::SetupObservers()
{
    if (!m_interactor || m_observerInstalled) {
        return;
    }

    m_interactor->RemoveObserver(m_eventCallback);

    m_interactor->AddObserver(vtkCommand::LeftButtonPressEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::LeftButtonReleaseEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseMoveEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelForwardEvent, m_eventCallback, 1.0);
    m_interactor->AddObserver(vtkCommand::MouseWheelBackwardEvent, m_eventCallback, 1.0);
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
    if (!m_interactor) {
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
    m_toolMode = mode;
}

void QtRenderContext::HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData)
{
    (void)callData;

    if (!m_interactiveService) {
        return;
    }

    auto* iren = vtkRenderWindowInteractor::SafeDownCast(caller);
    if (!iren) {
        return;
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
    event.alt = false;
    event.vizMode = m_currentMode;
    event.toolMode = m_toolMode;

    const InteractionResult result = m_interactionRouter.Dispatch(event);
    if (result.abortVtk && m_eventCallback) {
        m_eventCallback->SetAbortFlag(1);
    }
}
