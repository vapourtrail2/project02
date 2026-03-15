#include "c_ui/qt/interaction/handlers/3DViewerHandler.h"

#include "AppInterfaces.h"
#include <vtkActor.h>
#include <vtkCommand.h>
#include <vtkPropPicker.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <cmath>
#include <vtkCamera.h>
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
                m_lastMouseX = eve.x;
                m_lastMouseY = eve.y;
                m_dragAccumulator = 0;
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
            m_lastMouseX = -1;
            m_lastMouseY = -1;
            m_dragAccumulator = 0;
            return { true, false };
        }
        return {};
    }

    if (eve.vtkEventId == vtkCommand::MouseMoveEvent) {
        if (!m_isDragging || m_dragAxis == -1 || !m_renderer) {
            return {};
        }

        const int dx = eve.x - m_lastMouseX;
        const int dy = eve.y - m_lastMouseY;
        m_lastMouseX = eve.x;
        m_lastMouseY = eve.y;

		// 计算相机视角 right/up/dir  右，上，目的向量，叉乘关系
		double viewRight[3] = { 0.0, 0.0, 0.0 }, viewUp[3] = { 0.0, 0.0, 0.0 }, viewDir[3] = { 0.0, 0.0, 0.0 };
		vtkCamera* cam = m_renderer->GetActiveCamera();
		cam->GetDirectionOfProjection(viewDir);
		cam->GetViewUp(viewUp);

		// 计算右向量 viewRight = viewDir x viewUp
		vtkMath::Cross(viewDir, viewUp, viewRight);

		// 根据鼠标位移的主轴 m_dragAxis 判断是沿哪个方向移动，映射到相机的 right/up 方向上
		double axisVector[3] = { 0.0, 0.0, 0.0 };
		axisVector[m_dragAxis] = 1.0; // 初始化向量
    
		// 计算鼠标在3d方向移动的投影量，dotR 和 dotU 分别大于0 意味着 鼠标移动的方向与相机的 right 和 up 方向一致，反之则相反
		double dotR = vtkMath::Dot(viewRight,axisVector);
		double dotU = vtkMath::Dot(viewUp, axisVector);

        // 最后根据投影量计算最总增量
		double dragContribution = dx * dotR + dy * dotU;

        //单位转换
        int primaryDelta = (dragContribution >= 0) ?
            (int)(std::sqrt(dx * dx + dy * dy)):
			-(int)(std::sqrt(dx * dx + dy * dy));

        //const int primaryDelta = (std::abs(dx) >= std::abs(dy)) ? -dx : dy;
        m_dragAccumulator += primaryDelta;

        constexpr int kPixelsPerSlice = 4;
        int sliceDelta = 0;
        while (m_dragAccumulator >= kPixelsPerSlice) {
            ++sliceDelta;
            m_dragAccumulator -= kPixelsPerSlice;
        }
        while (m_dragAccumulator <= -kPixelsPerSlice) {
            --sliceDelta;
            m_dragAccumulator += kPixelsPerSlice;
        }

        if (sliceDelta == 0) {
            return { true, true };
        }

        //鼠标位移映射为slice增量
        //if (m_dragAxis == 1 || m_dragAxis == 2 || m_dragAxis == 0 ) {
        //    sliceDelta = -sliceDelta;
        //}


        m_service->UpdateInteractionAxis(m_dragAxis, sliceDelta);
        m_service->PresentFrame();
        return { true, true };
    }

    return {};
}



