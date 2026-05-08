#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "core/MVVCVTK/MVVCVTK/AppInterfaces.h"

#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkViewport.h>
#include <vtkCamera.h>
#include <cmath>

namespace {
bool IsSliceMode(VizMode mode)
{
    return mode == VizMode::SliceTop_down
        || mode == VizMode::SliceFront_back
        || mode == VizMode::SliceLeft_right;
}

const char* ToModeName(VizMode mode)
    {
    switch (mode) {
    case VizMode::SliceTop_down: return "Axial";
    case VizMode::SliceFront_back: return "Coronal";
    case VizMode::SliceLeft_right: return "Sagittal";
    default: return "Other";
    }
    }
}

Viewer2DHandler::Viewer2DHandler(AbstractInteractiveService* service, vtkCellPicker* picker, vtkRenderer* renderer)
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

    if (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent//滚轮
        || eve.vtkEventId == vtkCommand::MouseWheelBackwardEvent) {
        const int step = eve.ctrl ? 5 : 1;
        const int delta = (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent) ? step : -step;
        m_service->SetSliceScrolled(delta);
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::RightButtonPressEvent ) {//十字线
        if (eve.shift) {
        m_enableDragCrosshair = true;
        m_crosshairMoveLogTick = 0;
        m_service->SetInteracting(true);
        return { true, true };
        }
        //交给自带交互   2D 放大
        return {};
    }

    if (eve.vtkEventId == vtkCommand::RightButtonReleaseEvent) {
        //统一收口
        if (m_enableDragCrosshair) {
            m_enableDragCrosshair = false;
            m_service->SetInteracting(false);
            return { true,true };
        }
        //normal rightbutton
        return {};
    }
    
    //替换交互
    if (eve.vtkEventId == vtkCommand::LeftButtonPressEvent && !eve.shift) {
        m_enableDragWindowLevel = true;
        m_windowLevelMoveLogTick = 0;

        m_windowLevelStartX = eve.x;
        m_windowLevelStartY = eve.y;

        const auto wl = m_service->GetWindowLevel();
        m_startWindowWidth = wl.windowWidth;
        m_startWindowCenter = wl.windowCenter;

        m_service->SetInteracting(true);
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonReleaseEvent && m_enableDragWindowLevel) {
        m_enableDragWindowLevel = false;
        m_service->SetInteracting(false);
        return { true, false };
    }

    if (eve.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (m_enableDragCrosshair) {
            if (!m_renderer) {
                return {};
            }

            const auto cursorWorld = m_service->GetCursorWorld();//change
            m_renderer->SetWorldPoint(cursorWorld[0], cursorWorld[1], cursorWorld[2], 1.0);
            m_renderer->WorldToDisplay();
            double* displayPoint = m_renderer->GetDisplayPoint();
            if (!displayPoint) {

                return { true, true };
            }

            m_renderer->SetDisplayPoint(static_cast<double>(eve.x), static_cast<double>(eve.y), displayPoint[2]);
            m_renderer->DisplayToWorld();
            double* worldPoint = m_renderer->GetWorldPoint();
            if (!worldPoint || std::abs(worldPoint[3]) < 1e-6) {
                return { true, true };
            }

            double worldPos[3] = {
                worldPoint[0] / worldPoint[3],
                worldPoint[1] / worldPoint[3],
                worldPoint[2] / worldPoint[3]
            };

            int fixedAxis = 2;
            if (eve.vizMode == VizMode::SliceFront_back) {
                fixedAxis = 1;
            }
            else if (eve.vizMode == VizMode::SliceLeft_right) {
                fixedAxis = 0;
            }

            ++m_crosshairMoveLogTick;

            m_service->SetCursorWorldPosition(worldPos, fixedAxis);
            return { true, true };
        }
        
		//调整窗宽窗位
        if (m_enableDragWindowLevel) {
            const int totalDx = eve.x - m_windowLevelStartX;
            const int totalDy = eve.y - m_windowLevelStartY;

            int viewWidth = 1;
            int viewHeight = 1;
            if (m_renderer && m_renderer->GetRenderWindow()) {
                int* size = m_renderer->GetRenderWindow()->GetSize();
                if (size) {
                    viewWidth = std::max(1, size[0]);     
                    viewHeight = std::max(1, size[1]);
                }
            }

            ++m_windowLevelMoveLogTick;

            m_service->SetWindowLevelAdjusted(
                totalDx,
                totalDy,
                viewWidth,
                viewHeight,
                m_startWindowWidth,
                m_startWindowCenter);

            return { true, true };
        }

        return {};
    }

    return {};
}
