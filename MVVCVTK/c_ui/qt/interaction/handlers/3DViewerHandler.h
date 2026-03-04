#pragma once

#include "c_ui/qt/interaction/IInteractionHandler.h"

class AbstractInteractiveService;
class vtkPropPicker;
class vtkRenderer;

class Viewer3DHandler : public IInteractionHandler
{
public:
    Viewer3DHandler(AbstractInteractiveService* service, vtkPropPicker* picker, vtkRenderer* renderer);

    InteractionResult Handle(const InteractionEvent& eve) override;

private:
    AbstractInteractiveService* m_service = nullptr;
    vtkPropPicker* m_picker = nullptr;
    vtkRenderer* m_renderer = nullptr;

    bool m_isDragging = false;
    int m_dragAxis = -1;
};
