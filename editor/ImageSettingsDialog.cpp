#include "ImageSettingsDialog.h"
#include "ImageAssets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

ImageSettingsDialog::ImageSettingsDialog(ProjectDocument* doc, const naru::SceneObject& object, QWidget* parent)
    : QDialog(parent), doc_(doc), background_(false), object_(object.image) { build(object_.asset); }
ImageSettingsDialog::ImageSettingsDialog(ProjectDocument* doc, const naru::Scene& scene, QWidget* parent)
    : QDialog(parent), doc_(doc), background_(true), scene_(scene.backgroundImage) { build(scene_.asset); }

void ImageSettingsDialog::build(const std::string& selected) {
    setObjectName(background_ ? QStringLiteral("backgroundImageDialog") : QStringLiteral("objectImageDialog"));
    setWindowTitle(background_ ? QStringLiteral("장면 배경 이미지") : QStringLiteral("객체 이미지"));
    resize(860, 710);
    auto* outer = new QVBoxLayout(this);
    auto* help = new QLabel(background_
        ? QStringLiteral("장면 배경색 위, 모든 객체 아래에 표시합니다. 배경은 충돌하지 않습니다.")
        : QStringLiteral("그림 설정은 충돌 영역의 위치·너비·높이를 바꾸지 않습니다.\n그림 위치 = 객체 위치 + 오프셋. 기준점은 그림 안의 0~1 위치입니다."), this);
    help->setWordWrap(true); outer->addWidget(help);
    auto* body = new QHBoxLayout; outer->addLayout(body, 1);
    auto* scroll = new QScrollArea(this); scroll->setWidgetResizable(true); body->addWidget(scroll, 1);
    auto* fields = new QWidget(scroll); auto* form = new QFormLayout(fields); scroll->setWidget(fields);
    assets_ = new QComboBox(fields); assets_->setObjectName(QStringLiteral("imageAsset"));
    assets_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); assets_->setMinimumContentsLength(20);
    assets_->addItem(QStringLiteral("없음 (이미지 지우기)"), QString());
    for (const auto& a : doc_->project().assets) if (a.kind == "image") {
        assets_->addItem(QStringLiteral("%1 [%2] (%3)").arg(QString::fromStdString(a.name),
            a.group.empty() ? QStringLiteral("미분류") : QString::fromStdString(a.group), QString::fromStdString(a.id)), QString::fromStdString(a.id));
    }
    int index = assets_->findData(QString::fromStdString(selected));
    if (index < 0) { assets_->addItem(QStringLiteral("누락: ") + QString::fromStdString(selected), QString::fromStdString(selected)); index = assets_->count() - 1; }
    assets_->setCurrentIndex(index); form->addRow(QStringLiteral("이미지"), assets_);
    if (background_) {
        mode_ = new QComboBox(fields); mode_->setObjectName(QStringLiteral("backgroundMode"));
        for (const auto& pair : {std::pair{"맞추기 (비율 유지)", "fit"}, {"채우기 (잘릴 수 있음)", "fill"},
            {"늘리기", "stretch"}, {"원본 크기", "original"}, {"반복", "repeat"}})
            mode_->addItem(QString::fromUtf8(pair.first), QString::fromUtf8(pair.second));
        mode_->setCurrentIndex(mode_->findData(QString::fromStdString(scene_.mode)));
        form->addRow(QStringLiteral("표시 방식"), mode_);
    }
    const auto number = [&](const QString& key, const QString& label, double value, double lo, double hi, int decimals = 2) {
        auto* box = new QDoubleSpinBox(fields); box->setObjectName(key); box->setRange(lo, hi);
        box->setDecimals(decimals); box->setValue(value); box->setKeyboardTracking(false);
        numbers_.insert(key, box); form->addRow(label, box);
    };
    if (!background_) {
        number("imageWidth", QStringLiteral(" 그림 너비 (0=원본)"), object_.width, 0, 100000);
        number("imageHeight", QStringLiteral("그림 높이 (0=원본)"), object_.height, 0, 100000);
    }
    number("imageOffsetX", QStringLiteral("오프셋 X"), background_ ? scene_.offsetX : object_.offsetX, -100000, 100000);
    number("imageOffsetY", QStringLiteral("오프셋 Y"), background_ ? scene_.offsetY : object_.offsetY, -100000, 100000);
    if (background_) number("imageScale", QStringLiteral("배율"), scene_.scale, .01, 100);
    else {
        number("imageScaleX", QStringLiteral("가로 배율"), object_.scaleX, .01, 100);
        number("imageScaleY", QStringLiteral("세로 배율"), object_.scaleY, .01, 100);
        number("imageAnchorX", QStringLiteral("기준점 X (0=왼쪽, 1=오른쪽)"), object_.anchorX, 0, 1);
        number("imageAnchorY", QStringLiteral("기준점 Y (0=위, 1=아래)"), object_.anchorY, 0, 1);
        number("imageRotation", QStringLiteral("시계 방향 회전 (도)"), object_.rotation, -360, 360);
        flipX_ = new QCheckBox(QStringLiteral("좌우 뒤집기"), fields); flipX_->setChecked(object_.flipX); form->addRow(flipX_);
        flipY_ = new QCheckBox(QStringLiteral("상하 뒤집기"), fields); flipY_->setChecked(object_.flipY); form->addRow(flipY_);
        order_ = new QSpinBox(fields); order_->setObjectName(QStringLiteral("imageOrder")); order_->setRange(-10000, 10000); order_->setValue(object_.order);
        order_->setToolTip(QStringLiteral("값이 클수록 앞에 표시합니다. 같으면 장면 객체 목록 순서대로 그립니다.")); form->addRow(QStringLiteral("그리기 순서"), order_);
    }
    number("imageOpacity", QStringLiteral("불투명도 (%)"), (background_ ? scene_.opacity : object_.opacity) * 100, 0, 100);
    smooth_ = new QCheckBox(QStringLiteral("부드럽게 확대·축소 (끄면 도트용)"), fields);
    smooth_->setChecked(background_ ? scene_.smooth : object_.smooth); form->addRow(smooth_);
    auto* right = new QVBoxLayout; body->addLayout(right, 1);
    right->addWidget(new QLabel(QStringLiteral("원본 이미지 미리보기"), this));
    preview_ = new QLabel(this); preview_->setMinimumSize(300, 220); preview_->setAlignment(Qt::AlignCenter); right->addWidget(preview_, 1);
    status_ = new QLabel(this); status_->setTextFormat(Qt::PlainText); status_->setWordWrap(true); right->addWidget(status_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("적용")); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
    outer->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(assets_, &QComboBox::currentIndexChanged, this, [this] { updatePreview(); }); updatePreview();
}

