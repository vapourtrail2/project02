#include "c_ui/qt/interaction/handlers/2DViewerHandler.h"
#include "AppInterfaces.h"
#include <QDebug>
#include <vtkCellPicker.h>
#include <vtkCommand.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkViewport.h>
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

    if (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent
        || eve.vtkEventId == vtkCommand::MouseWheelBackwardEvent) {
        const int step = eve.ctrl ? 5 : 1;
        const int delta = (eve.vtkEventId == vtkCommand::MouseWheelForwardEvent) ? step : -step;
        //qDebug().noquote() << "[2D] wheel mode=" << ToModeName(eve.vizMode)
        //                   << " delta=" << delta << " x=" << eve.x << " y=" << eve.y;
        m_service->UpdateInteraction(delta);
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::RightButtonPressEvent && eve.shift) {
        m_enableDragCrosshair = true;
        m_crosshairMoveLogTick = 0;
        //qDebug().noquote() << "[2D] crosshair-press mode=" << ToModeName(eve.vizMode)
        //                   << " x=" << eve.x << " y=" << eve.y << " shift=" << eve.shift;
        m_service->SetInteracting(true);
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::RightButtonReleaseEvent && m_enableDragCrosshair) {
        m_enableDragCrosshair = false;
        //qDebug().noquote() << "[2D] crosshair-release mode=" << ToModeName(eve.vizMode)
        //                   << " x=" << eve.x << " y=" << eve.y;
        m_service->SetInteracting(false);
        return { true, false };
    }

    if (eve.vtkEventId == vtkCommand::RightButtonPressEvent && !eve.shift) {
        m_enableDragWindowLevel = true;
        m_windowLevelMoveLogTick = 0;
        //qDebug().noquote() << "[2D] wl-press mode=" << ToModeName(eve.vizMode)
        //                   << " x=" << eve.x << " y=" << eve.y;
        m_lastDragX = eve.x;
        m_lastDragY = eve.y;
        m_service->SetInteracting(true);
        return { true, true };
    }

    if (eve.vtkEventId == vtkCommand::RightButtonReleaseEvent && m_enableDragWindowLevel) {
        m_enableDragWindowLevel = false;
        //qDebug().noquote() << "[2D] wl-release mode=" << ToModeName(eve.vizMode)
        //                   << " x=" << eve.x << " y=" << eve.y;
        m_service->SetInteracting(false);
        return { true, false };
    }

    if (eve.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (m_enableDragCrosshair) {
            if (!m_renderer) {
                //qDebug().noquote() << "[2D] crosshair-move skipped: renderer=null";
                return {};
            }

            const auto cursorWorld = m_service->GetCursorWorldPosition();
            m_renderer->SetWorldPoint(cursorWorld[0], cursorWorld[1], cursorWorld[2], 1.0);
            m_renderer->WorldToDisplay();
            double* displayPoint = m_renderer->GetDisplayPoint();
            if (!displayPoint) {
                //qDebug().noquote() << "[2D] crosshair-move skipped: displayPoint=null";
                return { true, true };
            }

            m_renderer->SetDisplayPoint(static_cast<double>(eve.x), static_cast<double>(eve.y), displayPoint[2]);
            m_renderer->DisplayToWorld();
            double* worldPoint = m_renderer->GetWorldPoint();
            if (!worldPoint || std::abs(worldPoint[3]) < 1e-6) {
 /*               qDebug().noquote() << "[2D] crosshair-move skipped: invalid worldPoint";*/
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
            if (m_crosshairMoveLogTick <= 5 || (m_crosshairMoveLogTick % 10) == 0) {
                //qDebug().noquote() << "[2D] crosshair-move mode=" << ToModeName(eve.vizMode)
                //                   << " x=" << eve.x << " y=" << eve.y << " axis=" << fixedAxis
                //                   << " world=(" << worldPos[0] << "," << worldPos[1] << "," << worldPos[2] << ")";
            }
            m_service->SyncCursorToWorldPosition(worldPos, fixedAxis);
            return { true, true };
        }

        if (m_enableDragWindowLevel) {
            const int dx = eve.x - m_lastDragX;
            const int dy = eve.y - m_lastDragY;
            m_lastDragX = eve.x;
            m_lastDragY = eve.y;

            ++m_windowLevelMoveLogTick;
            if (m_windowLevelMoveLogTick <= 5 || (m_windowLevelMoveLogTick % 10) == 0) {
                //qDebug().noquote() << "[2D] wl-move mode=" << ToModeName(eve.vizMode)
                //                   << " x=" << eve.x << " y=" << eve.y
                //                   << " dx=" << dx << " dy=" << dy;
            }
            m_service->AdjustWindowLevel(dx * kWWSensitivity, dy * kWCSensitivity);
            return { true, true };
        }

        return {};
    }

    return {};
}
