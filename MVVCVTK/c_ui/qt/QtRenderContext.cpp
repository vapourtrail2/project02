#include "c_ui/qt/QtRenderContext.h"
#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/3DViewerHandler.h"
#include "c_ui/qt/interaction/handlers/TimeUpdateHandler.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDebug>
#include <memory>
#include <string>
#include <vtkCommand.h>
#include <vtkProp3D.h>
#include <vtkRenderWindow.h>

namespace {
class WidgetInputFilter : public QObject
{
public:
    explicit WidgetInputFilter(std::function<bool(QEvent*)> handler)
        : m_handler(std::move(handler))
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        (void)watched;
        if (m_handler && m_handler(event)) {
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<bool(QEvent*)> m_handler;
};

bool IsSliceMode(VizMode mode)
{
    return mode == VizMode::SliceAxial
        || mode == VizMode::SliceCoronal
        || mode == VizMode::SliceSagittal;
}

const char* ToVtkEventName(unsigned long eventId)
{
    switch (eventId) {
    case vtkCommand::LeftButtonPressEvent: return "LeftPress";
    case vtkCommand::LeftButtonReleaseEvent: return "LeftRelease";
    case vtkCommand::RightButtonPressEvent: return "RightPress";
    case vtkCommand::RightButtonReleaseEvent: return "RightRelease";
    case vtkCommand::MouseMoveEvent: return "MouseMove";
    case vtkCommand::MouseWheelForwardEvent: return "WheelForward";
    case vtkCommand::MouseWheelBackwardEvent: return "WheelBackward";
    case vtkCommand::TimerEvent: return "Timer";
    default: return "Other";
    }
}
}

QtRenderContext::QtRenderContext()
{
    m_eventCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_eventCallback->SetCallback(AbstractRenderContext::DispatchVTKEvent);
    m_eventCallback->SetClientData(this);
    m_picker = vtkSmartPointer<vtkPropPicker>::New();
    m_slicePicker = vtkSmartPointer<vtkCellPicker>::New();
    m_slicePicker->SetTolerance(0.005);
}

QtRenderContext::~QtRenderContext()
{
    if (m_service) {
        m_service->SetPresentCallback({});
    }
    RemoveQtEventFilter();
    if (m_qtEventFilter) {
        delete m_qtEventFilter;
        m_qtEventFilter = nullptr;
    }
    TeardownObservers();
    DetachRendererFromWindow();
}

void QtRenderContext::DetachRendererFromWindow()
{
    if (m_renderWindow && m_renderer) {
        m_renderWindow->RemoveRenderer(m_renderer);
    }
}

void QtRenderContext::InstallQtEventFilter()
{
    if (!m_widget) {
        return;
    }

    if (!m_qtEventFilter) {
        m_qtEventFilter = new WidgetInputFilter([this](QEvent* event) {
            return HandleQtInputEvent(event);
        });
    }

    m_widget->removeEventFilter(m_qtEventFilter);
    m_widget->installEventFilter(m_qtEventFilter);

    const auto children = m_widget->findChildren<QObject*>();
    for (QObject* child : children) {
        if (!child) {
            continue;
        }
        child->removeEventFilter(m_qtEventFilter);
        child->installEventFilter(m_qtEventFilter);
    }

    m_widget->setMouseTracking(true);
}

void QtRenderContext::RemoveQtEventFilter()
{
    if (m_widget && m_qtEventFilter) {
        m_widget->removeEventFilter(m_qtEventFilter);
        const auto children = m_widget->findChildren<QObject*>();
        for (QObject* child : children) {
            if (!child) {
                continue;
            }
            child->removeEventFilter(m_qtEventFilter);
        }
    }
}

bool QtRenderContext::HandleQtInputEvent(QEvent* event)
{
    if (!event || !m_widget) {
        return false;
    }
    if (!IsSliceMode(m_currentMode) || m_toolMode != ToolMode::Navigation) {
        return false;
    }

    switch (event->type()) {
    case QEvent::ContextMenu:
        return true;
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::RightButton) {
            m_widget->setFocus(Qt::MouseFocusReason);
        }
        break;
    }
    default:
        break;
    }

    return false;
}

