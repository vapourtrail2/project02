#include "c_ui/MainWindow.h"
#include "c_ui/workbenches/DocumentPage.h"
#include "c_ui/workbenches/StartPage.h"
#include "c_ui/workbenches/EditPage.h"
#include "c_ui/workbenches/VolumePage.h"
#include "c_ui/workbenches/SelectPage.h"
#include "c_ui/workbenches/AlignmentPage.h"
#include "c_ui/workbenches/GeometryPage.h"
#include "c_ui/workbenches/MeasurePage.h"
#include "c_ui/workbenches/CADAndThen.h"
#include "c_ui/workbenches/AnalysisPage.h"
#include "c_ui/workbenches/ReportPage.h"
#include "c_ui/workbenches/WindowPage.h"
#include "c_ui/workbenches/AnimationPage.h"
#include "c_ui/workbenches/PerformancePage.h"
#include "c_ui/workbenches/ReconstructPage.h"
#include "AppController.h"
#include "c_ui/panels/RenderPanel.h"
#include "c_ui/panels/SceneTreePanel.h"

#include <QApplication>
#include <QSize>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QStyle>
#include <QStatusBar>
#include <QMouseEvent>
#include <array>
#include <algorithm>
#include <QEvent>
#include <QStringList>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>  
#include <QWidget>      
#include <QSizePolicy>  
#include <QRect>        

#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkAutoInit.h>

VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);

// 构造函数
CTViewer::CTViewer(QWidget* parent)
    : QMainWindow(parent)
{
    //无边框窗口+深色主题 
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowTitle(QStringLiteral("data_viewer_demo"));
    setStyleSheet(QStringLiteral(
        "QMainWindow{background-color:#121212;}"
        "QMenuBar, QStatusBar{background-color:#1a1a1a; color:#e0e0e0;}"));

    //结构 
    buildTheTop();
    buildTheMiddle();  
    wireConnect();     
    setDefaults();      
}

CTViewer::~CTViewer() {
    if (mprViews_ && !mprViews_->parent()) {
        delete mprViews_;
    }
}

void CTViewer::buildTheTop()
{
	//第一行  包含 撤回 前进 标题 最小化 最大化 关闭
    auto* topBarContainer = new QWidget(this);
    auto* topBarLayout = new QVBoxLayout(topBarContainer);
    topBarLayout->setContentsMargins(0, 0, 0, 0);//这句话的作用是去掉边框
    topBarLayout->setSpacing(0);

    topBarContainer->setAttribute(Qt::WA_StyledBackground, true);
    topBarContainer->setStyleSheet(QStringLiteral("QWidget{background-color:#202020;}"));

    buildTitleBar(topBarContainer, topBarLayout);
    buildRibbonTitleBar(topBarContainer, topBarLayout);

	setMenuWidget(topBarContainer);
}

void CTViewer::buildTheMiddle()
{
    auto totalContainer = new QWidget(this);//totalContainer是总体容器
    auto v = new QVBoxLayout(totalContainer);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    buildRibbonStack(totalContainer, v);
    buildContentStack(totalContainer, v);

    setCentralWidget(totalContainer);
    appController_ = new AppController(this);//这行的作用存疑

    applyInitialUiState();
}

void CTViewer::wireConnect() {
    connectTabSignals();
    connectDocumentSignals();
    connectReconSignals();
    connectWindowButtonSignals();
}

