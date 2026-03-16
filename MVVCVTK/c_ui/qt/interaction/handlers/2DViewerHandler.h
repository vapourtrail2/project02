#pragma once

#include "c_ui/qt/interaction/IInteractionHandler.h"

class AbstractInteractiveService;
class vtkCellPicker;
class vtkRenderer;

class Viewer2DHandler : public IInteractionHandler
{
public:
    Viewer2DHandler(AbstractInteractiveService* service, vtkCellPicker* picker, vtkRenderer* renderer);

    InteractionResult Handle(const InteractionEvent& eve) override;

private:
    AbstractInteractiveService* m_service = nullptr;
    vtkCellPicker* m_picker = nullptr;
    vtkRenderer* m_renderer = nullptr;
    bool m_enableDragZoom = false;
    bool m_enableDragCrosshair = false;
    bool m_enableDragWindowLevel = false;

    //放大 缩小
	int m_lastZoomX = 0;
    int m_lastZoomY = 0;

    int m_lastDragX = 0;
    int m_lastDragY = 0;

    int m_crosshairMoveLogTick = 0;
    int m_windowLevelMoveLogTick = 0;
    int m_ZoomMoveLog = 0;
};
