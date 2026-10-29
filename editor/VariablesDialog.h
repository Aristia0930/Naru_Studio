#pragma once

#include "naru/project/Project.h"

#include <QDialog>
#include <QString>

#include <vector>

class QComboBox;
class QLineEdit;
class QTableWidget;

// 프로젝트 공용 변수·스위치 정의 창. 이름·종류·처음 값·설명을 표로 편집한다.
class VariablesDialog : public QDialog {
    Q_OBJECT

public:
    explicit VariablesDialog(const std::vector<naru::VariableDef>& variables, QWidget* parent = nullptr);

    std::vector<naru::VariableDef> variables() const { return result_; }

    // 종류 이름: 정수·소수·글자·스위치 (처음 값의 형식으로 정한다).
    static QString kindLabel(const naru::Value& value);

private:
    enum Kind { Integer, Decimal, Text, Switch };
    // 표 한 줄의 입력 내용 (숫자를 아직 읽지 않은 글자 그대로).
    struct Entry {
        QString name;
        int kind = Integer;
        QString text;
        bool on = false;
        QString description;
    };
    struct Row {
        QLineEdit* name;
        QComboBox* kind;
        QLineEdit* text;    // 정수·소수·글자
        QComboBox* boolean; // 스위치
        QLineEdit* description;
    };

    void fill(const std::vector<Entry>& entries, int current);
    std::vector<Entry> entries() const;
    void addRow(const Entry& entry);
    void moveCurrent(int delta);
    void removeCurrent();
    void accept() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    QTableWidget* table_ = nullptr;
    std::vector<Row> rows_;
    std::vector<naru::VariableDef> result_;
};
