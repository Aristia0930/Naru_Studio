#pragma once

#include "naru/data/Database.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class ValueEditor;

// 데이터 종류의 속성 하나를 정의한다: 표시 이름, 키, 형식, 기본값, 범위, 선택지, 참조 종류.
class FieldDialog : public QDialog {
    Q_OBJECT

public:
    // isNew가 false면 키는 바꿀 수 없다 (스크립트·저장 데이터가 키로 값을 찾기 때문).
    FieldDialog(const naru::FieldDef& field, const naru::DataType& owner, const naru::Database& database, bool isNew,
                QWidget* parent = nullptr);

    naru::FieldDef field() const { return field_; }

private:
    void accept() override;
    void rebuildDefaultEditor();
    naru::FieldDef currentDefinition() const; // 기본값을 제외한 현재 입력

    naru::FieldDef field_;
    const naru::DataType& owner_;
    const naru::Database& database_;
    bool isNew_;

    QFormLayout* form_;
    QLineEdit* name_;
    QLineEdit* key_;
    QComboBox* type_;
    QCheckBox* hasMin_;
    QDoubleSpinBox* min_;
    QCheckBox* hasMax_;
    QDoubleSpinBox* max_;
    QPlainTextEdit* choices_;
    QComboBox* refType_;
    QWidget* defaultHolder_;
    ValueEditor* default_ = nullptr;
    QLabel* hint_;
};