void CTViewer::buildTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout) 
{
    titleBar_ = new QWidget(topBarContainer);
    titleBar_->setAttribute(Qt::WA_StyledBackground, true);
    titleBar_->setObjectName(QStringLiteral("customTitleBar"));
    titleBar_->setFixedHeight(38);
    titleBar_->setStyleSheet(QStringLiteral(
        "QWidget#customTitleBar{background-color:#202020;}"
        "QToolButton{background:transparent; border:none; color:#f5f5f5; padding:6px;}"
        "QToolButton:hover{background-color:rgba(255,255,255,0.12);}"
        "QLabel#titleLabel{color:#f5f5f5; font-size:14px; font-weight:600;}"));

    auto* barLayout = new QHBoxLayout(titleBar_);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(0);

    // 左侧撤回/前进按钮
    titleLeftArea_ = new QWidget(titleBar_);
    auto* leftLayout = new QHBoxLayout(titleLeftArea_);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    btnTitleUndo_ = new QToolButton(titleLeftArea_);
    btnTitleUndo_->setToolTip(QStringLiteral("撤回"));
    btnTitleUndo_->setCursor(Qt::PointingHandCursor);
    btnTitleUndo_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    btnTitleUndo_->setAutoRaise(true);
    leftLayout->addWidget(btnTitleUndo_);

    btnTitleUndo02_ = new QToolButton(titleLeftArea_);
    btnTitleUndo02_->setToolTip(QStringLiteral("前进"));
    btnTitleUndo02_->setCursor(Qt::PointingHandCursor);
    btnTitleUndo02_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    btnTitleUndo02_->setAutoRaise(true);
    leftLayout->addWidget(btnTitleUndo02_);

    titleLeftArea_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    titleLeftArea_->installEventFilter(this);
    barLayout->addWidget(titleLeftArea_, 0);

    //  中间标题 
    titleCenterArea_ = new QWidget(titleBar_);
    auto* centerLayout = new QHBoxLayout(titleCenterArea_);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    titleLabel_ = new QLabel(windowTitle(), titleCenterArea_);
    titleLabel_->setObjectName(QStringLiteral("titleLabel"));
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    centerLayout->addWidget(titleLabel_);
    titleCenterArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleCenterArea_->installEventFilter(this);
    titleLabel_->installEventFilter(this);
    barLayout->addWidget(titleCenterArea_, 1);

    // 右侧控制按钮 
    auto* rightContainer = new QWidget(titleBar_);
    auto* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    const QString titleButtonStyle = QStringLiteral(
        "QToolButton { background:transparent; border:none; padding:6px; color:#f5f5f5; border-radius:4px; }"
        "QToolButton:hover { background-color:rgba(255,255,255,0.12); }"
        "QToolButton:pressed { background-color:rgba(255,255,255,0.20); }");

    auto makeBtn = [&](QPointer<QToolButton>& btn, const QString& text, const QString& tip) {
        btn = new QToolButton(rightContainer);
        btn->setToolTip(tip);
        btn->setText(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(32, 32);
        btn->setStyleSheet(titleButtonStyle);
        rightLayout->addWidget(btn);
        };

    makeBtn(btnMinimize_, QStringLiteral("—"), QStringLiteral("最小化"));
    makeBtn(btnMaximize_, QString(QChar(0x2750)), QStringLiteral("最大化"));
    makeBtn(btnClose_, QStringLiteral("×"), QStringLiteral("关闭"));

    QFont f = btnMaximize_->font();
    f.setFamily(QStringLiteral("Segoe UI Symbol"));
    btnMaximize_->setFont(f);

    barLayout->addWidget(rightContainer, 0);

    //拖动窗口
	titleBar_->installEventFilter(this);
    topBarLayout->addWidget(titleBar_);
    
	updateMaximizeButtonIcon();//这个函数的作用是根据当前窗口状态（最大化或正常）来更新最大化按钮的图标，使其在不同状态下显示不同的图标，以提供更好的用户体验。
}

void CTViewer::buildRibbonTitleBar(QWidget* topBarContainer, QVBoxLayout* topBarLayout) {
    //第二行  选项卡栏
    tabBar_ = new QTabBar(topBarContainer);
    tabBar_->setObjectName(QStringLiteral("mainRibbonTabBar"));
    tabBar_->setDrawBase(false);
    tabBar_->setExpanding(false);
    tabBar_->setMovable(false);
    tabBar_->setAttribute(Qt::WA_StyledBackground, true); //
    tabBar_->setStyleSheet(QStringLiteral(
        "QTabBar#mainRibbonTabBar{background-color:#202020; color:#f5f5f5;}"
        "QTabBar#mainRibbonTabBar::tab{padding:8px 16px; margin:0px; border:none; background-color:#202020;}"
        "QTabBar#mainRibbonTabBar::tab:selected{background-color:#333333;}"
        "QTabBar#mainRibbonTabBar::tab:hover{background-color:#2a2a2a;}"));

    //填充标签名称
    const QStringList tabNames = {
           QStringLiteral("文件"),
           QStringLiteral("开始"),
           QStringLiteral("编辑"),
           QStringLiteral("体积"),
           QStringLiteral("选择"),
           QStringLiteral("对齐"),
           QStringLiteral("几何"),
           QStringLiteral("测量"),
           QStringLiteral("CAD/表面测量"),
           QStringLiteral("分析"),
           QStringLiteral("报告"),
           QStringLiteral("动画"),
           QStringLiteral("窗口"),
           QStringLiteral("量具"),
    };

    for (auto name : tabNames) {
        tabBar_->addTab(name);
    }

    tabBar_->setCurrentIndex(0);//把下标设置成0 默认选中第一个标签页
    topBarLayout->addWidget(tabBar_);
}

void CTViewer::connectWindowButtonSignals() {
    //连接最小最大关闭的按钮 
    connect(btnMinimize_, &QToolButton::clicked, this, &CTViewer::showMinimized);
    connect(btnMaximize_, &QToolButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        }
        else {
            showMaximized();
        }
        updateMaximizeButtonIcon();//点一下就要变换图标
        });
    connect(btnClose_, &QToolButton::clicked, this, &CTViewer::close);
}



