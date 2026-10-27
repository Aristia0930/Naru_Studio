#include "SceneView.h"

#include "ProjectDocument.h"

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

#include <cmath>

// 객체 하나의 표시. 위치는 객체의 왼쪽 위 좌표다.
class SceneObjectItem : public QGraphicsRectItem {
public:
    explicit SceneObjectItem(const naru::SceneObject& object) {
        setFlags(ItemIsMovable | ItemIsSelectable);
        label_ = new QGraphicsSimpleTextItem(this);
        label_->setBrush(Qt::white);
        apply(object);
    }

    void apply(const naru::SceneObject& object) {
        id_ = QString::fromStdString(object.id);
        setRect(0, 0, object.width, object.height);
        setPos(object.x, object.y);
        QColor color(QString::fromStdString(object.color));
        setBrush(color.isValid() ? color : QColor(Qt::gray));
        setPen(QPen(QColor(255, 255, 255, 120), 0));
        label_->setText(QString::fromStdString(object.name));
        label_->setPos(4, 2);
        committedPos_ = pos();
    }

    const QString& id() const { return id_; }
    QPointF committedPos() const { return committedPos_; }

protected:
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override {
        QGraphicsRectItem::paint(painter, option, widget);
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
};

SceneView::SceneView(ProjectDocument* doc, QWidget* parent)
    : QGraphicsView(parent), doc_(doc), canvas_(new QGraphicsScene(this)) {
    setScene(canvas_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::RubberBandDrag);
    setBackgroundBrush(QColor(60, 60, 64));
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

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
        QColor background(QString::fromStdString(scene->background));
        frame_ = canvas_->addRect(0, 0, scene->width, scene->height, QPen(QColor(200, 200, 200), 0),
                                  background.isValid() ? background : QColor(Qt::black));
        frame_->setZValue(-1);
        canvas_->setSceneRect(QRectF(0, 0, scene->width, scene->height).adjusted(-2000, -2000, 2000, 2000));
        for (const auto& object : scene->objects) {
            auto* item = new SceneObjectItem(object);
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
        item->apply(*object);
    }
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
