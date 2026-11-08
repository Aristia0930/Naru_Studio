#pragma once

#include <QString>
#include <QWidget>

class ProjectDocument;
class QCheckBox;
class QDoubleSpinBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

// 선택한 객체(없으면 현재 장면)의 속성 편집. 변경은 Undo 명령으로 기록한다.
class Inspector : public QWidget {
    Q_OBJECT

public:
    explicit Inspector(ProjectDocument* doc, QWidget* parent = nullptr);

    void setTarget(const QString& sceneId, const QString& objectId);

private:
    void refresh();
    void commitObject(int field);
    void commitScene(int field);
    void setPlayer(bool on);
    void chooseObjectColor();
    void chooseNameBackgroundColor();
    void chooseSceneColor();

    ProjectDocument* doc_;
    QString sceneId_;
    QString objectId_;
    bool loading_ = false;

    QStackedWidget* pages_;
    QLabel* emptyLabel_;

    QLineEdit* objectIdLabel_;
    QLineEdit* objectName_;
    QCheckBox* objectShowName_;
    QLineEdit* objectNameText_;
    QCheckBox* objectNameTransparent_;
    QPushButton* objectNameBackgroundColor_;
    QComboBox* objectNamePosition_;
    QDoubleSpinBox* objectX_;
    QDoubleSpinBox* objectY_;
    QDoubleSpinBox* objectW_;
    QDoubleSpinBox* objectH_;
    QPushButton* objectColor_;
    QPushButton* objectImage_;
    QCheckBox* objectPlayer_;
    QCheckBox* objectPassable_;
    QCheckBox* objectVisible_;

    QLabel* sceneIdLabel_;
    QLineEdit* sceneName_;
    QSpinBox* sceneW_;
    QSpinBox* sceneH_;
    QPushButton* sceneColor_;
    QPushButton* backgroundImage_;
};
