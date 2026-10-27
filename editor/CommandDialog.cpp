#include "CommandDialog.h"

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

using naru::CompareOp;
using naru::EventCommandType;
using naru::Value;
using naru::VariableOp;

namespace {

enum ValueKind { NumberKind, StringKind, BoolKind, NoneKind };

int kindOf(const Value& v) {
    if (std::holds_alternative<std::string>(v)) return StringKind;
    if (std::holds_alternative<bool>(v)) return BoolKind;
    if (std::holds_alternative<std::monostate>(v)) return NoneKind;
    return NumberKind;
}

} // namespace

QString CommandDialog::typeLabel(EventCommandType type) {
    switch (type) {
    case EventCommandType::Say: return QStringLiteral("대사 표시");
    case EventCommandType::Wait: return QStringLiteral("대기");
    case EventCommandType::SetVariable: return QStringLiteral("변수 조작");
    case EventCommandType::IfVariable: return QStringLiteral("조건 분기 (변수)");
    case EventCommandType::Log: return QStringLiteral("로그 남기기");
    case EventCommandType::CallScript: return QStringLiteral("스크립트 호출");
    case EventCommandType::Comment: return QStringLiteral("주석");
    case EventCommandType::GetData: return QStringLiteral("데이터 → 변수");
    case EventCommandType::MoveObject: return QStringLiteral("객체 이동");
    case EventCommandType::SetVisible: return QStringLiteral("보이기/숨기기");
    }
    return {};
}

QString CommandDialog::targetLabel(const std::string& target, const naru::Scene* scene) {
    if (target == naru::kTargetSelf) {
        return QStringLiteral("이 객체");
    }
    if (target == naru::kTargetPlayer) {
        return QStringLiteral("플레이어");
    }
    const naru::SceneObject* o = scene ? scene->findObject(target) : nullptr;
    return o ? QString::fromStdString(o->name) : QStringLiteral("%1 (장면에 없음)").arg(QString::fromStdString(target));
}

QString CommandDialog::directionLabel(const std::string& direction) {
    if (direction == "up") return QStringLiteral("위");
    if (direction == "down") return QStringLiteral("아래");
    if (direction == "left") return QStringLiteral("왼쪽");
    if (direction == "right") return QStringLiteral("오른쪽");
    return QString::fromStdString(direction);
}

