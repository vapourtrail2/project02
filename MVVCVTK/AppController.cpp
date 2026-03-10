#include "AppController.h"

#include <QFileInfo>

namespace {
std::shared_ptr<AbstractDataManager> CreateManagerForPath(const QString& path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (info.isDir() || suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff")) {
        return std::make_shared<TiffVolumeDataManager>();
    }
    return std::make_shared<RawVolumeDataManager>();
}
}

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
    newSession->dataMgr = createDataManagerForPath(p);
    newSession->sharedState = std::make_shared<SharedInteractionState>();
    newSession->sharedState->SetLoadState(LoadState::Loading);
    newSession->sourcePath = p;

    const bool ok = newSession->dataMgr && newSession->dataMgr->LoadData(p.toStdString());
    if (!ok) {
        newSession->sharedState->NotifyLoadFailed();
        if (errorOut) {
            *errorOut = QStringLiteral("Load failed. Check whether the file path and input format are valid.");
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
    newSession->sharedState->SetLoadState(LoadState::Loading);
    newSession->sourcePath = sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();

    return finalizeSession(newSession, errorOut);
}

std::shared_ptr<AbstractDataManager> AppController::createDataManagerForPath(const QString& path) const
{
    return CreateManagerForPath(path);
}

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
        newSession->sharedState->NotifyLoadFailed();
        if (errorOut) {
            *errorOut = QStringLiteral("No image data was produced.");
        }
        return false;
    }

    double range[2];
    img->GetScalarRange(range);

    int dims[3];
    img->GetDimensions(dims);

    const double rangeSpan = range[1] - range[0];
    const double safeWindowWidth = rangeSpan > 0.0 ? rangeSpan : 1.0;
    const double windowCenter = range[0] + safeWindowWidth * 0.5;

    newSession->sharedState->SetWindowLevel(safeWindowWidth, windowCenter);
    newSession->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
    newSession->sharedState->SetIsoValue(range[0] + safeWindowWidth * 0.2);
    newSession->sharedState->NotifyDataReady(range[0], range[1]);

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
