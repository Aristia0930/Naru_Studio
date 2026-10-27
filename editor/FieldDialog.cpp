#include "FieldDialog.h"

#include "ValueEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <algorithm>

using naru::FieldType;

namespace {

QString q(const std::string& s) {
    return QString::fromStdString(s);
}

QDoubleSpinBox* limitBox(QWidget* parent) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(-1e12, 1e12);
    box->setDecimals(3);
    return box;
}

} // namespace

FieldDialog::FieldDialog(const naru::FieldDef& field, const naru::DataType& owner, const naru::Database& database,
                         bool isNew, QWidget* parent)
    : QDialog(parent), field_(field), owner_(owner), database_(database), isNew_(isNew) {
    setWindowTitle(isNew ? QStringLiteral("속성 추가") : QStringLiteral("속성 편집"));

    name_ = new QLineEdit(q(field.name), this);
    key_ = new QLineEdit(q(field.key), this);
    key_->setEnabled(isNew);
    key_->setToolTip(QStringLiteral("스크립트에서 ctx.db.get(...)[\"키\"]로 읽을 때 쓰는 이름입니다. 영문·숫자·_ 로 씁니다.\n"
                                    "만든 뒤에는 바꿀 수 없습니다."));
    type_ = new QComboBox(this);
    for (auto t : {FieldType::Integer, FieldType::Number, FieldType::Text, FieldType::Boolean, FieldType::Choice,
                   FieldType::Reference}) {
        type_->addItem(ValueEditor::typeLabel(t), static_cast<int>(t));
    }
    type_->setCurrentIndex(type_->findData(static_cast<int>(field.type)));

    hasMin_ = new QCheckBox(QStringLiteral("최소"), this);
    min_ = limitBox(this);
    hasMax_ = new QCheckBox(QStringLiteral("최대"), this);
    max_ = limitBox(this);
    hasMin_->setChecked(field.min.has_value());
    min_->setValue(field.min.value_or(0));
    hasMax_->setChecked(field.max.has_value());
    max_->setValue(field.max.value_or(100));
    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(hasMin_);
    rangeRow->addWidget(min_);
    rangeRow->addWidget(hasMax_);
    rangeRow->addWidget(max_);

    choices_ = new QPlainTextEdit(this);
    choices_->setPlaceholderText(QStringLiteral("한 줄에 하나씩"));
    choices_->setMaximumHeight(90);
    QStringList choiceLines;
    for (const auto& c : field.choices) {
        choiceLines << q(c);
    }
    choices_->setPlainText(choiceLines.join('\n'));

    refType_ = new QComboBox(this);
    for (const auto& t : database.types) {
        refType_->addItem(QStringLiteral("%1  (%2)").arg(q(t.name), q(t.key)), q(t.key));
    }
    if (int i = refType_->findData(q(field.refType)); i >= 0) {
        refType_->setCurrentIndex(i);
    }

    defaultHolder_ = new QWidget(this);
    auto* holderLayout = new QHBoxLayout(defaultHolder_);
    holderLayout->setContentsMargins(0, 0, 0, 0);

    hint_ = new QLabel(this);
    hint_->setWordWrap(true);
    hint_->setStyleSheet(QStringLiteral("color: gray"));

    form_ = new QFormLayout;
    form_->addRow(QStringLiteral("표시 이름"), name_);
    form_->addRow(QStringLiteral("키 (영문)"), key_);
    form_->addRow(QStringLiteral("형식"), type_);
    form_->addRow(QStringLiteral("범위"), rangeRow);
    form_->addRow(QStringLiteral("선택지"), choices_);
    form_->addRow(QStringLiteral("참조할 데이터"), refType_);
    form_->addRow(QStringLiteral("기본값"), defaultHolder_);
    form_->addRow(hint_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FieldDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form_);
    layout->addWidget(buttons);

    auto refresh = [this] { rebuildDefaultEditor(); };
    connect(type_, &QComboBox::currentIndexChanged, this, refresh);
    connect(hasMin_, &QCheckBox::toggled, this, refresh);
    connect(hasMax_, &QCheckBox::toggled, this, refresh);
    connect(min_, &QDoubleSpinBox::editingFinished, this, refresh);
    connect(max_, &QDoubleSpinBox::editingFinished, this, refresh);
    connect(choices_, &QPlainTextEdit::textChanged, this, refresh);
    connect(refType_, &QComboBox::currentIndexChanged, this, refresh);
    rebuildDefaultEditor();
    resize(480, sizeHint().height());
}

