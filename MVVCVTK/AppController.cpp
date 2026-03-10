#include "AppController.h"

AppController::AppController(QObject* parent)
    : QObject(parent)
{
}

bool AppController::openFile(const QString& path, QString* errorOut)
{
    const QString p = path.trimmed();
    if (p.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Empty path.");
        }
        return false;
    }

    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = std::make_shared<RawVolumeDataManager>();
    newSession->sharedState = std::make_shared<SharedInteractionState>();
    newSession->sourcePath = p;

    const bool ok = newSession->dataMgr->LoadData(p.toStdString());
    if (!ok) {
        if (errorOut) {
            *errorOut = QStringLiteral("Load failed. Check whether the file name contains dimensions.");
        }
        return false;
    }

    return finalizeSession(newSession, errorOut);
}

bool AppController::openReconstructedData(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    QString* errorOut)
{
    auto rawDataManager = std::make_shared<RawVolumeDataManager>();
    if (!rawDataManager->SetFromBuffer(data, dims, spacing, origin)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid reconstruction buffer.");
        }
        return false;
    }

    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = rawDataManager;
    newSession->sharedState = std::make_shared<SharedInteractionState>();
    newSession->sourcePath = sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();

    return finalizeSession(newSession, errorOut);
}

//不管重建数据还是从文件加载的数据，最终都要走这个函数来完成后续的状态初始化和信号通知
//公共路径
bool AppController::finalizeSession(const std::shared_ptr<AppSession>& newSession, QString* errorOut)
{
    if (!newSession || !newSession->dataMgr || !newSession->sharedState) {
        if (errorOut) {
            *errorOut = QStringLiteral("Session initialization failed.");
        }
        return false;
    }

    auto img = newSession->dataMgr->GetVtkImage();
    if (!img) {
        if (errorOut) {
            *errorOut = QStringLiteral("No image data was produced.");
        }
        return false;
    }

    double range[2];
    img->GetScalarRange(range);
    newSession->sharedState->SetScalarRange(range[0], range[1]);

    int dims[3];
    img->GetDimensions(dims);
    newSession->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
    newSession->sharedState->SetIsoValue(range[0] + (range[1] - range[0]) * 0.2);

    newSession->analysisService = std::make_shared<VolumeAnalysisService>(newSession->dataMgr);

    m_session = newSession;
    emit sessionChanged(m_session);
    return true;
}

void AppController::clearSession()
{
    m_session.reset();
    emit sessionChanged(nullptr);
}