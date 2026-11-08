#include "SceneView.h"

#include "ProjectDocument.h"
#include "ImageAssets.h"

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

// 객체 하나의 표시. 위치는 객체의 왼쪽 위 좌표다.
class SceneObjectItem : public QGraphicsRectItem {
public:
    SceneObjectItem(const naru::SceneObject& object, bool isPlayer, const QImage& image) {
        setFlags(ItemIsMovable | ItemIsSelectable);
        label_ = new QGraphicsSimpleTextItem(this);
        label_->setBrush(Qt::white);
        apply(object, isPlayer, image);
    }

    // 플레이어는 이름 앞에 ★, 통과 가능한 객체는 점선 테두리, 숨긴 객체는 흐리게 표시한다.
    void apply(const naru::SceneObject& object, bool isPlayer, const QImage& image) {
        prepareGeometryChange();
        image_ = image; settings_ = object.image;
        imageRect_ = naru::objectImageRect(object, image.width(), image.height());
        imageRect_.x -= object.x; imageRect_.y -= object.y;
        id_ = QString::fromStdString(object.id);
        setRect(0, 0, object.width, object.height);
        setPos(object.x, object.y);
        setZValue(object.image.order);
        setToolTip(!object.image.asset.empty() && image.isNull() ? QStringLiteral("이미지를 읽지 못해 색으로 표시합니다: ") + QString::fromStdString(object.image.asset) : QString());
        QColor color(QString::fromStdString(object.color));
        setBrush(color.isValid() ? color : QColor(Qt::gray));
        setPen(object.passable ? QPen(QColor(255, 255, 255, 200), 0, Qt::DashLine) : QPen(QColor(255, 255, 255, 120), 0));
        setOpacity(object.visible ? 1.0 : 0.35);
        label_->setText((isPlayer ? QStringLiteral("★ ") : QString()) + QString::fromStdString(object.name) +
                        (object.visible ? QString() : QStringLiteral(" (숨김)")));
        label_->setPos(4, 2);
        committedPos_ = pos();
    }

    const QString& id() const { return id_; }
    QPointF committedPos() const { return committedPos_; }
    QRectF boundingRect() const override {
        auto bounds = QGraphicsRectItem::boundingRect();
        if (!image_.isNull()) {
            QTransform t; t.translate(settings_.offsetX, settings_.offsetY); t.rotate(settings_.rotation);
            bounds = bounds.united(t.mapRect(QRectF(-settings_.anchorX * imageRect_.width, -settings_.anchorY * imageRect_.height, imageRect_.width, imageRect_.height)));
        }
        return bounds;
    }
    QPainterPath shape() const override { QPainterPath path; path.addRect(boundingRect()); return path; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        if (image_.isNull()) QGraphicsRectItem::paint(painter, option, widget);
        else {
            painter->save();
            painter->setOpacity(painter->opacity() * settings_.opacity);
            painter->setRenderHint(QPainter::SmoothPixmapTransform, settings_.smooth);
            painter->translate(settings_.offsetX, settings_.offsetY); painter->rotate(settings_.rotation);
            painter->translate(-settings_.anchorX * imageRect_.width, -settings_.anchorY * imageRect_.height);
            if (settings_.flipX) { painter->translate(imageRect_.width, 0); painter->scale(-1, 1); }
            if (settings_.flipY) { painter->translate(0, imageRect_.height); painter->scale(1, -1); }
            painter->drawImage(QRectF(0, 0, imageRect_.width, imageRect_.height), image_);
            painter->restore();
            painter->setPen(pen()); painter->setBrush(Qt::NoBrush); painter->drawRect(rect());
        }
        if (isSelected()) {
            painter->setPen(QPen(QColor(255, 210, 0), 0, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(rect());
        }
    }

private:
    QString id_;
    QGraphicsSimpleTextItem* label_;
    QPointF committedPos_;
    QImage image_;
    naru::ObjectImage settings_;
    naru::ImageRect imageRect_;
};

class SceneFrameItem : public QGraphicsRectItem {
public:
    SceneFrameItem(const naru::Scene& scene, QImage image) : QGraphicsRectItem(0, 0, scene.width, scene.height), scene_(scene), image_(std::move(image)) {
        const QColor color(QString::fromStdString(scene.background));
        setBrush(color.isValid() ? color : QColor(Qt::black)); setPen(QPen(QColor(200, 200, 200), 0));
        if (!scene.backgroundImage.asset.empty() && image_.isNull()) setToolTip(QStringLiteral("배경 이미지를 읽지 못해 배경색으로 표시합니다."));
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        QGraphicsRectItem::paint(painter, option, widget);
        if (image_.isNull()) return;
        painter->save(); painter->setClipRect(rect()); painter->setOpacity(scene_.backgroundImage.opacity);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, scene_.backgroundImage.smooth);
        const auto r = naru::backgroundImageRect(scene_, image_.width(), image_.height());
        if (scene_.backgroundImage.mode == "repeat") {
            QBrush brush(QPixmap::fromImage(image_)); QTransform t; t.translate(r.x, r.y); t.scale(scene_.backgroundImage.scale, scene_.backgroundImage.scale);
            brush.setTransform(t); painter->fillRect(rect(), brush);
        } else painter->drawImage(QRectF(r.x, r.y, r.width, r.height), image_);
        painter->restore();
    }
private:
    naru::Scene scene_;
    QImage image_;
};

SceneView::SceneView(ProjectDocument* doc, QWidget* parent)
    : QGraphicsView(parent), doc_(doc), canvas_(new QGraphicsScene(this)) {
    setScene(canvas_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);
    setBackgroundBrush(QColor(60, 60, 64));
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    connect(doc_, &ProjectDocument::objectIdChanged, this, [this](const QString& s, const QString& before, const QString& after) {
        if (s != sceneId_) return;
        const bool selected = selectedObjectId() == before;
        const bool wasBlocked = blockSignals(true);
        rebuild();
        blockSignals(wasBlocked);
        if (selected) { selectObject(after); emit selectionChangedTo(after); }
    });
    connect(doc_, &ProjectDocument::sceneChanged, this, [this](const QString& id) {
        if (id == sceneId_) {
            rebuild();
        }
    });
    connect(doc_, &ProjectDocument::scenesChanged, this, [this] {
        if (!doc_->project().findScene(sceneId_.toStdString())) {
            setSceneId(QString());
        }
    });
    connect(doc_, &ProjectDocument::objectChanged, this, &SceneView::updateObject);
    connect(doc_, &ProjectDocument::assetsChanged, this, [this] { images_.clear(); rebuild(); });
    connect(canvas_, &QGraphicsScene::selectionChanged, this, [this] {
        if (!syncingSelection_) {
            emit selectionChangedTo(selectedObjectId());
        }
    });
}

SceneView::~SceneView() {
    // 창을 닫을 때 QGraphicsScene이 항목을 지우며 선택 변경을 알린다.
    // 그 시점에는 인스펙터 등 다른 위젯이 이미 삭제됐을 수 있으므로 신호를 보내지 않는다.
    canvas_->disconnect(this);
    blockSignals(true);
}

void SceneView::setSceneId(const QString& sceneId) {
    if (sceneId == sceneId_) {
        return;
    }
    sceneId_ = sceneId;
    images_.clear();
    rebuild();
    autoFit_ = true;
    fitScene();
}

void SceneView::fitScene() {
    if (frame_) {
        fitInView(frame_->rect().adjusted(-40, -40, 40, 40), Qt::KeepAspectRatio);
    }
}

void SceneView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (autoFit_) {
        fitScene();
    }
}