//start edit volune ...
void CTViewer::buildRibbonStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {
    stack_ = new QStackedWidget(totalContainer);
    stack_->setFixedHeight(iconHeight_);
    rootLayout->addWidget(stack_, 0);

    whatEmpty_ = new QWidget(stack_);//这句话的详细意思是：创建一个新的 QWidget 对象，并将其父对象设置为 stack_。这意味着 whatEmpty_ 将成为 stack_ 的子组件。当你将 whatEmpty_ 添加到 stack_ 中时，它会被管理在 stack_ 的堆叠布局中，stack_ 会负责显示哪个子组件（页面）是当前可见的。
    stack_->addWidget(whatEmpty_);

    //添加多个页面
    pageStart_ = new StartPagePage(stack_);
    stack_->addWidget(pageStart_);
    pageEdit_ = new EditPage(stack_);
    stack_->addWidget(pageEdit_);
    pageVolume_ = new VolumePage(stack_);
    stack_->addWidget(pageVolume_);
    pageSelect_ = new SelectPage(stack_);
    stack_->addWidget(pageSelect_);
    pageAlignment_ = new AlignmentPage(stack_);
    stack_->addWidget(pageAlignment_);
    pageGeometry_ = new GeometryPage(stack_);
    stack_->addWidget(pageGeometry_);
    pageMeasure_ = new MeasurePage(stack_);
    stack_->addWidget(pageMeasure_);
    pageCAD_ = new CADAndThen(stack_);
    stack_->addWidget(pageCAD_);
    pageAnalysis_ = new AnalysisPage(stack_);
    stack_->addWidget(pageAnalysis_);
    pageWindow_ = new WindowPage(stack_);
    stack_->addWidget(pageWindow_);
    pageReport_ = new ReportPage(stack_);
    stack_->addWidget(pageReport_);
    pageAnimation_ = new AnimationPage(stack_);
    stack_->addWidget(pageAnimation_);
}

void CTViewer::buildContentStack(QWidget* totalContainer, QVBoxLayout* rootLayout) {
    secondstack_ = new QStackedWidget(totalContainer);
    rootLayout->addWidget(secondstack_, 1);

    pageDocument_ = new DocumentPage(secondstack_);
    secondstack_->addWidget(pageDocument_);

	buildWorkspacePage();
	buildEmptyPage();
}

