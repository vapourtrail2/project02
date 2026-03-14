#include "AppController.h"
#include  "AppService.h"
#include <QFileInfo>
#include <QDebug>
#include <qapplication.h>
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
	newSession->service = std::make_shared<MedicalVizService>(newSession->dataMgr, newSession->sharedState);
    newSession->sourcePath = p;
	newSession->analysisService = std::make_shared<VolumeAnalysisService>(newSession->dataMgr);
    
	auto weakSession = std::weak_ptr<AppSession>(newSession);
    newSession->service->LoadFileAsync(p.toStdString(), 
        [weakSession](bool ok)
        {
            // 所以这里的设值是没有任何影响的，不影响业务，我已经设了初始值，可以删去
            //// 在后台线程，只允许操作 SharedState
            //// 这里的设值可以抽离，或者删去，放在这里只是做验证
            //auto sess = weakSession.lock();
            //if (!sess || !ok) return;

            //auto img = sess->dataMgr->GetVtkImage();
            //if (!img) return;

            //// 拆解findingsetup的逻辑，直接在这里设置初始窗口/层级和iso值，避免重复调用notifyDataReady导致界面刷新两次
            //double range[2];
            //img->GetScalarRange(range);
            //int dims[3];
            //img->GetDimensions(dims);
            //const double rangeSpan = range[1] - range[0];
            //const double safeWindowWidth = rangeSpan > 0.0 ? rangeSpan : 1.0;
            //const double windowCenter = range[0] + safeWindowWidth * 0.5;

            //sess->sharedState->SetWindowLevel(safeWindowWidth, windowCenter);
            //sess->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
            //sess->sharedState->SetIsoValue(range[0] + safeWindowWidth * 0.2);
            //sess->sharedState->NotifyDataReady(range[0], range[1]);
        });
    m_session = newSession;
    emit sessionChanged(m_session);
	return true;
}

bool AppController::openReconstructedData(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    QString* errorOut)
{
	// 目前重建数据仅支持 RawVolumeDataManager，后续可根据需要扩展更多类型的 DataManager
    auto rawDataManager = std::make_shared<RawVolumeDataManager>();
    auto sharedState = std::make_shared<SharedInteractionState>();
	auto service = std::make_shared<MedicalVizService>(rawDataManager, sharedState);
    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = rawDataManager;
    newSession->sharedState = sharedState;
    newSession->service = service;
    newSession->sourcePath = sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();
	newSession->analysisService = std::make_shared<VolumeAnalysisService>(rawDataManager);
    
	// 弱引用，用于托管回调中的 this 指针，避免循环引用导致内存泄漏
	auto weakSession = std::weak_ptr<AppSession>(newSession);
    const bool queen = newSession->service->SetFromBufferAsync(data, dims, spacing, origin, [weakSession]
    (bool ok) 
        {
            // 同上
			auto sess = weakSession.lock();
            if (!sess || !ok) return;

			auto img = sess->dataMgr->GetVtkImage();
            if (!img) return;

			// 拆解findingsetup的逻辑，直接在这里设置初始窗口/层级和iso值，避免重复调用notifyDataReady导致界面刷新两次
			double range[2];
            img->GetScalarRange(range);
            int dims[3];
            img->GetDimensions(dims);
            const double rangeSpan = range[1] - range[0];
            const double safeWindowWidth = rangeSpan > 0.0 ? rangeSpan : 1.0;
            const double windowCenter = range[0] + safeWindowWidth * 0.5;
            
            sess->sharedState->SetWindowLevel(safeWindowWidth, windowCenter);
            sess->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
            sess->sharedState->SetIsoValue(range[0] + safeWindowWidth * 0.2);
			sess->sharedState->NotifyDataReady(range[0], range[1]);
        });

    if (!queen)
    {
        if (errorOut) *errorOut = QStringLiteral("Service is busy or initialization failed."); 
		return false;
    }
	m_session = newSession;
	emit sessionChanged(m_session);
    return true;
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

    //double range[2];
    //img->GetScalarRange(range);


    //int dims[3];
    //img->GetDimensions(dims);
    //const double rangeSpan = range[1] - range[0];
    //const double safeWindowWidth = rangeSpan > 0.0 ? rangeSpan : 1.0;
    //const double windowCenter = range[0] + safeWindowWidth * 0.5;

    //newSession->sharedState->SetWindowLevel(safeWindowWidth, windowCenter);
    //newSession->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);
    //newSession->sharedState->SetIsoValue(range[0] + safeWindowWidth * 0.2);
    //newSession->sharedState->NotifyDataReady(range[0], range[1]);

    //newSession->analysisService = std::make_shared<VolumeAnalysisService>(newSession->dataMgr);

    m_session = newSession;
    emit sessionChanged(m_session);
    return true;
}

void AppController::clearSession()
{
    m_session.reset();
    emit sessionChanged(nullptr);
}
