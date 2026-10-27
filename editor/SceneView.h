#pragma once

#include <QGraphicsView>
#include <QHash>
#include <QPointF>
#include <QString>

class ProjectDocument;
class QGraphicsRectItem;
class SceneObjectItem;

// 장면 하나를 2D로 보여 주고 객체를 드래그로 옮기는 캔버스.
// 드래그는 놓을 때 objectMoved를 보내며, 실제 변경은 MainWindow가 Undo 명령으로 기록한다.
class SceneView : public QGraphicsView {
    Q_OBJECT

public:
    explicit SceneView(ProjectDocument* doc, QWidget* parent = nullptr);
    ~SceneView() override;

    void setSceneId(const QString& sceneId);
    QString sceneId() const { return sceneId_; }
    void selectObject(const QString& objectId);
    QString selectedObjectId() const;
    // 화면 중앙의 장면 좌표 (새 객체 배치 위치).
    QPointF visibleCenter() const;

signals:
    void objectMoved(const QString& objectId, double x, double y);
    void selectionChangedTo(const QString& objectId);
    void objectActivated(const QString& objectId); // 더블클릭

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild();
    void fitScene();
    void updateObject(const QString& sceneId, const QString& objectId);

    ProjectDocument* doc_;
    QGraphicsScene* canvas_;
    QGraphicsRectItem* frame_ = nullptr;
    QString sceneId_;
    QHash<QString, SceneObjectItem*> items_;
    bool syncingSelection_ = false;
    // 사용자가 확대/축소하기 전까지는 창 크기에 맞춰 장면 전체를 보여 준다.
    bool autoFit_ = true;
};
