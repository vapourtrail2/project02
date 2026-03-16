#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "AppInterfaces.h"
#include <QDebug>
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
    return mode == VizMode::SliceAxial
        || mode == VizMode::SliceCoronal
        || mode == VizMode::SliceSagittal;
}

const char* ToModeName(VizMode mode)
{
    switch (mode) {
    case VizMode::SliceAxial: return "Axial";
    case VizMode::SliceCoronal: return "Coronal";
    case VizMode::SliceSagittal: return "Sagittal";
    default: return "Other";
    }
}

constexpr double kWWSensitivity = 2.0;
constexpr double kWCSensitivity = 2.0;
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
        m_service->UpdateInteraction(delta);
        return { true, true };
    }


    if (eve.vtkEventId == vtkCommand::RightButtonPressEvent ) {//十字线
        if (eve.shift) {
        m_enableDragCrosshair = true;
        m_crosshairMoveLogTick = 0;
        m_service->SetInteracting(true);
        return { true, true };
        }
        else
        {
            m_enableDragZoom = true;
			m_ZoomMoveLog = 0;
			m_lastZoomX = eve.x;
			m_lastZoomY = eve.y;
			m_service->SetInteracting(true);
			return { true, true };
        }
    }

    if (eve.vtkEventId == vtkCommand::RightButtonReleaseEvent) {
        //统一收口
        bool ok = false;
        if (m_enableDragCrosshair) {
            m_enableDragCrosshair = false;
            ok = true;
        }
        if (m_enableDragZoom) {
            m_enableDragZoom = false;
            ok = true;
		}
        if (ok) {
            m_service->SetInteracting(false);
            return { true, false };
        }
    }

    if (eve.vtkEventId == vtkCommand::LeftButtonPressEvent && !eve.shift) {
        m_enableDragWindowLevel = true;
        m_windowLevelMoveLogTick = 0;
        m_lastDragX = eve.x;
        m_lastDragY = eve.y;
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

            const auto cursorWorld = m_service->GetCursorWorldPosition();
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
            if (eve.vizMode == VizMode::SliceCoronal) {
                fixedAxis = 1;
            }
            else if (eve.vizMode == VizMode::SliceSagittal) {
                fixedAxis = 0;
            }

            ++m_crosshairMoveLogTick;

            m_service->SyncCursorToWorldPosition(worldPos, fixedAxis);
            return { true, true };
        }

        if (m_enableDragZoom) {
            if (!m_renderer) {
                return {};
            }

            auto* camera = m_renderer->GetActiveCamera();
            if (!camera) {
                return { true , true };
            }
            
			camera->ParallelProjectionOn();//确保是正交投影

            const int dy = eve.y - m_lastZoomY;//增量
			m_lastZoomX = eve.x;
            m_lastZoomY = eve.y;

			++m_ZoomMoveLog;
			double whatsfuckingmean = camera->GetParallelScale(); // 调整缩放灵敏度
			whatsfuckingmean *= (1.0 + dy * 0.01);//whatsfuckingmean越大，缩放越慢，反之越快

            if (whatsfuckingmean < 0.1) {
				whatsfuckingmean = 0.1;
            }
            if (whatsfuckingmean > 99999.0) {
				whatsfuckingmean = 99999.0;
            }

            camera->SetParallelScale(whatsfuckingmean);
            m_renderer->ResetCameraClippingRange();

            return { true, true };
        }


        if (m_enableDragWindowLevel) {
            const int dx = eve.x - m_lastDragX;
            const int dy = eve.y - m_lastDragY;
            m_lastDragX = eve.x;
            m_lastDragY = eve.y;

            ++m_windowLevelMoveLogTick;

            m_service->AdjustWindowLevel(dx * kWWSensitivity, dy * kWCSensitivity);
            return { true, true };
        }

        return {};
    }

    return {};
}
