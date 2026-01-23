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
class PerformancePage;
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
    void buildTitleBar();
    void buildCentral();
    void wireSignals();
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
    QPointer<QLabel>  titleLabel_;
    QPointer<QToolButton> btnTitleUndo_;
    QPointer<QToolButton> btnTitleUndo02_;
    QPointer<QToolButton> btnMinimize_;
    QPointer<QToolButton> btnMaximize_;
    QPointer<QToolButton> btnClose_;
	QPointer<QTabBar> ribbontabBar_;//主界面状态栏指针
 
    QPointer<QStackedWidget> stack_;
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
	QPointer<PerformancePage> pagePerformance_;
    QPointer<ReconstructPage> mprViews_;
    /*QPointer<UIReconstruct3D> uiRecon3d_;*/

  
    std::shared_ptr<AbstractDataManager> m_currentDataMgr;
    std::shared_ptr<SharedInteractionState> m_currentState;

	UIReconstruct3D* uiRecon3d_ = nullptr;
	QPointer<AppController> appController_;

    //workspace
    QPointer<QSplitter> workspaceSplit_;       // 水平splitter: 左视口和右面板
    QPointer<QSplitter> rightSplit_;           // 垂直splitter: 场景树与渲染
    QPointer<SceneTreePanel> scenePanel_;
    QPointer<RenderPanel> renderPanel_;

    int ribbonHeight_ = 115;// 功能区高度
    
    
};