void ImageSettingsDialog::updatePreview() {
    preview_->clear();
    const auto id = assets_->currentData().toString().toStdString();
    if (id.empty()) { status_->setText(QStringLiteral("이미지 없음. 객체는 색 사각형, 배경은 배경색으로 표시합니다.")); return; }
    QString error; const auto image = loadProjectImage(doc_, id, &error);
    if (image.isNull()) { status_->setText(error + QStringLiteral("\n참조는 유지됩니다. 에셋 목록의 파일 교체로 복구하세요.")); return; }
    QPixmap pixmap(320, 240); QPainter painter(&pixmap);
    for (int y = 0; y < 240; y += 12) for (int x = 0; x < 320; x += 12)
        painter.fillRect(x, y, 12, 12, (x / 12 + y / 12) % 2 ? QColor("#bbc1ca") : QColor("#e7ebef"));
    const auto scaled = image.scaled(320, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawImage((320 - scaled.width()) / 2, (240 - scaled.height()) / 2, scaled); painter.end(); preview_->setPixmap(pixmap);
    status_->setText(QStringLiteral("%1 × %2 픽셀\n설정 적용 후 장면 캔버스와 Ctrl+R 게임에서 결과를 확인하세요.").arg(image.width()).arg(image.height()));
}

naru::ObjectImage ImageSettingsDialog::objectImage() const {
    auto i = object_; i.asset = assets_->currentData().toString().toStdString();
    i.width = numbers_["imageWidth"]->value(); i.height = numbers_["imageHeight"]->value();
    i.offsetX = numbers_["imageOffsetX"]->value(); i.offsetY = numbers_["imageOffsetY"]->value();
    i.scaleX = numbers_["imageScaleX"]->value(); i.scaleY = numbers_["imageScaleY"]->value();
    i.anchorX = numbers_["imageAnchorX"]->value(); i.anchorY = numbers_["imageAnchorY"]->value();
    i.rotation = numbers_["imageRotation"]->value(); i.opacity = numbers_["imageOpacity"]->value() / 100;
    i.flipX = flipX_->isChecked(); i.flipY = flipY_->isChecked(); i.smooth = smooth_->isChecked(); i.order = order_->value(); return i;
}
naru::BackgroundImage ImageSettingsDialog::backgroundImage() const {
    auto b = scene_; b.asset = assets_->currentData().toString().toStdString(); b.mode = mode_->currentData().toString().toStdString();
    b.offsetX = numbers_["imageOffsetX"]->value(); b.offsetY = numbers_["imageOffsetY"]->value(); b.scale = numbers_["imageScale"]->value();
    b.opacity = numbers_["imageOpacity"]->value() / 100; b.smooth = smooth_->isChecked(); return b;
}