CommandDialog::CommandDialog(const naru::EventCommand& command, const QStringList& scripts,
                             const naru::Database* database, const naru::Scene* scene, QWidget* parent)
    : QDialog(parent), command_(command) {
    setWindowTitle(typeLabel(command.type));
    auto* form = new QFormLayout;

    auto addValueRow = [this, form](const QString& label) {
        valueType_ = new QComboBox(this);
        valueType_->addItems({QStringLiteral("숫자"), QStringLiteral("문자열"), QStringLiteral("참/거짓"),
                              QStringLiteral("없음")});
        valueEdit_ = new QLineEdit(this);
        valueBool_ = new QComboBox(this);
        valueBool_->addItems({QStringLiteral("참 (True)"), QStringLiteral("거짓 (False)")});
        auto* row = new QHBoxLayout;
        row->addWidget(valueType_);
        row->addWidget(valueEdit_, 1);
        row->addWidget(valueBool_, 1);
        form->addRow(label, row);

        const Value& v = command_.value;
        valueType_->setCurrentIndex(kindOf(v));
        if (const auto* b = std::get_if<bool>(&v)) {
            valueBool_->setCurrentIndex(*b ? 0 : 1);
        } else if (!std::holds_alternative<std::monostate>(v)) {
            valueEdit_->setText(QString::fromStdString(naru::toDisplayString(v)));
        }
        auto sync = [this] {
            int kind = valueType_->currentIndex();
            valueEdit_->setVisible(kind == NumberKind || kind == StringKind);
            valueBool_->setVisible(kind == BoolKind);
        };
        connect(valueType_, &QComboBox::currentIndexChanged, this, sync);
        sync();
    };

    auto addTargetRow = [this, form, scene] {
        target_ = new QComboBox(this);
        target_->addItem(QStringLiteral("이 이벤트의 객체"), QString::fromLatin1(naru::kTargetSelf));
        target_->addItem(QStringLiteral("플레이어"), QString::fromLatin1(naru::kTargetPlayer));
        if (scene) {
            for (const auto& o : scene->objects) {
                target_->addItem(QStringLiteral("%1  (%2)").arg(QString::fromStdString(o.name), QString::fromStdString(o.id)),
                                 QString::fromStdString(o.id));
            }
        }
        QString current = QString::fromStdString(command_.target);
        int i = target_->findData(current);
        if (i < 0) { // 지워진 객체: 바꾸지 않고 알아볼 수 있게 남긴다
            target_->addItem(QStringLiteral("%1  (장면에 없음)").arg(current), current);
            i = target_->count() - 1;
        }
        target_->setCurrentIndex(i);
        form->addRow(QStringLiteral("대상"), target_);
    };

    switch (command.type) {
    case EventCommandType::Say:
        speaker_ = new QLineEdit(QString::fromStdString(command.speaker), this);
        speaker_->setPlaceholderText(QStringLiteral("비우면 이름 없이 표시"));
        text_ = new QPlainTextEdit(QString::fromStdString(command.text), this);
        form->addRow(QStringLiteral("이름"), speaker_);
        form->addRow(QStringLiteral("대사"), text_);
        break;
    case EventCommandType::Wait:
        seconds_ = new QDoubleSpinBox(this);
        seconds_->setRange(0.0, 3600.0);
        seconds_->setDecimals(2);
        seconds_->setSingleStep(0.1);
        seconds_->setSuffix(QStringLiteral(" 초"));
        seconds_->setValue(command.seconds);
        form->addRow(QStringLiteral("시간"), seconds_);
        break;
    case EventCommandType::SetVariable:
        variable_ = new QLineEdit(QString::fromStdString(command.variable), this);
        op_ = new QComboBox(this);
        op_->addItems({QStringLiteral("= 대입"), QStringLiteral("+= 더하기"), QStringLiteral("-= 빼기")});
        op_->setCurrentIndex(static_cast<int>(command.op));
        form->addRow(QStringLiteral("변수 이름"), variable_);
        form->addRow(QStringLiteral("연산"), op_);
        addValueRow(QStringLiteral("값"));
        break;
    case EventCommandType::IfVariable:
        variable_ = new QLineEdit(QString::fromStdString(command.variable), this);
        op_ = new QComboBox(this);
        op_->addItems({QStringLiteral("== 같다"), QStringLiteral("!= 다르다"), QStringLiteral("< 작다"),
                       QStringLiteral("<= 작거나 같다"), QStringLiteral("> 크다"), QStringLiteral(">= 크거나 같다")});
        op_->setCurrentIndex(static_cast<int>(command.compare));
        form->addRow(QStringLiteral("변수 이름"), variable_);
        form->addRow(QStringLiteral("비교"), op_);
        addValueRow(QStringLiteral("값"));
        form->addRow(new QLabel(QStringLiteral("값이 없는 변수는 숫자와 비교할 때 0으로 봅니다."), this));
        break;
    case EventCommandType::Log:
    case EventCommandType::Comment:
        line_ = new QLineEdit(QString::fromStdString(command.text), this);
        form->addRow(command.type == EventCommandType::Log ? QStringLiteral("메시지") : QStringLiteral("내용"), line_);
        break;
    case EventCommandType::CallScript:
        script_ = new QComboBox(this);
        script_->setEditable(true);
        script_->addItems(scripts);
        script_->setCurrentText(QString::fromStdString(command.script));
        function_ = new QLineEdit(QString::fromStdString(command.function.empty() ? "run" : command.function), this);
        form->addRow(QStringLiteral("스크립트 (scripts/)"), script_);
        form->addRow(QStringLiteral("함수"), function_);
        variable_ = new QLineEdit(QString::fromStdString(command.variable), this);
        variable_->setPlaceholderText(QStringLiteral("비워 두면 반환값을 쓰지 않음"));
        form->addRow(QStringLiteral("반환값 → 변수"), variable_);
        form->addRow(new QLabel(QStringLiteral("함수는 def 함수이름(ctx): 형태여야 합니다.\n"
                                               "return 값(None·bool·int·float·str)을 변수에 넣어 조건 분기에서 쓸 수 있습니다.\n"
                                               "전역 스크립트로 등록한 파일이면 게임 중 실행 중인 그 모듈의 함수가 불립니다."),
                                this));
        break;
    case EventCommandType::MoveObject: {
        addTargetRow();
        direction_ = new QComboBox(this);
        for (const char* key : {"up", "down", "left", "right"}) {
            direction_->addItem(directionLabel(key), QString::fromLatin1(key));
        }
        direction_->setCurrentIndex(std::max(0, direction_->findData(QString::fromStdString(command.direction))));
        distance_ = new QDoubleSpinBox(this);
        distance_->setRange(0.0, 100000.0);
        distance_->setDecimals(0);
        distance_->setSuffix(QStringLiteral(" px"));
        distance_->setValue(command.distance);
        speed_ = new QDoubleSpinBox(this);
        speed_->setRange(0.0, 100000.0);
        speed_->setDecimals(0);
        speed_->setSingleStep(10.0);
        speed_->setSuffix(QStringLiteral(" px/초"));
        speed_->setSpecialValueText(QStringLiteral("순간 이동"));
        speed_->setValue(command.speed);
        waitForEnd_ = new QCheckBox(QStringLiteral("이동이 끝날 때까지 다음 명령을 기다림"), this);
        waitForEnd_->setChecked(command.waitForEnd);
        form->addRow(QStringLiteral("방향"), direction_);
        form->addRow(QStringLiteral("거리"), distance_);
        form->addRow(QStringLiteral("속도"), speed_);
        form->addRow(QString(), waitForEnd_);
        form->addRow(new QLabel(QStringLiteral("플레이어와 같은 충돌 규칙으로 움직이며, 막히면 그 자리에서 멈춥니다.\n"
                                               "기본 속도 240은 플레이어 이동 속도와 같습니다. 0이면 순간 이동합니다.\n"
                                               "F6 테스트 실행에서는 움직이지 않고 로그만 남깁니다 (게임 실행 Ctrl+R에서 확인)."),
                                this));
        break;
    }
    case EventCommandType::SetVisible:
        addTargetRow();
        show_ = new QComboBox(this);
        show_->addItems({QStringLiteral("보이기"), QStringLiteral("숨기기")});
        show_->setCurrentIndex(command.show ? 0 : 1);
        form->addRow(QStringLiteral("동작"), show_);
        form->addRow(new QLabel(QStringLiteral("숨긴 객체는 그리지 않고, 막지 않으며, 조사·접촉 이벤트도 실행되지 않습니다.\n"
                                               "자동 실행·병렬 처리 이벤트는 숨겨도 계속 실행됩니다."),
                                this));
        break;
    case EventCommandType::GetData: {
        // 종류 → 항목 → 속성 순으로 고른다. 직접 입력도 된다 (나중에 만들 항목 등).
        dataType_ = new QComboBox(this);
        record_ = new QComboBox(this);
        field_ = new QComboBox(this);
        for (QComboBox* box : {dataType_, record_, field_}) {
            box->setEditable(true);
        }
        // 연결된 람다가 나중에도 쓰므로 포인터를 값으로 잡는다 (프로젝트 데이터는 대화상자보다 오래 산다).
        static const naru::Database empty;
        const naru::Database* dbp = database ? database : &empty;
        for (const auto& t : dbp->types) {
            dataType_->addItem(QStringLiteral("%1  (%2)").arg(QString::fromStdString(t.name), QString::fromStdString(t.key)),
                               QString::fromStdString(t.key));
        }
        auto keyOf = [](QComboBox* box) {
            int i = box->findText(box->currentText());
            return (i >= 0 && box->itemData(i).isValid() ? box->itemData(i).toString() : box->currentText()).trimmed();
        };
        auto fillDependent = [this, dbp, keyOf](const std::string& record, const std::string& field) {
            const naru::DataType* t = dbp->findType(keyOf(dataType_).toStdString());
            record_->clear();
            field_->clear();
            field_->addItem(QStringLiteral("이름  (name)"), QStringLiteral("name"));
            field_->addItem(QStringLiteral("ID  (id)"), QStringLiteral("id"));
            if (t) {
                for (const auto& r : t->records) {
                    record_->addItem(QStringLiteral("%1  (%2)").arg(QString::fromStdString(r.name), QString::fromStdString(r.id)),
                                     QString::fromStdString(r.id));
                }
                for (const auto& f : t->fields) {
                    field_->addItem(QStringLiteral("%1  (%2)").arg(QString::fromStdString(f.name), QString::fromStdString(f.key)),
                                    QString::fromStdString(f.key));
                }
            }
            auto pick = [](QComboBox* box, const std::string& key) {
                if (key.empty()) {
                    return;
                }
                int i = box->findData(QString::fromStdString(key));
                if (i >= 0) {
                    box->setCurrentIndex(i);
                } else {
                    box->setEditText(QString::fromStdString(key));
                }
            };
            pick(record_, record);
            pick(field_, field);
        };
        if (int i = dataType_->findData(QString::fromStdString(command.dataType)); i >= 0) {
            dataType_->setCurrentIndex(i);
        } else if (!command.dataType.empty()) {
            dataType_->setEditText(QString::fromStdString(command.dataType));
        }
        fillDependent(command.record, command.field.empty() ? std::string("name") : command.field);
        connect(dataType_, &QComboBox::currentIndexChanged, this, [fillDependent] { fillDependent({}, "name"); });
        variable_ = new QLineEdit(QString::fromStdString(command.variable), this);
        form->addRow(QStringLiteral("데이터 종류"), dataType_);
        form->addRow(QStringLiteral("항목"), record_);
        form->addRow(QStringLiteral("속성"), field_);
        form->addRow(QStringLiteral("넣을 변수"), variable_);
        form->addRow(new QLabel(QStringLiteral("예: 몬스터 slime의 hp를 변수 enemy_hp에 넣기"), this));
        break;
    }
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CommandDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(command.type == EventCommandType::Say ? 520 : 440, sizeHint().height());
}

bool CommandDialog::readValue(QComboBox* typeBox, QLineEdit* edit, QComboBox* boolBox, Value& out) {
    switch (typeBox->currentIndex()) {
    case NumberKind: {
        QString text = edit->text().trimmed();
        bool ok = false;
        qlonglong i = text.toLongLong(&ok);
        if (ok) {
            out = static_cast<std::int64_t>(i);
            return true;
        }
        double d = text.toDouble(&ok);
        if (ok) {
            out = d;
            return true;
        }
        QMessageBox::warning(this, windowTitle(), QStringLiteral("숫자를 입력하세요: %1").arg(text));
        return false;
    }
    case StringKind: out = edit->text().toStdString(); return true;
    case BoolKind: out = boolBox->currentIndex() == 0; return true;
    default: out = std::monostate{}; return true;
    }
}

void CommandDialog::accept() {
    naru::EventCommand c = command_;
    switch (c.type) {
    case EventCommandType::Say:
        c.speaker = speaker_->text().trimmed().toStdString();
        c.text = text_->toPlainText().toStdString();
        break;
    case EventCommandType::Wait:
        c.seconds = seconds_->value();
        break;
    case EventCommandType::SetVariable:
    case EventCommandType::IfVariable: {
        QString name = variable_->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("변수 이름을 입력하세요."));
            return;
        }
        c.variable = name.toStdString();
        if (c.type == EventCommandType::SetVariable) {
            c.op = static_cast<VariableOp>(op_->currentIndex());
        } else {
            c.compare = static_cast<CompareOp>(op_->currentIndex());
        }
        if (!readValue(valueType_, valueEdit_, valueBool_, c.value)) {
            return;
        }
        break;
    }
    case EventCommandType::Log:
    case EventCommandType::Comment:
        c.text = line_->text().toStdString();
        break;
    case EventCommandType::CallScript: {
        QString script = script_->currentText().trimmed();
        if (script.isEmpty() || script.contains('/') || script.contains('\\')) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("scripts 폴더 안의 파일 이름을 고르세요."));
            return;
        }
        c.script = script.toStdString();
        c.function = function_->text().trimmed().isEmpty() ? "run" : function_->text().trimmed().toStdString();
        c.variable = variable_->text().trimmed().toStdString();
        break;
    }
    case EventCommandType::GetData: {
        auto keyOf = [](QComboBox* box) {
            int i = box->findText(box->currentText());
            return (i >= 0 && box->itemData(i).isValid() ? box->itemData(i).toString() : box->currentText()).trimmed();
        };
        c.dataType = keyOf(dataType_).toStdString();
        c.record = keyOf(record_).toStdString();
        c.field = keyOf(field_).toStdString();
        c.variable = variable_->text().trimmed().toStdString();
        if (c.dataType.empty() || c.record.empty() || c.field.empty() || c.variable.empty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("데이터 종류·항목·속성·변수를 모두 정하세요."));
            return;
        }
        break;
    }
    case EventCommandType::MoveObject:
        c.target = target_->currentData().toString().toStdString();
        c.direction = direction_->currentData().toString().toStdString();
        c.distance = distance_->value();
        c.speed = speed_->value();
        c.waitForEnd = waitForEnd_->isChecked();
        break;
    case EventCommandType::SetVisible:
        c.target = target_->currentData().toString().toStdString();
        c.show = show_->currentIndex() == 0;
        break;
    }
    command_ = c;
    QDialog::accept();
}
