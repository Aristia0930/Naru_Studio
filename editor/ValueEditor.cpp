#include "ValueEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>

#include <algorithm>
#include <climits>
#include <cmath>

using naru::FieldType;
using naru::Value;

namespace {

QString q(const std::string& s) {
    return QString::fromStdString(s);
}

double toNumber(const Value& v) {
    if (const auto* i = std::get_if<std::int64_t>(&v)) {
        return static_cast<double>(*i);
    }
    if (const auto* d = std::get_if<double>(&v)) {
        return *d;
    }
    return 0.0;
}

} // namespace

QString ValueEditor::typeLabel(FieldType type) {
    switch (type) {
    case FieldType::Integer: return QStringLiteral("정수");
    case FieldType::Number: return QStringLiteral("실수");
    case FieldType::Text: return QStringLiteral("문자열");
    case FieldType::Boolean: return QStringLiteral("참/거짓");
    case FieldType::Choice: return QStringLiteral("선택지");
    case FieldType::Reference: return QStringLiteral("데이터 참조");
    }
    return {};
}

ValueEditor::ValueEditor(const naru::FieldDef& field, const Value& value, const naru::Database* database,
                         QWidget* parent)
    : QWidget(parent), field_(field) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    switch (field.type) {
    case FieldType::Integer: {
        integer_ = new QSpinBox(this);
        // QSpinBox는 32비트 범위만 다룬다.
        integer_->setRange(field.min ? static_cast<int>(std::max<double>(*field.min, INT_MIN)) : INT_MIN,
                           field.max ? static_cast<int>(std::min<double>(*field.max, INT_MAX)) : INT_MAX);
        integer_->setValue(static_cast<int>(std::clamp<double>(toNumber(value), INT_MIN, INT_MAX)));
        integer_->setKeyboardTracking(false);
        layout->addWidget(integer_);
        connect(integer_, &QSpinBox::valueChanged, this, &ValueEditor::edited);
        break;
    }
    case FieldType::Number:
        number_ = new QDoubleSpinBox(this);
        number_->setDecimals(3);
        number_->setRange(field.min.value_or(-1e12), field.max.value_or(1e12));
        number_->setValue(toNumber(value));
        number_->setKeyboardTracking(false);
        layout->addWidget(number_);
        connect(number_, &QDoubleSpinBox::valueChanged, this, &ValueEditor::edited);
        break;
    case FieldType::Text:
        text_ = new QLineEdit(this);
        if (const auto* s = std::get_if<std::string>(&value)) {
            text_->setText(q(*s));
        }
        layout->addWidget(text_);
        connect(text_, &QLineEdit::editingFinished, this, [this] {
            if (text_->isModified()) {
                text_->setModified(false);
                emit edited();
            }
        });
        break;
    case FieldType::Boolean:
        boolean_ = new QCheckBox(this);
        if (const auto* b = std::get_if<bool>(&value)) {
            boolean_->setChecked(*b);
        }
        layout->addWidget(boolean_);
        connect(boolean_, &QCheckBox::toggled, this, &ValueEditor::edited);
        break;
    case FieldType::Choice: {
        combo_ = new QComboBox(this);
        for (const auto& choice : field.choices) {
            combo_->addItem(q(choice), q(choice));
        }
        const auto* s = std::get_if<std::string>(&value);
        int index = s ? combo_->findData(q(*s)) : -1;
        if (s && index < 0) {
            // 선택지에서 빠진 값도 보여 주어 사용자가 고칠 수 있게 한다.
            combo_->addItem(QStringLiteral("⚠ 없는 값: %1").arg(q(*s)), q(*s));
            index = combo_->count() - 1;
        }
        combo_->setCurrentIndex(std::max(index, 0));
        layout->addWidget(combo_, 1);
        connect(combo_, &QComboBox::currentIndexChanged, this, &ValueEditor::edited);
        break;
    }
    case FieldType::Reference: {
        combo_ = new QComboBox(this);
        combo_->addItem(QStringLiteral("(없음)"), QVariant());
        const naru::DataType* target = database ? database->findType(field.refType) : nullptr;
        if (target) {
            for (const auto& r : target->records) {
                combo_->addItem(QStringLiteral("%1  (%2)").arg(q(r.name), q(r.id)), q(r.id));
            }
        }
        int index = 0;
        if (const auto* id = std::get_if<std::string>(&value)) {
            index = combo_->findData(q(*id));
            if (index < 0) {
                combo_->addItem(QStringLiteral("⚠ 없는 항목: %1").arg(q(*id)), q(*id));
                index = combo_->count() - 1;
            }
        }
        combo_->setCurrentIndex(index);
        if (!target) {
            combo_->setToolTip(QStringLiteral("참조할 데이터 종류가 정해지지 않았습니다. '속성 정의'에서 고르세요."));
        }
        layout->addWidget(combo_, 1);
        connect(combo_, &QComboBox::currentIndexChanged, this, &ValueEditor::edited);
        break;
    }
    }
}

Value ValueEditor::value() const {
    switch (field_.type) {
    case FieldType::Integer: return static_cast<std::int64_t>(integer_->value());
    case FieldType::Number: return number_->value();
    case FieldType::Text: return text_->text().toStdString();
    case FieldType::Boolean: return boolean_->isChecked();
    case FieldType::Choice:
        return combo_->currentData().isValid() ? Value{combo_->currentData().toString().toStdString()}
                                               : Value{std::string()};
    case FieldType::Reference:
        return combo_->currentData().isValid() ? Value{combo_->currentData().toString().toStdString()}
                                               : Value{std::monostate{}};
    }
    return {};
}
