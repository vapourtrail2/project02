#include "c_ui/qt/interaction/handlers/TimeUpdateHandler.h"

#include "core/MVVCVTK/MVVCVTK/AppInterfaces.h"

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

    m_service->SetPendingUpdatesProcessed();

    if (m_service->IsDirty()) {
        if (m_renderWindow) {
            m_renderWindow->Render();
        }
        m_service->SetDirty(false);
    }
    return { true, false };
}
