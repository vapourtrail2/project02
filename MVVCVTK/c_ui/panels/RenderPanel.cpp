#include "RenderPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QResizeEvent>

#include <cmath>

#include "AppController.h" // AppSession
#include "AppService.h"    // VolumeAnalysisService

static inline double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static inline bool HasFlag(UpdateFlags flags, UpdateFlags f)
{
    return (static_cast<int>(flags) & static_cast<int>(f)) != 0;
}

RenderPanel::RenderPanel(QWidget* parent)
    : QWidget(parent)
{
    auto v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(8);

    // 1) 直方图
    auto histGroup = new QGroupBox(QStringLiteral("直方图"), this);
    histGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto hv = new QVBoxLayout(histGroup);

    histLabel_ = new QLabel(QStringLiteral("（未加载）"), histGroup);
    histLabel_->setMinimumHeight(140);
    histLabel_->setAlignment(Qt::AlignCenter);
    histLabel_->setStyleSheet("QLabel{background:#111; border:1px solid #444; color:#888;}");
    hv->addWidget(histLabel_);

    v->addWidget(histGroup);

    // 2) ISO
    auto isoGroup = new QGroupBox(QStringLiteral("阈值 / ISO"), this);
    isoGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto iv = new QVBoxLayout(isoGroup);

    isoValueLabel_ = new QLabel("ISO: -", isoGroup);
    isoSlider_ = new QSlider(Qt::Horizontal, isoGroup);
    isoSlider_->setRange(0, 1000);
    iv->addWidget(isoValueLabel_);
    iv->addWidget(isoSlider_);
    v->addWidget(isoGroup);

    // 3) 材质
    auto matGroup = new QGroupBox(QStringLiteral("光照/材质"), this);
    matGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto mv = new QVBoxLayout(matGroup);

    auto makeSlider = [&](const QString& text, QSlider*& out) {
        auto row = new QHBoxLayout();
        auto lbl = new QLabel(text, matGroup);
        lbl->setFixedWidth(60);
        out = new QSlider(Qt::Horizontal, matGroup);
        out->setRange(0, 100);
        row->addWidget(lbl);
        row->addWidget(out, 1);
        mv->addLayout(row);
        };

    makeSlider("Ambient", ambient_);
    makeSlider("Diffuse", diffuse_);
    makeSlider("Specular", specular_);
    makeSlider("Power", power_);

    shadeOn_ = new QCheckBox(QStringLiteral("阴影"), matGroup);
    mv->addWidget(shadeOn_);

    v->addWidget(matGroup);
    v->addStretch();

    // -------------------------
    // UI -> State
    // -------------------------
    connect(isoSlider_, &QSlider::valueChanged, this, [this](int v) {
        if (!state_ || updatingUi_) return;

        const double t = static_cast<double>(v) / 1000.0;
        const double iso = rangeMin_ + (rangeMax_ - rangeMin_) * t;

        state_->SetIsoValue(iso);
        isoValueLabel_->setText(QString("ISO: %1").arg(iso, 0, 'f', 2));
        });

    auto onMatChanged = [this]() {
        if (!state_ || updatingUi_) return;

        auto mat = state_->GetMaterial();
        mat.ambient = ambient_->value() / 100.0;
        mat.diffuse = diffuse_->value() / 100.0;
        mat.specular = specular_->value() / 100.0;
        // power 取 1~100
        mat.specularPower = 1.0 + (power_->value() / 100.0) * 99.0;
        mat.shadeOn = shadeOn_->isChecked();

        state_->SetMaterial(mat);
        };

    connect(ambient_, &QSlider::valueChanged, this, [=](int) { onMatChanged(); });
    connect(diffuse_, &QSlider::valueChanged, this, [=](int) { onMatChanged(); });
    connect(specular_, &QSlider::valueChanged, this, [=](int) { onMatChanged(); });
    connect(power_, &QSlider::valueChanged, this, [=](int) { onMatChanged(); });
    connect(shadeOn_, &QCheckBox::toggled, this, [=](bool) { onMatChanged(); });
}

RenderPanel::~RenderPanel()
{
    // 关键：让观察者凭证失效，state 下次通知会自动清掉 expired 的 observer
    lifeToken_.reset();
}

void RenderPanel::setSession(const std::shared_ptr<AppSession>& session)
{
    if (!session) {
        setAnalysisService(nullptr);
        setSharedState(nullptr);
        return;
    }
    setAnalysisService(session->analysisService);
    setSharedState(session->sharedState);
}

