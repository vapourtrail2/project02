#pragma once

#include "AppInterfaces.h"
#include "c_ui/qt/interaction/InteractionRouter.h"
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkAngleWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkCellPicker.h>
#include <vtkDistanceWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleUser.h>
#include <vtkInteractorStyleTrackballActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPropPicker.h>
#include <vtkRenderWindowInteractor.h>

class QtRenderContext : public AbstractRenderContext
{
public:
    QtRenderContext();
    ~QtRenderContext() override;

    void SetQtWidget(QVTKOpenGLNativeWidget* widget);
    void Start() override;
    void SetInteractionMode(VizMode mode) override;
    void SetToolMode(ToolMode mode);
    void BindService(std::shared_ptr<AbstractAppService> service) override;

protected:
    void HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData) override;

private:
    void SetupObservers();
    void TeardownObservers();
    void BuildInteractionRouter();
    void DetachRendererFromWindow();
    void UpdatePresentCallback();
    bool HandleQtInputEvent(QEvent* event);
    void InstallQtEventFilter();
    void RemoveQtEventFilter();

    std::shared_ptr<AbstractInteractiveService> m_interactiveService;
    InteractionRouter m_interactionRouter;

    QPointer<QVTKOpenGLNativeWidget> m_widget;
    QObject* m_qtEventFilter = nullptr;
    vtkSmartPointer<vtkCallbackCommand> m_eventCallback;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkPropPicker> m_picker;
    vtkSmartPointer<vtkCellPicker> m_slicePicker;
    vtkSmartPointer<vtkDistanceWidget> m_distanceWidget;
    vtkSmartPointer<vtkAngleWidget> m_angleWidget;

    VizMode m_currentMode = VizMode::Volume;
    ToolMode m_toolMode = ToolMode::Navigation;

    int m_timerId = -1;
    bool m_observerInstalled = false;
};