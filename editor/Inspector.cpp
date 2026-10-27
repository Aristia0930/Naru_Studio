#include "Inspector.h"

#include "ProjectDocument.h"
#include "UndoCommands.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

enum Field { Name, X, Y, Width, Height, Color, Passable };

QDoubleSpinBox* coordinateBox(QWidget* parent, double min) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(min, 100000);
    box->setDecimals(0);
    box->setKeyboardTracking(false);
    return box;
}

void showColor(QPushButton* button, const std::string& color) {
    QString c = QString::fromStdString(color);
    button->setText(c);
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(c, QColor(c).lightness() > 128 ? QStringLiteral("black") : QStringLiteral("white")));
}

} // namespace

Inspector::Inspector(ProjectDocument* doc, QWidget* parent) : QWidget(parent), doc_(doc) {
    pages_ = new QStackedWidget(this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(pages_);
    layout->addStretch();

    emptyLabel_ = new QLabel(QStringLiteral("장면이나 객체를 선택하세요."), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    pages_->addWidget(emptyLabel_);

    // 객체 페이지
    auto* objectPage = new QWidget(this);
    auto* objectForm = new QFormLayout(objectPage);
    objectIdLabel_ = new QLabel(objectPage);
    objectIdLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    objectName_ = new QLineEdit(objectPage);
    objectX_ = coordinateBox(objectPage, -100000);
    objectY_ = coordinateBox(objectPage, -100000);
    objectW_ = coordinateBox(objectPage, 1);
    objectH_ = coordinateBox(objectPage, 1);
    objectColor_ = new QPushButton(objectPage);
    objectForm->addRow(QStringLiteral("ID"), objectIdLabel_);
    objectForm->addRow(QStringLiteral("이름"), objectName_);
    objectForm->addRow(QStringLiteral("X"), objectX_);
    objectForm->addRow(QStringLiteral("Y"), objectY_);
    objectForm->addRow(QStringLiteral("너비"), objectW_);
    objectForm->addRow(QStringLiteral("높이"), objectH_);
    objectForm->addRow(QStringLiteral("색"), objectColor_);
    objectPlayer_ = new QCheckBox(QStringLiteral("방향키로 조작 (장면마다 하나)"), objectPage);
    objectPlayer_->setToolTip(QStringLiteral("게임 실행 화면에서 이 객체를 방향키로 움직입니다. 결정 키로 앞의 객체를 조사합니다."));
    objectPassable_ = new QCheckBox(QStringLiteral("지나갈 수 있음"), objectPage);
    objectPassable_->setToolTip(QStringLiteral("끄면 벽처럼 플레이어를 막습니다. 켜면 위를 지나갈 수 있고, 밟으면 '플레이어 접촉' 이벤트가 실행됩니다."));
    objectForm->addRow(QStringLiteral("플레이어"), objectPlayer_);
    objectForm->addRow(QStringLiteral("통과"), objectPassable_);
    pages_->addWidget(objectPage);

    connect(objectName_, &QLineEdit::editingFinished, this, [this] { commitObject(Name); });
    connect(objectX_, &QDoubleSpinBox::valueChanged, this, [this] { commitObject(X); });
    connect(objectY_, &QDoubleSpinBox::valueChanged, this, [this] { commitObject(Y); });
    connect(objectW_, &QDoubleSpinBox::valueChanged, this, [this] { commitObject(Width); });
    connect(objectH_, &QDoubleSpinBox::valueChanged, this, [this] { commitObject(Height); });
    connect(objectColor_, &QPushButton::clicked, this, &Inspector::chooseObjectColor);
    connect(objectPlayer_, &QCheckBox::toggled, this, &Inspector::setPlayer);
    connect(objectPassable_, &QCheckBox::toggled, this, [this] { commitObject(Passable); });

    // 장면 페이지
    auto* scenePage = new QWidget(this);
    auto* sceneForm = new QFormLayout(scenePage);
    sceneIdLabel_ = new QLabel(scenePage);
    sceneIdLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sceneName_ = new QLineEdit(scenePage);
    sceneW_ = new QSpinBox(scenePage);
    sceneH_ = new QSpinBox(scenePage);
    for (QSpinBox* box : {sceneW_, sceneH_}) {
        box->setRange(16, 16384);
        box->setKeyboardTracking(false);
    }
    sceneColor_ = new QPushButton(scenePage);
    sceneForm->addRow(QStringLiteral("장면 ID"), sceneIdLabel_);
    sceneForm->addRow(QStringLiteral("이름"), sceneName_);
    sceneForm->addRow(QStringLiteral("너비"), sceneW_);
    sceneForm->addRow(QStringLiteral("높이"), sceneH_);
    sceneForm->addRow(QStringLiteral("배경색"), sceneColor_);
    pages_->addWidget(scenePage);

    connect(sceneName_, &QLineEdit::editingFinished, this, [this] { commitScene(Name); });
    connect(sceneW_, &QSpinBox::valueChanged, this, [this] { commitScene(Width); });
    connect(sceneH_, &QSpinBox::valueChanged, this, [this] { commitScene(Height); });
    connect(sceneColor_, &QPushButton::clicked, this, &Inspector::chooseSceneColor);

    connect(doc_, &ProjectDocument::objectChanged, this, [this](const QString& s, const QString& o) {
        if (s == sceneId_ && o == objectId_) {
            refresh();
        }
    });
    connect(doc_, &ProjectDocument::sceneChanged, this, [this](const QString& s) {
        if (s == sceneId_) {
            refresh();
        }
    });
    connect(doc_, &ProjectDocument::scenesChanged, this, &Inspector::refresh);
    refresh();
}

void Inspector::setTarget(const QString& sceneId, const QString& objectId) {
    sceneId_ = sceneId;
    objectId_ = objectId;
    refresh();
}

void Inspector::refresh() {
    loading_ = true;
    const naru::Scene* scene = doc_->project().findScene(sceneId_.toStdString());
    const naru::SceneObject* object = scene ? scene->findObject(objectId_.toStdString()) : nullptr;
    if (object) {
        pages_->setCurrentIndex(1);
        objectIdLabel_->setText(QString::fromStdString(object->id));
        objectName_->setText(QString::fromStdString(object->name));
        objectX_->setValue(object->x);
        objectY_->setValue(object->y);
        objectW_->setValue(object->width);
        objectH_->setValue(object->height);
        showColor(objectColor_, object->color);
        objectPlayer_->setChecked(scene->playerObjectId == object->id);
        objectPassable_->setChecked(object->passable);
    } else if (scene) {
        pages_->setCurrentIndex(2);
        sceneIdLabel_->setText(QString::fromStdString(scene->id));
        sceneName_->setText(QString::fromStdString(scene->name));
        sceneW_->setValue(scene->width);
        sceneH_->setValue(scene->height);
        showColor(sceneColor_, scene->background);
    } else {
        pages_->setCurrentIndex(0);
    }
    loading_ = false;
}

void Inspector::commitObject(int field) {
    if (loading_) {
        return;
    }
    const naru::Scene* scene = doc_->project().findScene(sceneId_.toStdString());
    const naru::SceneObject* current = scene ? scene->findObject(objectId_.toStdString()) : nullptr;
    if (!current) {
        return;
    }
    naru::SceneObject after = *current;
    switch (field) {
    case Name: {
        QString name = objectName_->text().trimmed();
        if (name.isEmpty()) {
            refresh();
            return;
        }
        after.name = name.toStdString();
        break;
    }
    case X: after.x = objectX_->value(); break;
    case Y: after.y = objectY_->value(); break;
    case Width: after.width = objectW_->value(); break;
    case Height: after.height = objectH_->value(); break;
    case Passable: after.passable = objectPassable_->isChecked(); break;
    default: break;
    }
    if (after == *current) {
        return;
    }
    // 체크 상자는 연속 변경으로 묶지 않는다.
    doc_->undoStack()->push(
        new SetObjectCommand(doc_, sceneId_.toStdString(), *current, after, field == Passable ? -1 : field));
}

void Inspector::commitScene(int field) {
    if (loading_) {
        return;
    }
    const naru::Scene* current = doc_->project().findScene(sceneId_.toStdString());
    if (!current) {
        return;
    }
    naru::Scene after = *current;
    switch (field) {
    case Name: {
        QString name = sceneName_->text().trimmed();
        if (name.isEmpty()) {
            refresh();
            return;
        }
        after.name = name.toStdString();
        break;
    }
    case Width: after.width = sceneW_->value(); break;
    case Height: after.height = sceneH_->value(); break;
    default: break;
    }
    if (after.name == current->name && after.width == current->width && after.height == current->height) {
        return;
    }
    doc_->undoStack()->push(new SetSceneInfoCommand(doc_, *current, after, field));
}

void Inspector::setPlayer(bool on) {
    if (loading_) {
        return;
    }
    const naru::Scene* current = doc_->project().findScene(sceneId_.toStdString());
    if (!current) {
        return;
    }
    naru::Scene after = *current;
    after.playerObjectId = on ? objectId_.toStdString() : std::string();
    if (after.playerObjectId != current->playerObjectId) {
        doc_->undoStack()->push(new SetSceneInfoCommand(doc_, *current, after, -1));
    }
}

void Inspector::chooseObjectColor() {
    const naru::Scene* scene = doc_->project().findScene(sceneId_.toStdString());
    const naru::SceneObject* current = scene ? scene->findObject(objectId_.toStdString()) : nullptr;
    if (!current) {
        return;
    }
    QColor color = QColorDialog::getColor(QColor(QString::fromStdString(current->color)), this);
    if (!color.isValid()) {
        return;
    }
    naru::SceneObject after = *current;
    after.color = color.name().toStdString();
    if (after != *current) {
        doc_->undoStack()->push(new SetObjectCommand(doc_, sceneId_.toStdString(), *current, after));
    }
}

void Inspector::chooseSceneColor() {
    const naru::Scene* current = doc_->project().findScene(sceneId_.toStdString());
    if (!current) {
        return;
    }
    QColor color = QColorDialog::getColor(QColor(QString::fromStdString(current->background)), this);
    if (!color.isValid()) {
        return;
    }
    naru::Scene after = *current;
    after.background = color.name().toStdString();
    if (after.background != current->background) {
        doc_->undoStack()->push(new SetSceneInfoCommand(doc_, *current, after, -1));
    }
}