void RenderPanel::setAnalysisService(const std::shared_ptr<VolumeAnalysisService>& analysis)
{
    analysis_ = analysis;
    /*refreshHistogram();*/
}

void RenderPanel::setSharedState(const std::shared_ptr<SharedInteractionState>& state)
{
    state_ = state;

    // 每次重新绑定 state，都重新生成 token
    lifeToken_ = std::make_shared<int>(1);

    if (!state_) {
        updatingUi_ = true;
        isoValueLabel_->setText("ISO: -");
        isoSlider_->setValue(0);
        ambient_->setValue(0);
        diffuse_->setValue(0);
        specular_->setValue(0);
        power_->setValue(0);
        shadeOn_->setChecked(false);
        updatingUi_ = false;

        histPixmap_ = QPixmap();
        histLabel_->setPixmap(QPixmap());
        histLabel_->setText(QStringLiteral("（未加载）"));
        return;
    }

    // 从 state 读 scalar range（用于 slider 映射）
    const double* r = state_->GetDataRange();
    rangeMin_ = r[0];
    rangeMax_ = r[1];

    // state -> UI：注册观察者（owner 是 shared_ptr<void>，内部存 weak_ptr）
    // 你的项目就是这么设计的：AddObserver(owner, cb) :contentReference[oaicite:2]{index=2}
    state_->AddObserver(lifeToken_, [this](UpdateFlags flags) {
        // 注意：这里通常就在 UI 线程触发；如果你未来从非 UI 线程通知，再加 invokeMethod/QueuedConnection
        syncFromState(flags);
        });

    // 首次全量同步
    syncFromState(UpdateFlags::All);
    /*refreshHistogram();*/
}

void RenderPanel::syncFromState(UpdateFlags flags)
{
    if (!state_) return;

    updatingUi_ = true;

    // 1) ISO
    if (HasFlag(flags, UpdateFlags::IsoValue) || flags == UpdateFlags::All) {
        const double iso = state_->GetIsoValue();
        double t = 0.0;
        if (std::abs(rangeMax_ - rangeMin_) > 1e-12) {
            t = (iso - rangeMin_) / (rangeMax_ - rangeMin_);
        }
        t = clamp01(t);
        isoSlider_->setValue(static_cast<int>(std::round(t * 1000.0)));
        isoValueLabel_->setText(QString("ISO: %1").arg(iso, 0, 'f', 2));
    }

    // 2) Material
    if (HasFlag(flags, UpdateFlags::Material) || flags == UpdateFlags::All) {
        const auto& mat = state_->GetMaterial();

        auto to01_100 = [](double v01) {
            int v = static_cast<int>(std::round(clamp01(v01) * 100.0));
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            return v;
            };

        auto powerToSlider = [](double p) {
            if (p < 1.0) p = 1.0;
            if (p > 100.0) p = 100.0;
            const double t = (p - 1.0) / 99.0;
            return static_cast<int>(std::round(t * 100.0));
            };

        ambient_->setValue(to01_100(mat.ambient));
        diffuse_->setValue(to01_100(mat.diffuse));
        specular_->setValue(to01_100(mat.specular));
        power_->setValue(powerToSlider(mat.specularPower));
        shadeOn_->setChecked(mat.shadeOn);
    }

    updatingUi_ = false;
}

//void RenderPanel::refreshHistogram()
//{
//    if (!histLabel_) return;
//
//    if (!analysis_) {
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("（未加载）"));
//        return;
//    }
//
//    // 缓存路径
//    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
//    QDir().mkpath(dir);
//    histCachePath_ = dir + "/ctviewer_hist.png";
//
//    // 生成直方图图片（你的 VolumeAnalysisService::SaveHistogramImage 已实现）
//    //analysis_->SaveHistogramImage(histCachePath_.toStdString(), 1024);
//
//    QFileInfo fi(histCachePath_);
//    if (!fi.exists() || fi.size() <= 0) {
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("（直方图生成失败）"));
//        return;
//    }
//
//    QPixmap pm(histCachePath_);
//    if (pm.isNull()) {
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("（直方图加载失败）"));
//        return;
//    }
//
//    histPixmap_ = pm;
//    applyHistogramPixmap();
//}

void RenderPanel::applyHistogramPixmap()
{
    if (histPixmap_.isNull()) return;
    histLabel_->setText(QString());
    histLabel_->setPixmap(
        histPixmap_.scaled(histLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
    );
}

void RenderPanel::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (!histPixmap_.isNull()) {
        applyHistogramPixmap();
    }
}
