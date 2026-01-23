#include "uireconstruct3d.h"
#include <array>
#include "c_ui/MainWindow.h"
#include "DataManager.h"
#include "AppState.h"
#include <memory>
#include <qstatusbar.h>
#include "c_ui/workbenches/StartPage.h"

void CTViewer::openCtReconUi()
{
    if (!uiRecon3d_) {
        uiRecon3d_ = new UIReconstruct3D(this);

        QObject::connect(uiRecon3d_, &UIReconstruct3D::reconFinished, this, [this]() {

            float* data = nullptr;
            std::array<float, 3> spacing{}, origin{};
            std::array<int, 3> outSize{};

            uiRecon3d_->getReconData(data, spacing, origin, outSize);

            if (!data) {
                if (auto bar = statusBar()) {
                    bar->showMessage(QStringLiteral("重建返回数据为空"), 3000);
                }
                return;
            }

            //  float* 到 数据管理器 会 memcpy 拷贝进 vtkImageData
            auto rawDataManager = std::make_shared<RawVolumeDataManager>();
            rawDataManager->SetFromBuffer(data, outSize, spacing, origin);

            //  创建共享状态
            auto state = std::make_shared<SharedInteractionState>();
            if (auto img = rawDataManager->GetVtkImage()) {
                double range[2];
                img->GetScalarRange(range);
                state->SetScalarRange(range[0], range[1]);
            }
            
            this->m_currentDataMgr = rawDataManager;
            this->m_currentState = state;

            // 初始化四视图+mount到StartPage的viewerHost
            if (mprViews_) {
                mprViews_->initWithData(this->m_currentDataMgr, this->m_currentState);
                stack_->setCurrentWidget(mprViews_);
                ribbontabBar_->setCurrentIndex(1);
                statusBar()->showMessage(QStringLiteral("重建完成，视图已更新。"), 3000);
            }

            uiRecon3d_->close();
            }, Qt::QueuedConnection);

    }

    uiRecon3d_->show();
    uiRecon3d_->raise();
    uiRecon3d_->activateWindow();
}
