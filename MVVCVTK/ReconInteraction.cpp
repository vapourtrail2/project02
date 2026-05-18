#include "uireconstruct3d.h"
#include <array>
#include "c_ui/MainWindow.h"
#include "c_ui/nav/WorkspaceFlow.h"
#include "c_ui/workbenches/StartPage.h"
#include <QProgressDialog>
#include <qstatusbar.h>

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
                if (auto* bar = statusBar()) {
                    bar->showMessage(QStringLiteral("Empty reconstruction result."), 3000);
                }
                return;
            }
            
            //进度条
            if (loadProgressDialog_) {
                loadProgressDialog_->close();
                loadProgressDialog_.clear();
            }

            loadProgressDialog_ = new QProgressDialog(
                QStringLiteral("Loading..."),
                QString(),
                0,
                0,
                this);

            loadProgressDialog_->setWindowTitle(QStringLiteral("Loading"));
            loadProgressDialog_->setWindowModality(Qt::ApplicationModal);
            loadProgressDialog_->setMinimumDuration(0);
            loadProgressDialog_->setAutoClose(false);
            loadProgressDialog_->setAutoReset(false);
            loadProgressDialog_->setCancelButton(nullptr);
            loadProgressDialog_->setRange(0, 0);
            loadProgressDialog_->show();

            QString err;
            const bool ok = workspaceFlow_ && workspaceFlow_->openReconstructedData(
                data,
                outSize,
                spacing,
                origin,
                QStringLiteral("CT reconstruction"),
                &err);

            if (!ok) {
                if (loadProgressDialog_) {
                    loadProgressDialog_->close();
                    loadProgressDialog_.clear();
                }

                if (auto* bar = statusBar()) {
                    bar->showMessage(
                        err.isEmpty() ? QStringLiteral("Failed to open reconstruction session.") : err,
                        3000);
                }
                return;
            }

            if (auto* bar = statusBar()) {
                bar->showMessage(QStringLiteral("Reconstruction completed."), 3000);
            }

            uiRecon3d_->close();
        }, Qt::QueuedConnection);
    }

    uiRecon3d_->show();
    uiRecon3d_->raise();
    uiRecon3d_->activateWindow();
}
