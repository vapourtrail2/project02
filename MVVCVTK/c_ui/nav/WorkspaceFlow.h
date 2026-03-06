#pragma once

#include <QString>
#include <memory>

class AppController;
struct AppSession;

class ReconstructPage;
class SceneTreePanel;
class RenderPanel;

class WorkspaceFlow
{
public:
    explicit WorkspaceFlow(AppController* controller);

    bool openFile(const QString& path, QString* err);
    std::shared_ptr<AppSession> session() const;
    bool hasData() const;

    bool bindSession(
        ReconstructPage* reconstructPage,
        SceneTreePanel* scenePanel,
        RenderPanel* renderPanel,
        QString* err);
    
	//将打开文件和绑定session合成一个接口，外部调用时只需调用这个接口即可完成打开文件并更新界面
    bool openAndBind(const QString& path, 
        ReconstructPage* reconstructPage,
		SceneTreePanel* scenePanel, 
        RenderPanel* renderPanel, 
        QString* err);

private:
    AppController* controller_ = nullptr;
};