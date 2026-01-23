#pragma once
#include "AppInterfaces.h" 
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkPropPicker.h>
#include <vtkCallbackCommand.h>
#include <vtkDistanceWidget.h>
#include <vtkAngleWidget.h>
#include <vtkDistanceRepresentation2D.h>
#include <vtkAngleRepresentation2D.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkInteractorStyleImage.h>



class QtRenderContext : public AbstractRenderContext {
public:
    QtRenderContext();
    //初始化
    void SetQtWidget(QVTKOpenGLNativeWidget* widget);
    void Start() override;
    void SetInteractionMode(VizMode mode) override;
	//重写绑定 转为InteractiveService
    void BindService(std::shared_ptr<AbstractAppService> service) override;

protected:
    void HandleVTKEvent(vtkObject* caller, long unsigned int eventId, void* callData) override;

private:
	void SetupObservers();

	std::shared_ptr<AbstractInteractiveService> m_interactiveService;

    vtkSmartPointer<vtkCallbackCommand> m_eventCallback;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkPropPicker> m_picker;

    //测量
    vtkSmartPointer<vtkDistanceWidget> m_distanceWidget;
    vtkSmartPointer<vtkAngleWidget> m_angleWidget;

    VizMode m_currentMode = VizMode::Volume;
    ToolMode m_toolMode = ToolMode::Navigation;

    // 交互状态变量
    bool m_enableDragCrosshair = false;
    bool m_isDragging = false;
    int m_dragAxis = -1;
    // 记录拖拽时的深度值
    double m_dragDepth = 0.0;
	int m_timerId = -1;
    bool m_observerInstalled = false;
};