void SceneView::rebuild() {
    QString selected = selectedObjectId();
    syncingSelection_ = true;
    canvas_->clear();
    items_.clear();
    frame_ = nullptr;
    const naru::Scene* scene = doc_->project().findScene(sceneId_.toStdString());
    if (scene) {
        frame_ = new SceneFrameItem(*scene, imageFor(scene->backgroundImage.asset));
        canvas_->addItem(frame_);
        frame_->setZValue(-10001);
        canvas_->setSceneRect(QRectF(0, 0, scene->width, scene->height).adjusted(-2000, -2000, 2000, 2000));
        for (const auto& object : scene->objects) {
            auto* item = new SceneObjectItem(object, object.id == scene->playerObjectId, imageFor(object.image.asset));
            canvas_->addItem(item);
            items_.insert(item->id(), item);
        }
    }
    if (auto* item = items_.value(selected)) {
        item->setSelected(true);
    }
    syncingSelection_ = false;
    if (selected != selectedObjectId()) {
        emit selectionChangedTo(selectedObjectId());
    }
}

void SceneView::updateObject(const QString& sceneId, const QString& objectId) {
    if (sceneId != sceneId_) {
        return;
    }
    const naru::Scene* scene = doc_->project().findScene(sceneId.toStdString());
    const naru::SceneObject* object = scene ? scene->findObject(objectId.toStdString()) : nullptr;
    if (auto* item = items_.value(objectId); item && object) {
        item->apply(*object, object->id == scene->playerObjectId, imageFor(object->image.asset));
    }
}

QImage SceneView::imageFor(const std::string& id) {
    if (id.empty()) return {};
    const auto key = QString::fromStdString(id);
    if (!images_.contains(key)) images_.insert(key, loadProjectImage(doc_, id));
    return images_.value(key);
}

void SceneView::selectObject(const QString& objectId) {
    if (objectId == selectedObjectId()) {
        return;
    }
    syncingSelection_ = true;
    canvas_->clearSelection();
    if (auto* item = items_.value(objectId)) {
        item->setSelected(true);
        ensureVisible(item);
    }
    syncingSelection_ = false;
}

QString SceneView::selectedObjectId() const {
    for (QGraphicsItem* item : canvas_->selectedItems()) {
        if (auto* objectItem = dynamic_cast<SceneObjectItem*>(item)) {
            return objectItem->id();
        }
    }
    return {};
}

QPointF SceneView::visibleCenter() const {
    return mapToScene(viewport()->rect().center());
}

void SceneView::wheelEvent(QWheelEvent* event) {
    // 휠: 확대/축소. 스크롤은 스크롤바나 가운데 버튼 드래그로 한다.
    double factor = std::pow(1.0015, event->angleDelta().y());
    double current = transform().m11();
    if (current * factor < 0.05 || current * factor > 20.0) {
        return;
    }
    autoFit_ = false;
    scale(factor, factor);
}

void SceneView::mouseDoubleClickEvent(QMouseEvent* event) {
    for (QGraphicsItem* item = itemAt(event->pos()); item; item = item->parentItem()) {
        if (auto* objectItem = dynamic_cast<SceneObjectItem*>(item)) {
            emit objectActivated(objectItem->id());
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void SceneView::mouseReleaseEvent(QMouseEvent* event) {
    QGraphicsView::mouseReleaseEvent(event);
    // 드래그가 끝난 객체마다 이동을 알린다. 정수 좌표로 맞춘다.
    for (auto* item : std::as_const(items_)) {
        QPointF p = item->pos();
        if (p != item->committedPos()) {
            emit objectMoved(item->id(), std::round(p.x()), std::round(p.y()));
        }
    }
}