void CTViewer::buildWorkspacePage() {
    workspacePage_ = new QWidget(secondstack_);
    auto workspaceContainerLayout = new QVBoxLayout(workspacePage_);
    workspaceContainerLayout->setContentsMargins(0, 0, 0, 0);
    workspaceContainerLayout->setSpacing(0);

    workspaceSplit_ = new QSplitter(Qt::Horizontal, workspacePage_);//第一个参数是水平分割
    workspaceSplit_->setObjectName("workspaceSplit");
    workspaceContainerLayout->addWidget(workspaceSplit_, 1);

    //workshop左边
    mprViews_ = new ReconstructPage(workspaceSplit_);

    //右侧
    rightSplit_ = new QSplitter(Qt::Vertical, workspaceSplit_);
    rightSplit_->setObjectName("rightsplit");

    renderPanel_ = new RenderPanel(rightSplit_);
    rightSplit_->addWidget(renderPanel_);

    scenePanel_ = new SceneTreePanel(rightSplit_);
    rightSplit_->addWidget(scenePanel_);

    //安装
    workspaceSplit_->addWidget(mprViews_);
    workspaceSplit_->addWidget(rightSplit_);
    workspaceSplit_->setStretchFactor(0, 120); // 左侧视口占5份
    workspaceSplit_->setStretchFactor(1, 2); // 右侧面板占2份

    rightSplit_->setStretchFactor(0, 3); // 渲染区域占3份
    rightSplit_->setStretchFactor(1, 2); // 场景树占2份

    secondstack_->addWidget(workspacePage_);
}

void CTViewer::buildEmptyPage() {
    //  Empty 页：无数据提示
    emptyPage_ = new QWidget(secondstack_);
    emptyPage_->setStyleSheet(QStringLiteral("background-color:#000000;"));

    auto ev = new QVBoxLayout(emptyPage_);
    ev->setContentsMargins(0, 0, 0, 0);
    ev->setSpacing(0);

    auto tip = new QLabel(QStringLiteral("请先在“文件”中加载数据"), emptyPage_);
    tip->setAlignment(Qt::AlignCenter);
    tip->setStyleSheet(QStringLiteral("color:#808080;"));
    ev->addWidget(tip, 1);
    secondstack_->addWidget(emptyPage_);
}

void CTViewer::applyInitialUiState() {
    if (stack_) {
        stack_->setCurrentWidget(whatEmpty_);
        // 文件页启动时隐藏顶部图标栏，并清零高度避免首帧占位
        stack_->setFixedHeight(0);  
        stack_->setVisible(false);
    }
    if (secondstack_ && pageDocument_) {
        secondstack_->setCurrentWidget(pageDocument_);
    }
}



void CTViewer::connectTabSignals() {
    connect(tabBar_, &QTabBar::currentChanged, this, &CTViewer::onTabChanged);
}

void CTViewer::connectDocumentSignals() {
    connect(pageDocument_, &DocumentPage::moduleClicked, this, [this](const QString& msg) {
        statusBar()->showMessage(msg, 1500);
        });

    connect(pageDocument_, &DocumentPage::recentOpenRequested, this, [this](const QString& name) {
        statusBar()->showMessage(QStringLiteral("正在打开 %1 ...").arg(name), 1500);
        });

    connect(pageDocument_, &DocumentPage::openRequested, this, &CTViewer::onOpenRequested);
}

void CTViewer::connectReconSignals() {
    connect(pageStart_, &StartPagePage::ctReconRequested, this, [this]() {
        openCtReconUi();
        });
}

void CTViewer::onTabChanged(int index) {
    if (!stack_ || !secondstack_) return;

    auto session = appController_ ? appController_->session() : nullptr;
    const bool hasData = (session && session->dataMgr && session->sharedState)
        || (m_currentDataMgr && m_currentState);

    if (index == 0) {
        if (stack_) {
            stack_->setFixedHeight(0);
            stack_->setVisible(false);
        }
        if (whatEmpty_) {
            stack_->setCurrentWidget(whatEmpty_);
        }
        if (pageDocument_) {
            secondstack_->setCurrentWidget(pageDocument_);
        }
        return;
    }

    if (stack_) {
        stack_->setFixedHeight(iconHeight_);
        stack_->setVisible(true);
    }

    if (hasData) {
        secondstack_->setCurrentWidget(workspacePage_);
    }
    else {
        secondstack_->setCurrentWidget(emptyPage_);
    }

    if (index == 1 && pageStart_) stack_->setCurrentWidget(pageStart_);
    else if (index == 2 && pageEdit_) stack_->setCurrentWidget(pageEdit_);
    else if (index == 3 && pageVolume_) stack_->setCurrentWidget(pageVolume_);
    else if (index == 4 && pageSelect_) stack_->setCurrentWidget(pageSelect_);
    else if (index == 5 && pageAlignment_) stack_->setCurrentWidget(pageAlignment_);
    else if (index == 6 && pageGeometry_) stack_->setCurrentWidget(pageGeometry_);
    else if (index == 7 && pageMeasure_) stack_->setCurrentWidget(pageMeasure_);
    else if (index == 8 && pageCAD_) stack_->setCurrentWidget(pageCAD_);
    else if (index == 9 && pageAnalysis_) stack_->setCurrentWidget(pageAnalysis_);
    else if (index == 10 && pageReport_) stack_->setCurrentWidget(pageReport_);
    else if (index == 11 && pageAnimation_) stack_->setCurrentWidget(pageAnimation_);
    else if (index == 12 && pageWindow_) stack_->setCurrentWidget(pageWindow_);
}

