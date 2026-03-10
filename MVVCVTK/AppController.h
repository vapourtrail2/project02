#pragma once
#include <QObject>
#include <array>
#include <memory>
#include <QString>
#include "DataManager.h"
#include "AppState.h"
#include "VolumeAnalysisService.h"

struct AppSession
{
    std::shared_ptr<AbstractDataManager> dataMgr;
    std::shared_ptr<SharedInteractionState> sharedState;
    std::shared_ptr<VolumeAnalysisService> analysisService;
    QString sourcePath;
};

class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);

    std::shared_ptr<AppSession> session() const { return m_session; }

    bool openFile(const QString& path, QString* errorOut = nullptr);
    bool openReconstructedData(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        const QString& sourcePath,
        QString* errorOut = nullptr);

    void clearSession();

signals:
    void sessionChanged(std::shared_ptr<AppSession> session);

private:
    std::shared_ptr<AbstractDataManager> createDataManagerForPath(const QString& path) const;
    bool finalizeSession(const std::shared_ptr<AppSession>& newSession, QString* errorOut);

    std::shared_ptr<AppSession> m_session;
};

