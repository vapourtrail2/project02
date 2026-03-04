#pragma once

#include "c_ui/qt/interaction/IInteractionHandler.h"

class AbstractInteractiveService;
class vtkRenderWindow;

class TimeUpdateHandler : public IInteractionHandler
{
public:
    TimeUpdateHandler(AbstractInteractiveService* service, vtkRenderWindow* renderWindow);

    InteractionResult Handle(const InteractionEvent& eve) override;

private:
    AbstractInteractiveService* m_service = nullptr;
    vtkRenderWindow* m_renderWindow = nullptr;
};
