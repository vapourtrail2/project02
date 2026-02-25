#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QPoint>
#include <QToolButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QSplitter>
#include <memory>
#include "AppInterfaces.h"
#include "AppState.h"
#include "c_ui/workbenches/ReconstructPage.h"
using namespace std;

class DocumentPage;
class StartPagePage;
class EditPage;
class VolumePage;
class SelectPage;
class AlignmentPage;
class GeometryPage;
class MeasurePage;
class CADAndThen;
class AnalysisPage;
class WindowPage;
class ReportPage;
class AnimationPage;
class ReconstructPage;
class UIReconstruct3D;
class AppController;
class RenderPanel;
class SceneTreePanel;

class CTViewer : public QMainWindow
{
    Q_OBJECT
public:
    explicit CTViewer(QWidget* parent = nullptr);
    ~CTViewer();
    
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildTheTop();
    void buildTheMiddle();
    void wireConnect();
    void setDefaults();
    void updateMaximizeButtonIcon();
    void openCtReconUi();

private:
    //关于标题栏拖动的变量
    bool draggingWindow_ = false;
    QPoint dragOffset_;

    QPointer<QWidget> titleBar_;
    QPointer<QWidget> titleLeftArea_;
    QPointer<QWidget> titleCenterArea_;

    QPointer<QWidget> whatEmpty_; //“文件”Tab对应的空ribbon页（占位）
    QPointer<QWidget> workspacePage_;//contentStack里的workspace容器页
    QPointer<QWidget> emptyPage_;//contentStack里的“无数据提示”页

    QPointer<QLabel>  titleLabel_;
    QPointer<QToolButton> btnTitleUndo_;
    QPointer<QToolButton> btnTitleUndo02_;
    QPointer<QToolButton> btnMinimize_;
    QPointer<QToolButton> btnMaximize_;
    QPointer<QToolButton> btnClose_;
	QPointer<QTabBar> tabBar_;//主界面状态栏指针
 
	QPointer<DocumentPage> pageDocument_;//文档页面的指针
    QPointer<StartPagePage> pageStart_;
	QPointer<EditPage> pageEdit_;
	QPointer<VolumePage> pageVolume_;
	QPointer<SelectPage> pageSelect_;
    QPointer<AlignmentPage> pageAlignment_;
    QPointer<GeometryPage> pageGeometry_;
    QPointer<MeasurePage> pageMeasure_;
    QPointer<CADAndThen> pageCAD_;
	QPointer<AnalysisPage> pageAnalysis_;
    QPointer<WindowPage> pageWindow_;
	QPointer<ReportPage> pageReport_;
	QPointer<AnimationPage> pageAnimation_;
    QPointer<ReconstructPage> mprViews_;
	QPointer<QStackedWidget> stack_;//放开始 编辑 体积 这些工具页 固定高度iconHeight_
    QPointer<QStackedWidget> secondstack_;//占满剩余高度，放 Backstage(DocumentPage)、Workspace(四视图+右侧面板)、Empty(无数据提示)
  
    shared_ptr<AbstractDataManager> m_currentDataMgr;
    shared_ptr<SharedInteractionState> m_currentState;

	UIReconstruct3D* uiRecon3d_ = nullptr;
	QPointer<AppController> appController_;

    //workspace
    QPointer<QSplitter> workspaceSplit_;       // 水平splitter: 左视口和右面板
    QPointer<QSplitter> rightSplit_;           // 垂直splitter: 场景树与渲染
    QPointer<SceneTreePanel> scenePanel_;
    QPointer<RenderPanel> renderPanel_;

    int iconHeight_ = 100;// 图标区高度
    
    
};