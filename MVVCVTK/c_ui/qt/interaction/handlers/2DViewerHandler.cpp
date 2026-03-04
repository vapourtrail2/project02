#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "AppInterfaces.h"
#include <vtkCommand.h>
#include <vtkPropPicker.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

namespace {
bool IsSliceMode(VizMode mode)
{
    return mode == VizMode::SliceAxial
        || mode == VizMode::SliceCoronal
        || mode == VizMode::SliceSagittal;
}
}

Viewer2DHandler::Viewer2DHandler(AbstractInteractiveService* service, vtkPropPicker* picker, vtkRenderer* renderer)
    : m_service(service),
      m_picker(picker),
      m_renderer(renderer)
{
}

InteractionResult Viewer2DHandler::Handle(const InteractionEvent& eve)
{
    if (!m_service || !IsSliceMode(eve.vizMode)) {
        return {};
    }

    if (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent
        || eve.vtkEventId == vtkCommand::MouseWheelBackwardEvent) {
        const int delta = (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent) ? 1 : -1;
        m_service->UpdateInteraction(delta);
        m_service->MarkDirty();
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonPressEvent) {
        if (eve.shift) {
            m_enableDragCrosshair = true;
            m_service->SetInteracting(true);
            return { true, true };
        }
        return {};
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonReleaseEvent) {
        if (m_enableDragCrosshair) {
            m_enableDragCrosshair = false;
            m_service->SetInteracting(false);
            return { true, false };
        }
        return {};
    }

    if (eve.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (!m_enableDragCrosshair || !m_picker || !m_renderer) {
            return {};
        }

        m_picker->Pick(eve.x, eve.y, 0, m_renderer);
        double* worldPos = m_picker->GetPickPosition();
        if (worldPos) {
            m_service->SyncCursorToWorldPosition(worldPos);
        }
        return { true, true };
    }

    return {};
}