void CTViewer::onOpenRequested(const QString& path) {
    QString err;
    if (!appController_ || !appController_->openFile(path, &err)) {
        statusBar()->showMessage(err.isEmpty() ? QStringLiteral("打开失败") : err, 3000);
        if (pageDocument_) pageDocument_->notifyFail(err);
        return;
    }

    if (!initWorkspaceFromCurrentSession(&err)) {
        statusBar()->showMessage(err.isEmpty()?  QStringLiteral("Session 无效"):err, 3000);
        if (pageDocument_) {
            pageDocument_->notifyFail(err);
        }
        return;
    }
    if (pageDocument_) {
        pageDocument_->notifySucc();
    }
}

bool CTViewer::initWorkspaceFromCurrentSession(QString* errorOut) {
    auto sess = appController_ ? appController_->session() : nullptr;
    if (!sess || !sess->dataMgr || !sess->sharedState) {
        if (errorOut) *errorOut = QStringLiteral("Session 无效");
        return false;
    }
    // 初始化四视图
    if (mprViews_) {
        mprViews_->initWithData(sess->dataMgr, sess->sharedState);
    }
    if (scenePanel_) {
        scenePanel_->setSession(sess->dataMgr, sess->sourcePath);
    }
    if (renderPanel_) {
        renderPanel_->setSession(sess);
    }
    return true;
}

bool CTViewer::eventFilter(QObject* watched, QEvent* event)//实现标题栏拖动
{
    if (!event) return false;
    bool titleArea = (watched == titleBar_.data()
        || watched == titleLeftArea_.data()
        || watched == titleCenterArea_.data()
        || watched == titleLabel_.data());
    if (titleArea) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* e = static_cast<QMouseEvent*>(event);
            if (e->button() == Qt::LeftButton) {
                draggingWindow_ = true;
                dragOffset_ = e->globalPos() - frameGeometry().topLeft();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if (draggingWindow_) {
                auto* e = static_cast<QMouseEvent*>(event);
                move(e->globalPos() - dragOffset_);
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            draggingWindow_ = false;
            break;
        case QEvent::MouseButtonDblClick:
            draggingWindow_ = false;
            if (isMaximized()) showNormal();
            else showMaximized();
            updateMaximizeButtonIcon();
            return true;
        default: break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void CTViewer::updateMaximizeButtonIcon()
{
    if (!btnMaximize_) return;

    // 确保字体里有符号
    QFont f = btnMaximize_->font();
    f.setFamily(QStringLiteral("Segoe UI Symbol"));
    btnMaximize_->setFont(f);

    if (isMaximized()) {
        btnMaximize_->setText(QString(QChar(0x2750)));
        btnMaximize_->setToolTip(QStringLiteral("还原"));
    }
    else {
        btnMaximize_->setText(QStringLiteral("□"));
        btnMaximize_->setToolTip(QStringLiteral("最大化"));
    }
    //btnMaximize_->setText(QStringLiteral("□"));
}

void CTViewer::setDefaults() {
    // 在默认显示时把窗口定位到主屏幕的可用区域左上角，避免每次运行都要手动拖动窗口
    if (auto* screen = QGuiApplication::primaryScreen()) {
        const QRect availableGeometry = screen->availableGeometry();
        move(availableGeometry.topLeft());
    }
}
