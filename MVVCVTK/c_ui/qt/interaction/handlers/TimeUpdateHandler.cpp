#include "c_ui/qt/interaction/handlers/TimeUpdateHandler.h"

#include "AppInterfaces.h"

#include <vtkCommand.h>
#include <vtkRenderWindow.h>

TimeUpdateHandler::TimeUpdateHandler(AbstractInteractiveService* service, vtkRenderWindow* renderWindow)
    : m_service(service),
      m_renderWindow(renderWindow)
{
}

InteractionResult TimeUpdateHandler::Handle(const InteractionEvent& eve)
{
    if (eve.vtkEventId != vtkCommand::TimerEvent) {
        return {};
    }

    if (!m_service) {
        return { true, false };
    }

    m_service->ProcessPendingUpdates();

    if (m_service->IsDirty()) {
        if (m_service->PresentFrame()) {
            m_service->SetDirty(false);
        }
    }
    return { true, false };
}