naru::FieldDef FieldDialog::currentDefinition() const {
    naru::FieldDef f;
    f.key = key_->text().trimmed().toStdString();
    f.name = name_->text().trimmed().toStdString();
    f.type = static_cast<FieldType>(type_->currentData().toInt());
    if (f.type == FieldType::Integer || f.type == FieldType::Number) {
        if (hasMin_->isChecked()) {
            f.min = min_->value();
        }
        if (hasMax_->isChecked()) {
            f.max = max_->value();
        }
    }
    if (f.type == FieldType::Choice) {
        for (const QString& line : choices_->toPlainText().split('\n')) {
            QString c = line.trimmed();
            if (!c.isEmpty() && std::find(f.choices.begin(), f.choices.end(), c.toStdString()) == f.choices.end()) {
                f.choices.push_back(c.toStdString());
            }
        }
    }
    if (f.type == FieldType::Reference) {
        f.refType = refType_->currentData().toString().toStdString();
    }
    return f;
}

void FieldDialog::rebuildDefaultEditor() {
    naru::FieldDef f = currentDefinition();
    bool numeric = f.type == FieldType::Integer || f.type == FieldType::Number;
    form_->setRowVisible(3, numeric);
    form_->setRowVisible(4, f.type == FieldType::Choice);
    form_->setRowVisible(5, f.type == FieldType::Reference);
    min_->setEnabled(hasMin_->isChecked());
    max_->setEnabled(hasMax_->isChecked());

    // 기존 기본값을 새 형식으로 옮기고, 안 되면 빈 값으로 시작한다.
    naru::Value current = default_ ? default_->value() : field_.defaultValue;
    f.defaultValue = naru::convertValue(current, f).value_or(naru::emptyValueFor(f));
    if (naru::checkValue(f.defaultValue, f)) {
        f.defaultValue = naru::emptyValueFor(f);
    }
    delete default_;
    default_ = new ValueEditor(f, f.defaultValue, &database_, defaultHolder_);
    defaultHolder_->layout()->addWidget(default_);

    switch (f.type) {
    case FieldType::Integer: hint_->setText(QStringLiteral("예: 체력, 레벨, 가격")); break;
    case FieldType::Number: hint_->setText(QStringLiteral("소수점이 있는 수. 예: 이동 속도 1.5, 확률 0.25")); break;
    case FieldType::Text: hint_->setText(QStringLiteral("예: 설명, 그림 파일 이름")); break;
    case FieldType::Boolean: hint_->setText(QStringLiteral("예: 사용하면 사라짐, 보스 여부")); break;
    case FieldType::Choice: hint_->setText(QStringLiteral("정해진 것 중 하나. 예: 속성(불/물/풀), 분류")); break;
    case FieldType::Reference: hint_->setText(QStringLiteral("다른 데이터의 항목 하나를 가리킴. 예: 몬스터가 떨어뜨리는 아이템")); break;
    }
}

void FieldDialog::accept() {
    naru::FieldDef f = currentDefinition();
    if (f.name.empty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("표시 이름을 입력하세요."));
        return;
    }
    if (isNew_) {
        if (!naru::isValidFieldKey(f.key)) {
            QMessageBox::warning(this, windowTitle(),
                                 QStringLiteral("키는 영문·숫자·_ 로 쓰고 숫자로 시작할 수 없습니다. id, name은 쓸 수 없습니다."));
            return;
        }
        if (owner_.findField(f.key)) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("같은 키의 속성이 이미 있습니다: %1").arg(q(f.key)));
            return;
        }
    } else {
        f.key = field_.key;
    }
    if (f.type == FieldType::Choice && f.choices.empty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("선택지를 한 개 이상 입력하세요."));
        return;
    }
    if (f.type == FieldType::Reference && f.refType.empty()) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("참조할 데이터 종류를 고르세요."));
        return;
    }
    if (f.min && f.max && *f.min > *f.max) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("최소가 최대보다 큽니다."));
        return;
    }
    f.defaultValue = default_->value();
    field_ = f;
    QDialog::accept();
}
