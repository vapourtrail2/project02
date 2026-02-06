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
            *errorOut = QStringLiteral("路径为空，请选择文件。");
        }
        return false;
    }

    //  newSession 到 m_session
    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = std::make_shared<RawVolumeDataManager>();
    newSession->sharedState = std::make_shared<SharedInteractionState>();
    newSession->sourcePath = p;

    const bool ok = newSession->dataMgr->LoadData(p.toStdString());
    if (!ok) {
        if (errorOut) {
            *errorOut = QStringLiteral("加载失败！请确认文件名包含尺寸(如 data_512x512x200.raw)");
        }
        return false;
    }

    // 把标量范围写进 sharedState
    if (auto img = newSession->dataMgr->GetVtkImage()) {
        double range[2];
        img->GetScalarRange(range);
        newSession->sharedState->SetScalarRange(range[0], range[1]);

        // 设置初始光标位置到图像中心 (修复画面为空的问题)
        int dims[3];
        img->GetDimensions(dims);
        newSession->sharedState->SetCursorPosition(dims[0] / 2, dims[1] / 2, dims[2] / 2);

        // 设置一个默认的 ISO 阈值 (防止3D视图为空)
        newSession->sharedState->SetIsoValue(range[0] + (range[1] - range[0]) * 0.2);
    }

    // 分析服务 跟dataMgr同生命周期
    newSession->analysisService = std::make_shared<VolumeAnalysisService>(newSession->dataMgr);

    m_session = std::move(newSession);
    emit sessionChanged(m_session);
    return true;
}

void AppController::clearSession()
{
    m_session.reset();
    emit sessionChanged(nullptr);
}
