#pragma once

#include "naru/data/Database.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

// 데이터 속성 하나의 값을 형식에 맞는 입력 칸으로 편집한다
// (정수·실수 숫자 칸, 문자열 칸, 체크박스, 선택지 목록, 참조 항목 목록).
class ValueEditor : public QWidget {
    Q_OBJECT

public:
    ValueEditor(const naru::FieldDef& field, const naru::Value& value, const naru::Database* database,
                QWidget* parent = nullptr);

    naru::Value value() const;

    static QString typeLabel(naru::FieldType type);

signals:
    void edited();

private:
    naru::FieldDef field_;
    QSpinBox* integer_ = nullptr;
    QDoubleSpinBox* number_ = nullptr;
    QLineEdit* text_ = nullptr;
    QCheckBox* boolean_ = nullptr;
    QComboBox* combo_ = nullptr;
};
