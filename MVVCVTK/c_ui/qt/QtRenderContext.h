#pragma once

#include "AppInterfaces.h"
#include "c_ui/qt/interaction/InteractionRouter.h"
#include <QVTKOpenGLNativeWidget.h>
#include <vtkCallbackCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleImage.h>
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

    std::shared_ptr<AbstractInteractiveService> m_interactiveService;
    InteractionRouter m_interactionRouter;

    vtkSmartPointer<vtkCallbackCommand> m_eventCallback;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkPropPicker> m_picker;

    VizMode m_currentMode = VizMode::Volume;
    ToolMode m_toolMode = ToolMode::Navigation;

    int m_timerId = -1;
    bool m_observerInstalled = false;
};
