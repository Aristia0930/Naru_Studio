#pragma once

#include "naru/events/EventData.h"
#include "naru/project/Project.h"

#include <QDialog>
#include <QString>

#include <vector>

class QListWidget;

// 이벤트 실행 조건 목록 창. 조건 하나는 조건 분기와 같은 입력 창(변수·비교·값)으로 편집한다.
class EventConditionsDialog : public QDialog {
    Q_OBJECT

public:
    EventConditionsDialog(const std::vector<naru::EventCondition>& conditions, const naru::Project* project,
                          QWidget* parent = nullptr);

    std::vector<naru::EventCondition> conditions() const { return conditions_; }

    // "gold >= 10", 스위치는 "door_open == 참".
    static QString describe(const naru::EventCondition& condition);
    // 이벤트 편집 화면에 보이는 요약. 비어 있으면 "없음 (언제나)".
    static QString summary(const std::vector<naru::EventCondition>& conditions);

private:
    void refresh(int current);
    // 조건 하나를 입력 창으로 편집한다. 취소하면 false.
    bool editOne(naru::EventCondition& condition);
    void addCondition();
    void editCurrent();
    void removeCurrent();

    const naru::Project* project_;
    std::vector<naru::EventCondition> conditions_;
    QListWidget* list_ = nullptr;
};
