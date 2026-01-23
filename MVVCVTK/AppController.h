#pragma once
#include <QObject>
#include <memory>
#include <QString>
#include "DataManager.h"
#include "AppState.h"
#include "AppService.h"  

// 这一份就是“当前会话”的核心对象集合
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

    // 成功则替换 失败则不替换
    bool openFile(const QString& path, QString* errorOut = nullptr);

    void clearSession();

signals:
    void sessionChanged(std::shared_ptr<AppSession> session);

private:
    std::shared_ptr<AppSession> m_session;
};
