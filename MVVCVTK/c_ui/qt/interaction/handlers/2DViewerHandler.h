#pragma once

#include "c_ui/qt/interaction/IInteractionHandler.h"

class AbstractInteractiveService;
class vtkPropPicker;
class vtkRenderer;

class Viewer2DHandler : public IInteractionHandler
{
public:
    Viewer2DHandler(AbstractInteractiveService* service, vtkPropPicker* picker, vtkRenderer* renderer);

    InteractionResult Handle(const InteractionEvent& eve) override;

private:
    AbstractInteractiveService* m_service = nullptr;
    vtkPropPicker* m_picker = nullptr;
    vtkRenderer* m_renderer = nullptr;
    bool m_enableDragCrosshair = false;
    bool m_enableDragWindowLevel = false;
    int m_lastDragX = 0;
    int m_lastDragY = 0;
};