void QtRenderContext::SetQtWidget(QVTKOpenGLNativeWidget* widget)
{
    if (!widget) {
        return;
    }

    RemoveQtEventFilter();
    m_widget = widget;
    m_widget->setFocusPolicy(Qt::StrongFocus);
    m_widget->setContextMenuPolicy(Qt::NoContextMenu);

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
    UpdatePresentCallback();
    InstallQtEventFilter();

    if (m_renderWindow) {
        m_renderWindow->Render();
    }
}

void QtRenderContext::BindService(std::shared_ptr<AbstractAppService> service)
{
    AbstractRenderContext::BindService(service);
    m_interactiveService = std::dynamic_pointer_cast<AbstractInteractiveService>(service);

    UpdatePresentCallback();
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

void QtRenderContext::UpdatePresentCallback()
{
    if (!m_service) {
        return;
    }

    QPointer<QVTKOpenGLNativeWidget> widget = m_widget;
    vtkSmartPointer<vtkRenderWindow> renderWindow = m_renderWindow;
    m_service->SetPresentCallback([widget, renderWindow]() {
        if (!renderWindow) {
            return;
        }

        renderWindow->Render();
        if (widget) {
            widget->update();
        }
    });
}

void QtRenderContext::BuildInteractionRouter()
{
    m_interactionRouter.Clear();
    if (!m_interactiveService) {
        return;
    }

    m_interactionRouter.Add(std::make_unique<TimeUpdateHandler>(m_interactiveService.get(), m_renderWindow.GetPointer()));
    m_interactionRouter.Add(std::make_unique<Viewer2DHandler>(m_interactiveService.get(), m_slicePicker.GetPointer(), m_renderer.GetPointer()));
    m_interactionRouter.Add(std::make_unique<Viewer3DHandler>(m_interactiveService.get(), m_picker.GetPointer(), m_renderer.GetPointer()));
}

void QtRenderContext::Start()
{
    InstallQtEventFilter();
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

    if (IsSliceMode(mode)) {
        auto style = vtkSmartPointer<vtkInteractorStyleUser>::New();
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

    //if (mode == ToolMode::distanceclear)
    //{
    //    m_distanceWidget->Delete();

    //}
    //else if (mode == ToolMode::angtleclear) { 
    //    dele
    //}
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

    static int sSliceMoveLogTick = 0;
    if (IsSliceMode(m_currentMode)
        && (eventId == vtkCommand::RightButtonPressEvent
            || eventId == vtkCommand::RightButtonReleaseEvent
            || eventId == vtkCommand::MouseMoveEvent
            || eventId == vtkCommand::MouseWheelForwardEvent
            || eventId == vtkCommand::MouseWheelBackwardEvent)) {
        const bool shouldLog = (eventId != vtkCommand::MouseMoveEvent)
            || (++sSliceMoveLogTick <= 5)
            || ((sSliceMoveLogTick % 10) == 0);
        if (shouldLog) {
            qDebug().noquote() << "[VTK] event=" << ToVtkEventName(eventId)
                               << " mode=" << static_cast<int>(m_currentMode)
                               << " tool=" << static_cast<int>(m_toolMode)
                               << " x=" << event.x << " y=" << event.y
                               << " shift=" << event.shift << " ctrl=" << event.ctrl;
        }
    }

    const RouterDispatchMode dispatchMode =
        (eventId == vtkCommand::TimerEvent) ? RouterDispatchMode::Broadcast : RouterDispatchMode::FirstMatch;

    const InteractionResult result = m_interactionRouter.Dispatch(event, dispatchMode);
    if (IsSliceMode(m_currentMode)
        && eventId != vtkCommand::TimerEvent
        && (eventId == vtkCommand::RightButtonPressEvent
            || eventId == vtkCommand::RightButtonReleaseEvent
            || eventId == vtkCommand::MouseMoveEvent
            || eventId == vtkCommand::MouseWheelForwardEvent
            || eventId == vtkCommand::MouseWheelBackwardEvent)) {
        const bool shouldLogResult = (eventId != vtkCommand::MouseMoveEvent)
            || (sSliceMoveLogTick <= 5)
            || ((sSliceMoveLogTick % 10) == 0);
        if (shouldLogResult) {
            qDebug().noquote() << "[VTK] dispatch handled=" << result.handled << " abort=" << result.abortVtk;
        }
    }
    if (result.abortVtk && m_eventCallback) {
        m_eventCallback->SetAbortFlag(1);
    }
}
