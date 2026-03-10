#include "RenderPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QResizeEvent>
#include <QMetaObject>
#include <QThread>

#include <cmath>

#include "AppController.h" // AppSession
#include "VolumeAnalysisService.h"

static inline double clamp01(double v)
{
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

RenderPanel::RenderPanel(QWidget* parent)
    : QWidget(parent)
{
    auto v = new QVBoxLayout(this);
    v->setContentsMargins(6, 6, 6, 6);
    v->setSpacing(8);

    // 1) 鐩存柟鍥?
    auto histGroup = new QGroupBox(QStringLiteral("Histogram"), this);
    histGroup->setStyleSheet(
        "QGroupBox{color:#ddd; border:1px solid #333; margin-top:8px;}"
        "QGroupBox::title{subcontrol-origin: margin; left:8px;}"
    );
    auto hv = new QVBoxLayout(histGroup);

    histLabel_ = new QLabel(QStringLiteral("(Not loaded)"), histGroup);
    histLabel_->setMinimumHeight(140);
    histLabel_->setAlignment(Qt::AlignCenter);
    histLabel_->setStyleSheet("QLabel{background:#111; border:1px solid #444; color:#888;}");
    hv->addWidget(histLabel_);

    v->addWidget(histGroup);

    // 2) ISO
    auto isoGroup = new QGroupBox(QStringLiteral("Threshold / ISO"), this);
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

    // 3) 鏉愯川
    auto matGroup = new QGroupBox(QStringLiteral("Lighting / Material"), this);
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

    shadeOn_ = new QCheckBox(QStringLiteral("Shade"), matGroup);
    mv->addWidget(shadeOn_);

    v->addWidget(matGroup);
    v->addStretch();

    //淇敼 閬垮厤姣忎竴甯ч兘瑙﹀彂3D閲嶇畻
    auto sliderToIso= [this](int v) {
        const double t = static_cast<double>(v) / 1000.0;
		return rangeMin_ + (rangeMax_ - rangeMin_) * t;
		};

    //3/3 涓や釜淇″彿鏈夌偣闂
    connect(isoSlider_, &QSlider::valueChanged, this, [this,sliderToIso](int v) {
        if (!state_ || updatingUi_) { 
            return;
        }

		const double iso = sliderToIso(v);
        isoValueLabel_->setText(QString("ISO: %1").arg(iso, 0, 'f', 2));
        });

    //鎷栧姩鏉炬墜鍚庡啀鎻愪氦涓€娆?
    connect(isoSlider_, &QSlider::sliderReleased, this, [this, sliderToIso]() {
        if (!state_ || updatingUi_) return;
        const double iso = sliderToIso(isoSlider_->value());
        state_->SetIsoValue(iso);
        });


    auto onMatChanged = [this]() {
        if (!state_ || updatingUi_) return;

        auto mat = state_->GetMaterial();
        mat.ambient = ambient_->value() / 100.0;
        mat.diffuse = diffuse_->value() / 100.0;
        mat.specular = specular_->value() / 100.0;
        // power 鍙?1~100
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
    // 鍏抽敭锛氳瑙傚療鑰呭嚟璇佸け鏁堬紝state 涓嬫閫氱煡浼氳嚜鍔ㄦ竻鎺?expired 鐨?observer
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

    // 姣忔閲嶆柊缁戝畾 state锛岄兘閲嶆柊鐢熸垚 token
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
        histLabel_->setText(QStringLiteral("(Not loaded)"));
        return;
    }

    // 浠?state 璇?scalar range锛堢敤浜?slider 鏄犲皠锛?
    const auto r = state_->GetDataRange();
    rangeMin_ = r[0];
    rangeMax_ = r[1];

    // state -> UI锛氭敞鍐岃瀵熻€咃紙owner 鏄?shared_ptr<void>锛屽唴閮ㄥ瓨 weak_ptr锛?
    // 浣犵殑椤圭洰灏辨槸杩欎箞璁捐鐨勶細AddObserver(owner, cb) :contentReference[oaicite:2]{index=2}
    state_->AddObserver(lifeToken_, [this](UpdateFlags flags) {
        if (QThread::currentThread() == thread()) {
            syncFromState(flags);
            return;
        }

        QMetaObject::invokeMethod(
            this,
            [this, flags]() {
                syncFromState(flags);
            },
            Qt::QueuedConnection);
    });

    // 棣栨鍏ㄩ噺鍚屾
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
        const auto mat = state_->GetMaterial();

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
//        histLabel_->setText(QStringLiteral("锛堟湭鍔犺浇锛?));
//        return;
//    }
//
//    // 缂撳瓨璺緞
//    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
//    QDir().mkpath(dir);
//    histCachePath_ = dir + "/ctviewer_hist.png";
//
//    // 鐢熸垚鐩存柟鍥惧浘鐗囷紙浣犵殑 VolumeAnalysisService::SaveHistogramImage 宸插疄鐜帮級
//    //analysis_->SaveHistogramImage(histCachePath_.toStdString(), 1024);
//
//    QFileInfo fi(histCachePath_);
//    if (!fi.exists() || fi.size() <= 0) {
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("锛堢洿鏂瑰浘鐢熸垚澶辫触锛?));
//        return;
//    }
//
//    QPixmap pm(histCachePath_);
//    if (pm.isNull()) {
//        histPixmap_ = QPixmap();
//        histLabel_->setPixmap(QPixmap());
//        histLabel_->setText(QStringLiteral("锛堢洿鏂瑰浘鍔犺浇澶辫触锛?));
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

