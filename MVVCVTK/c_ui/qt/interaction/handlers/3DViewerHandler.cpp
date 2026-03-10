#include "c_ui/qt/interaction/handlers/3DViewerHandler.h"

#include "AppInterfaces.h"

#include <vtkActor.h>
#include <vtkCommand.h>
#include <vtkPropPicker.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

namespace {

bool IsCompositeMode(VizMode mode)
{
    return mode == VizMode::CompositeVolume
        || mode == VizMode::CompositeIsoSurface;
}

} // namespace

Viewer3DHandler::Viewer3DHandler(AbstractInteractiveService* service, vtkPropPicker* picker, vtkRenderer* renderer)
    : m_service(service),
      m_picker(picker),
      m_renderer(renderer)
{
}

InteractionResult Viewer3DHandler::Handle(const InteractionEvent& eve)
{
    if (!m_service || !IsCompositeMode(eve.vizMode)) {
        return {};
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonPressEvent) {
        if (!m_picker || !m_renderer) {
            return {};
        }

        if (m_picker->Pick(eve.x, eve.y, 0, m_renderer)) {
            vtkActor* actor = m_picker->GetActor();
            const int axis = m_service->GetPlaneAxis(actor);
            if (axis != -1) {
                m_isDragging = true;
                m_dragAxis = axis;
                m_service->SetInteracting(true);
                if (vtkRenderWindow* rw = m_renderer->GetRenderWindow()) {
                    rw->SetDesiredUpdateRate(15.0);
                }
                return { true, true };
            }
        }
        return {};
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonReleaseEvent) {
        if (m_isDragging) {
            if (vtkRenderWindow* rw = m_renderer->GetRenderWindow()) {
                rw->SetDesiredUpdateRate(0.001);
            }
            m_service->SetInteracting(false);
            m_service->MarkDirty();
            m_isDragging = false;
            m_dragAxis = -1;
            return { true, false };
        }
        return {};
    }

    if (eve.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (!m_isDragging || m_dragAxis == -1 || !m_picker || !m_renderer) {
            return {};
        }

        m_picker->Pick(eve.x, eve.y, 0, m_renderer);
        double* worldPos = m_picker->GetPickPosition();
        if (worldPos) {
            m_service->SyncCursorToWorldPosition(worldPos, m_dragAxis);
        }
        return { true, true };
    }

    return {};
}
