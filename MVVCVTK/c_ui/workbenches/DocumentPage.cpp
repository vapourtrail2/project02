#include "DocumentPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QDialog>
#include "AppState.h"

DocumentPage::DocumentPage(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
    wireLeftDockSignals();
}

//构建整页：左右分栏（左：导航，右：主内容）
void DocumentPage::buildUi()
{
    setObjectName(QStringLiteral("pageDocument"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageDocument{background-color:#404040;}"
        "QLabel{color:#f2f2f2;}"));

    auto hl = new QHBoxLayout(this);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    // 左侧Dock区
    buildLeftDock();
	hl->addWidget(listNav_, 0); //第二个参数的意思是伸缩比例，0表示不伸缩

    // 右侧主内容
    auto right = buildRightContent(this);
    hl->addWidget(right, 1);
}

 //构建左侧导航列表
void DocumentPage::buildLeftDock()
{
	listNav_ = new QListWidget(this);//listNav_代表的是左侧的导航列表
    listNav_->setMinimumWidth(180);
    listNav_->setStyleSheet(R"(
        QListWidget { background:#181818; color:#ddd; border:none; }
        QListWidget::item { height:30px; padding-left:8px; }
        QListWidget::item:selected { background:#444; color:#fff; }
    )");

    // 工具函数:分割线
    auto addSeparator = [this]() {
        auto sep = new QListWidgetItem();//占位
        sep->setFlags(Qt::NoItemFlags);//这个项只能用来占位或展示，不允许用户交互
        sep->setSizeHint(QSize(180, 10));
        listNav_->addItem(sep);

        auto line = new QFrame(listNav_);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("background:#444; margin:4px 12px;");
        listNav_->setItemWidget(sep, line);
    };

    auto addItem = [this](const QString& text) {
        auto item = new QListWidgetItem(text);
        item->setSizeHint(QSize(180, 30));
        listNav_->addItem(item);
    };

    //Dock的内容
    addItem(QStringLiteral("欢迎使用"));
    addSeparator();

    addItem(QStringLiteral("新建"));
    addItem(QStringLiteral("打开"));
    addItem(QStringLiteral("保存"));
    addItem(QStringLiteral("另存为"));
    addItem(QStringLiteral("打包"));
    addItem(QStringLiteral("导出为"));
    addSeparator();

    addItem(QStringLiteral("快速导入"));
    addItem(QStringLiteral("CT重建"));
    addItem(QStringLiteral("导入"));
    addItem(QStringLiteral("导出"));
    addSeparator();

    addItem(QStringLiteral("合并对象"));
    addItem(QStringLiteral("保存对象"));
    addItem(QStringLiteral("保存图像"));
    addItem(QStringLiteral("保存影像/图像堆栈"));
	addItem(QStringLiteral("保存报告"));
	addItem(QStringLiteral("批处理"));
	addItem(QStringLiteral("首选项"));
    addSeparator();

    addItem(QStringLiteral("退出"));
}


//  构建右侧主内容
QWidget* DocumentPage::buildRightContent(QWidget* parent)
{
    auto right = new QWidget(parent);
    auto vl = new QVBoxLayout(right);
    vl->setContentsMargins(18, 18, 18, 18);
    vl->setSpacing(16);

    // 顶部横幅
    auto banner = new QFrame(right);
    banner->setObjectName(QStringLiteral("heroBanner"));
    banner->setStyleSheet(QStringLiteral(
        "QFrame#heroBanner{background:#322F30; border-radius:10px;}"
        "QFrame#heroBanner QLabel{color:#f9f9f9;}"));
    auto bannerLayout = new QVBoxLayout(banner);
    bannerLayout->setContentsMargins(20, 16, 20, 16);
    bannerLayout->setSpacing(8);
    auto title = new QLabel(QStringLiteral("欢迎使用 data_viewer_demo"), banner);
    title->setStyleSheet(QStringLiteral("font-size:24px; font-weight:700;"));
    bannerLayout->addWidget(title);
    auto subtitle = new QLabel(QStringLiteral("继续最近项目，或通过下方模块快速开始您的工作流程。"), banner);
    subtitle->setStyleSheet(QStringLiteral("font-size:14px; color:#bbbbbb;"));
    subtitle->setWordWrap(true);
    bannerLayout->addWidget(subtitle);
    vl->addWidget(banner);

    // 操作提示
    auto tipsFrame = new QFrame(right);
    tipsFrame->setObjectName(QStringLiteral("tipsFrame"));
    tipsFrame->setStyleSheet(QStringLiteral(
        "QFrame#tipsFrame{background:#322F30; border-radius:10px;}"
        "QFrame#tipsFrame QLabel{color:#d8d8d8;}"));
    auto tipsLayout = new QVBoxLayout(tipsFrame);
    tipsLayout->setContentsMargins(20, 18, 20, 18);
    tipsLayout->setSpacing(10);
    auto tipsTitle = new QLabel(QStringLiteral("操作提示"), tipsFrame);
    tipsTitle->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600;"));
    tipsLayout->addWidget(tipsTitle);
    auto tips = new QLabel(QStringLiteral(
        "1.可导入 DICOM、TIFF、RAW 等常见工业 CT 数据。\n"
        "2.若需培训资料，可访问帮助中心以获取最新教程。"), tipsFrame);
    tips->setWordWrap(true);
    tips->setStyleSheet(QStringLiteral("font-size:13px; line-height:20px;"));
    tipsLayout->addWidget(tips);
    tipsLayout->addStretch();
    vl->addWidget(tipsFrame);

    // 模块入口按钮
    auto moduleFrame = new QFrame(right);
    moduleFrame->setObjectName(QStringLiteral("moduleFrame"));
    moduleFrame->setStyleSheet(QStringLiteral(
        "QFrame#moduleFrame{background:#322F30; border-radius:10px;}"
        "QFrame#moduleFrame QPushButton{background:#2C2C2C; border-radius:8px; border:1px solid #333;"
        " color:#f5f5f5; font-size:16px; padding:18px 12px;}"
        "QFrame#moduleFrame QPushButton:hover{background:#2C2C2C; border-color:#4d6fff;}"
        "QFrame#moduleFrame QLabel{color:#f5f5f5;}"));
    auto moduleLayout = new QVBoxLayout(moduleFrame);
    moduleLayout->setContentsMargins(20, 18, 20, 18);
    moduleLayout->setSpacing(12);
    auto moduleTitle = new QLabel(QStringLiteral("选择最适合您工作流程的“开始”选项卡"), moduleFrame);
    moduleTitle->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600;"));
    moduleLayout->addWidget(moduleTitle);

    auto grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    btnVisCheck_ = new QPushButton(QStringLiteral("视觉检查"), moduleFrame);
    btnPorosity_ = new QPushButton(QStringLiteral("孔隙度"), moduleFrame);
    btnMetrology_ = new QPushButton(QStringLiteral("计量"), moduleFrame);
    btnMaterial_ = new QPushButton(QStringLiteral("材料"), moduleFrame);
    for (auto* b : { btnVisCheck_.data(), btnPorosity_.data(), btnMetrology_.data(), btnMaterial_.data() })
        b->setMinimumSize(160, 70);
    grid->addWidget(btnVisCheck_, 0, 0);
    grid->addWidget(btnPorosity_, 0, 1);
    grid->addWidget(btnMetrology_, 0, 2);
    grid->addWidget(btnMaterial_, 0, 3);
    moduleLayout->addLayout(grid);
    vl->addWidget(moduleFrame);

    // 最近项目
    auto recentFrame = new QFrame(right);
    recentFrame->setObjectName(QStringLiteral("recentFrame"));
    recentFrame->setStyleSheet(QStringLiteral(
        "QFrame#recentFrame{background:#322F30; border-radius:10px;}"
        "QFrame#recentFrame QLabel{color:#f5f5f5;}"
        "QFrame#recentFrame QHeaderView::section{background:#2c2c2c; color:#f0f0f0; border:0;}"
        "QFrame#recentFrame QTableWidget{background:transparent; border:0; color:#f5f5f5;}"
        "QFrame#recentFrame QTableWidget::item:selected{background-color:#3d65f5;}"));
    auto recentLayout = new QVBoxLayout(recentFrame);
    recentLayout->setContentsMargins(20, 18, 20, 18);
    recentLayout->setSpacing(12);

    auto recentTitle = new QLabel(QStringLiteral("最近项目"), recentFrame);
    recentTitle->setStyleSheet(QStringLiteral("font-size:16px; font-weight:600;"));
    recentLayout->addWidget(recentTitle);

    tableRecent_ = new QTableWidget(0, 3, recentFrame);
    tableRecent_->setHorizontalHeaderLabels({ QStringLiteral("名称"), QStringLiteral("位置"), QStringLiteral("上次打开") });
    tableRecent_->horizontalHeader()->setStretchLastSection(true);
    tableRecent_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    tableRecent_->verticalHeader()->setVisible(false);
    tableRecent_->setShowGrid(false);
    tableRecent_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableRecent_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableRecent_->setAlternatingRowColors(true);
    tableRecent_->setStyleSheet(QStringLiteral(
        "QTableWidget{alternate-background-color:#2C2C2C;}"
        "QTableWidget QTableCornerButton::section{background:#2c2c2c;}"));

    struct RecentItem { QString name; QString path; QString time; };
    const QList<RecentItem> recents = {
        { QStringLiteral("发动机缸体.vgl"), QStringLiteral("D:/Projects/CT/EngineBlock"), QStringLiteral("今天 09:24") },
        { QStringLiteral("齿轮箱.vgl"),     QStringLiteral("D:/Projects/CT/GearBox"),     QStringLiteral("昨天 17:42") },
        { QStringLiteral("叶片扫描.vgi"),   QStringLiteral("E:/Scan/Blade"),              QStringLiteral("2024-05-12") },
        { QStringLiteral("材料试样.raw"),   QStringLiteral("E:/Lab/Materials"),           QStringLiteral("2024-04-28") }
    };
    for (const auto& it : recents) {
        int row = tableRecent_->rowCount();
        tableRecent_->insertRow(row);
        tableRecent_->setItem(row, 0, new QTableWidgetItem(it.name));
        tableRecent_->setItem(row, 1, new QTableWidgetItem(it.path));
        tableRecent_->setItem(row, 2, new QTableWidgetItem(it.time));
    }
    tableRecent_->setMinimumHeight(220);
    recentLayout->addWidget(tableRecent_);
    vl->addWidget(recentFrame);

    vl->addStretch();
    return right;
}

 // 连接左侧栏点击
void DocumentPage::wireLeftDockSignals(){
    if (!listNav_)
        return;
    connect(listNav_, &QListWidget::itemClicked, this,
        [this](QListWidgetItem* it) {
            const QString t = it ? it->text() : QString();
            if (t == QStringLiteral("打开")){
                emit moduleClicked(QStringLiteral("选择：%1").arg(t));
                showOpenDialog();
            }
            else if (t == QStringLiteral("CT重建")
                  || t == QStringLiteral("快速导入")
                  || t == QStringLiteral("导入")
                  || t == QStringLiteral("导出")) {
                emit moduleClicked(QStringLiteral("正在进入：%1").arg(t));
            }
            else if (t == QStringLiteral("退出")) {
                emit moduleClicked(QStringLiteral("准备退出"));
            }
            else {
                emit moduleClicked(QStringLiteral("选择：%1").arg(t));
            }
        });
}

void DocumentPage::showOpenDialog()
{
    if (!docDialog_) {
        docDialog_ = new QDialog(this);
		docDialog_->setFixedSize(300, 110);
        docDialog_->setModal(true);
        docDialog_->setWindowTitle(QStringLiteral("打开"));
       
        auto* dialogLayout = new QVBoxLayout(docDialog_);
        dialogLayout->setContentsMargins(14, 14, 14, 14);
        dialogLayout->setSpacing(12);

        auto* introLabel = new QLabel(QStringLiteral("请选择RAW，然后点击“加载”。"), docDialog_);
        introLabel->setWordWrap(true);
        dialogLayout->addWidget(introLabel);

        // 输入与按钮区域
        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(8);
        auto* dirLabel = new QLabel(QStringLiteral("目录:"), docDialog_);
        inputRow->addWidget(dirLabel);

        inputDicomDirectory_ = new QLineEdit(docDialog_);
        inputDicomDirectory_->setPlaceholderText(QStringLiteral("选择或输入 RAW 文件"));
		inputRow->addWidget(inputDicomDirectory_, 1);

        btnDicomBrowse_ = new QPushButton(QStringLiteral("浏览..."), docDialog_);
        inputRow->addWidget(btnDicomBrowse_);

        dialogLayout->addLayout(inputRow);

        // 状态与动作行
        auto* actionRow = new QHBoxLayout();
        actionRow->setSpacing(8);
        dicomStatusLabel_ = new QLabel(QStringLiteral("尚未加载数据"), docDialog_);
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#d0d0d0;"));
        actionRow->addWidget(dicomStatusLabel_, 1);

        btnDicomLoad_ = new QPushButton(QStringLiteral("加载"), docDialog_);
        btnDicomLoad_->setDefault(true);
        actionRow->addWidget(btnDicomLoad_);
        dialogLayout->addLayout(actionRow);

        connect(btnDicomBrowse_, &QPushButton::clicked, this, [this]() {
            const QString filePath = QFileDialog::getOpenFileName(//读文件
                this,
                QStringLiteral("选择数据文件"),
                "",
                QStringLiteral("Raw Data (*.raw);;All Files (*.*)")
            );

            if (!filePath.isEmpty()) {
                inputDicomDirectory_->setText(filePath); // 显示路径
            }
            });
        // 加载按钮连接
        connect(btnDicomLoad_, &QPushButton::clicked, this, [this]() {
            loadFilePath(inputDicomDirectory_->text().trimmed());
            });
    }

    // 每次展示前重置状态文案
    updateStatusLabel(QStringLiteral("尚未加载数据"), false);
    docDialog_->show();
    docDialog_->raise();
    docDialog_->activateWindow();
}

// 考虑：更改架构 数据生命周期属于某个页面 而不是属于会话(session) 应属于会话
void DocumentPage::loadFilePath(const QString& path)
{
    const QString p = path.trimmed();
    if (p.isEmpty()) {
        updateStatusLabel(QStringLiteral("路径为空，请选择文件。"), true);
        return;
    }

    // 这里只发请求，不做实际 LoadData
    updateStatusLabel(QStringLiteral("正在加载..."), false);
    emit openRequested(p);
}

void DocumentPage::notifySucc()
{
    updateStatusLabel(QStringLiteral("数据加载成功。"), false);
    if (docDialog_) {
		docDialog_->accept();
    }
}

void DocumentPage::notifyFail(const QString& reason)
{
    const QString msg = reason.isEmpty() ? QStringLiteral("加载失败") : reason;
	updateStatusLabel(msg, true);
}

// 统一更新状态提示，带上错误标记
void DocumentPage::updateStatusLabel(const QString& text, bool isError)
{
    if (!dicomStatusLabel_) {
        return;
    }

    dicomStatusLabel_->setText(text);

    // 根据状态选择颜色：错误为红色，成功为绿色，其余保持中性灰色
    if (isError) {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#ff6464;"));
    }
    else if (text.contains(QStringLiteral("成功"))) {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#8ae66a;"));
    }
    else {
        dicomStatusLabel_->setStyleSheet(QStringLiteral("color:#d0d0d0;"));
    }
}