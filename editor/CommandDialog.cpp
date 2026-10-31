#include "CommandDialog.h"

#include "VariablesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

using naru::CompareOp;
using naru::EventCommandType;
using naru::Value;
using naru::VariableOp;

namespace {

enum ValueKind { NumberKind, StringKind, BoolKind, NoneKind };

QDoubleSpinBox* coordinate(QWidget* parent) {
    auto* box = new QDoubleSpinBox(parent);
    box->setRange(-100000.0, 100000.0);
    box->setDecimals(0);
    return box;
}

int kindOf(const Value& v) {
    if (std::holds_alternative<std::string>(v)) return StringKind;
    if (std::holds_alternative<bool>(v)) return BoolKind;
    if (std::holds_alternative<std::monostate>(v)) return NoneKind;
    return NumberKind;
}

} // namespace

QString CommandDialog::typeLabel(EventCommandType type) {
    switch (type) {
    case EventCommandType::ShowChoices: return QStringLiteral("선택지 표시");
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
    case EventCommandType::ChangeScene: return QStringLiteral("장면 이동");
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

QString CommandDialog::sceneLabel(const std::string& sceneId, const naru::Project* project) {
    const naru::Scene* s = project ? project->findScene(sceneId) : nullptr;
    return s ? QString::fromStdString(s->name) : QStringLiteral("%1 (없는 장면)").arg(QString::fromStdString(sceneId));
}

QString CommandDialog::directionLabel(const std::string& direction) {
    if (direction == "up") return QStringLiteral("위");
    if (direction == "down") return QStringLiteral("아래");
    if (direction == "left") return QStringLiteral("왼쪽");
    if (direction == "right") return QStringLiteral("오른쪽");
    return QString::fromStdString(direction);
}

CommandDialog::CommandDialog(const naru::EventCommand& command, const QStringList& scripts,
                             const naru::Project* project, const naru::Scene* scene, QWidget* parent)
    : QDialog(parent), command_(command) {
    const naru::Database* database = project ? &project->database : nullptr;
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

    // 변수 칸: 정의한 변수·스위치를 고르거나 직접 입력한다. 아래 줄에 고른 변수의 종류·처음 값을 보여 준다.
    auto addVariableRow = [this, form, project](const QString& label, const QString& placeholder) {
        variable_ = new QComboBox(this);
        variable_->setEditable(true);
        variable_->setInsertPolicy(QComboBox::NoInsert);
        if (project) {
            for (const auto& v : project->variables) {
                variable_->addItem(QString::fromStdString(v.name));
            }
        }
        variable_->setCurrentText(QString::fromStdString(command_.variable));
        variable_->lineEdit()->setPlaceholderText(placeholder);
        auto* info = new QLabel(this);
        info->setStyleSheet(QStringLiteral("color: gray"));
        auto describe = [info, project](const QString& text) {
            QString name = text.trimmed();
            const naru::VariableDef* def = project ? project->findVariable(name.toStdString()) : nullptr;
            if (name.isEmpty()) {
                info->setText(QString());
            } else if (def) {
                QString value = std::holds_alternative<bool>(def->initial)
                                    ? (std::get<bool>(def->initial) ? QStringLiteral("참") : QStringLiteral("거짓"))
                                    : QString::fromStdString(naru::toDisplayString(def->initial));
                info->setText(QStringLiteral("%1 · 처음 값 %2%3")
                                  .arg(VariablesDialog::kindLabel(def->initial), value,
                                       def->description.empty() ? QString()
                                                                : QStringLiteral(" · ") + QString::fromStdString(def->description)));
            } else {
                info->setText(QStringLiteral("변수·스위치 목록에 없는 이름입니다 (처음에는 값이 없음)."));
            }
        };
        connect(variable_, &QComboBox::currentTextChanged, this, describe);
        describe(variable_->currentText());
        form->addRow(label, variable_);
        form->addRow(QString(), info);
    };
    // 목록에서 변수를 고르면 값 칸의 형식을 그 변수에 맞춘다 (스위치면 = 참).
    auto matchValueToVariable = [this, project] {
        connect(variable_, &QComboBox::activated, this, [this, project](int index) {
            const naru::VariableDef* def =
                project ? project->findVariable(variable_->itemText(index).toStdString()) : nullptr;
            if (!def || !valueType_) {
                return;
            }
            if (std::holds_alternative<bool>(def->initial)) {
                valueType_->setCurrentIndex(BoolKind);
                valueBool_->setCurrentIndex(0);
                op_->setCurrentIndex(0); // = 대입 / == 같다
            } else {
                valueType_->setCurrentIndex(std::holds_alternative<std::string>(def->initial) ? StringKind : NumberKind);
            }
        });
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
    case EventCommandType::ShowChoices: {
        speaker_ = new QLineEdit(QString::fromStdString(command.speaker), this);
        text_ = new QPlainTextEdit(QString::fromStdString(command.text), this);
        text_->setMaximumHeight(90);
        form->addRow(QStringLiteral("이름"), speaker_);
        form->addRow(QStringLiteral("질문 (생략 가능)"), text_);
        choices_ = new QListWidget(this);
        choices_->setObjectName(QStringLiteral("choiceOptions"));
        choices_->setToolTip(QStringLiteral("더블클릭하여 글자를 고칩니다. 순서를 바꾸면 분기 안의 명령도 함께 이동합니다."));
        if (command_.choices.empty()) command_.choices = {{"예", {}}, {"아니요", {}}};
        for (std::size_t i = 0; i < command_.choices.size(); ++i) {
            auto* item = new QListWidgetItem(QString::fromStdString(command_.choices[i].text), choices_);
            item->setData(Qt::UserRole, static_cast<int>(i));
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
        form->addRow(QStringLiteral("선택지 (1~6개)"), choices_);
        defaultChoice_ = new QComboBox(this);
        defaultChoice_->setObjectName(QStringLiteral("choiceDefault"));
        cancelChoice_ = new QComboBox(this);
        cancelChoice_->setObjectName(QStringLiteral("choiceCancel"));
        auto refresh = [this] {
            int initial = defaultChoice_->currentData().isValid() ? defaultChoice_->currentData().toInt() : command_.defaultChoice;
            int cancel = cancelChoice_->currentData().isValid() ? cancelChoice_->currentData().toInt() : command_.cancelChoice;
            defaultChoice_->clear();
            cancelChoice_->clear();
            cancelChoice_->addItem(QStringLiteral("취소 불가"), -1);
            for (int row = 0; row < choices_->count(); ++row) {
                const auto* item = choices_->item(row);
                QString label = QStringLiteral("%1. %2").arg(row + 1).arg(item->text());
                defaultChoice_->addItem(label, item->data(Qt::UserRole));
                cancelChoice_->addItem(label, item->data(Qt::UserRole));
            }
            defaultChoice_->setCurrentIndex(std::max(0, defaultChoice_->findData(initial)));
            cancelChoice_->setCurrentIndex(std::max(0, cancelChoice_->findData(cancel)));
            if (auto* add = findChild<QPushButton*>(QStringLiteral("choiceAdd"))) add->setEnabled(choices_->count() < naru::kMaxChoices);
            if (auto* remove = findChild<QPushButton*>(QStringLiteral("choiceRemove"))) remove->setEnabled(choices_->count() > 1);
        };
        refresh();
        auto* rowButtons = new QHBoxLayout;
        auto* add = new QPushButton(QStringLiteral("추가"), this);
        auto* remove = new QPushButton(QStringLiteral("삭제"), this);
        auto* up = new QPushButton(QStringLiteral("위로"), this);
        auto* down = new QPushButton(QStringLiteral("아래로"), this);
        up->setObjectName(QStringLiteral("choiceUp"));
        down->setObjectName(QStringLiteral("choiceDown"));
        add->setObjectName(QStringLiteral("choiceAdd"));
        remove->setObjectName(QStringLiteral("choiceRemove"));
        for (auto* button : {add, remove, up, down}) rowButtons->addWidget(button);
        form->addRow(rowButtons);
        form->addRow(QStringLiteral("처음 선택"), defaultChoice_);
        form->addRow(QStringLiteral("취소 키를 누르면"), cancelChoice_);
        form->addRow(new QLabel(QStringLiteral("확인 후 이벤트 목록의 각 선택지 아래에 실행할 명령을 추가하세요.\n"
                                              "선택지 삭제 시 그 안의 명령도 삭제됩니다 (Ctrl+Z로 복원)."), this));
        connect(choices_, &QListWidget::itemChanged, this, [refresh] { refresh(); });
        connect(add, &QPushButton::clicked, this, [this, refresh] {
            if (choices_->count() >= naru::kMaxChoices) return;
            int id = static_cast<int>(command_.choices.size());
            command_.choices.push_back({"선택지", {}});
            auto* item = new QListWidgetItem(QStringLiteral("선택지"), choices_);
            item->setData(Qt::UserRole, id);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            choices_->setCurrentItem(item);
            refresh();
        });
        connect(remove, &QPushButton::clicked, this, [this, refresh] {
            if (choices_->count() <= 1 || !choices_->currentItem()) return;
            const int id = choices_->currentItem()->data(Qt::UserRole).toInt();
            if (!command_.choices[static_cast<std::size_t>(id)].commands.empty() &&
                QMessageBox::question(this, QStringLiteral("선택지 삭제"), QStringLiteral("이 선택지 안의 명령도 삭제합니다. 계속할까요?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
            delete choices_->takeItem(choices_->currentRow());
            refresh();
        });
        auto move = [this, refresh](int delta) {
            int row = choices_->currentRow();
            if (row < 0 || row + delta < 0 || row + delta >= choices_->count()) return;
            auto* item = choices_->takeItem(row);
            choices_->insertItem(row + delta, item);
            choices_->setCurrentItem(item);
            refresh();
        };
        connect(up, &QPushButton::clicked, this, [move] { move(-1); });
        connect(down, &QPushButton::clicked, this, [move] { move(1); });
        choices_->setCurrentRow(0);
        refresh();
        break;
    }
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
        addVariableRow(QStringLiteral("변수 이름"), QString());
        op_ = new QComboBox(this);
        op_->addItems({QStringLiteral("= 대입"), QStringLiteral("+= 더하기"), QStringLiteral("-= 빼기")});
        op_->setCurrentIndex(static_cast<int>(command.op));
        form->addRow(QStringLiteral("연산"), op_);
        addValueRow(QStringLiteral("값"));
        matchValueToVariable();
        break;
    case EventCommandType::IfVariable:
        addVariableRow(QStringLiteral("변수 이름"), QString());
        op_ = new QComboBox(this);
        op_->addItems({QStringLiteral("== 같다"), QStringLiteral("!= 다르다"), QStringLiteral("< 작다"),
                       QStringLiteral("<= 작거나 같다"), QStringLiteral("> 크다"), QStringLiteral(">= 크거나 같다")});
        op_->setCurrentIndex(static_cast<int>(command.compare));
        form->addRow(QStringLiteral("비교"), op_);
        addValueRow(QStringLiteral("값"));
        matchValueToVariable();
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
        addVariableRow(QStringLiteral("반환값 → 변수"), QStringLiteral("비워 두면 반환값을 쓰지 않음"));
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
    case EventCommandType::ChangeScene: {
        scene_ = new QComboBox(this);
        if (project) {
            for (const auto& s : project->scenes) {
                scene_->addItem(QStringLiteral("%1  (%2)").arg(QString::fromStdString(s.name), QString::fromStdString(s.id)),
                                QString::fromStdString(s.id));
            }
        }
        if (!command.scene.empty() && scene_->findData(QString::fromStdString(command.scene)) < 0) {
            scene_->addItem(QStringLiteral("%1  (없는 장면)").arg(QString::fromStdString(command.scene)),
                            QString::fromStdString(command.scene));
        }
        scene_->setCurrentIndex(std::max(0, scene_->findData(QString::fromStdString(command.scene))));
        setPosition_ = new QCheckBox(QStringLiteral("도착 위치 지정 (끄면 그 장면에 놓인 위치 그대로)"), this);
        setPosition_->setChecked(command.setPosition);
        x_ = coordinate(this);
        y_ = coordinate(this);
        x_->setValue(command.x);
        y_->setValue(command.y);
        auto* fromPlayer = new QPushButton(QStringLiteral("플레이어 위치로"), this);
        fromPlayer->setToolTip(QStringLiteral("도착 장면에서 플레이어 객체가 놓인 위치를 넣습니다."));
        auto* position = new QHBoxLayout;
        position->addWidget(new QLabel(QStringLiteral("X"), this));
        position->addWidget(x_, 1);
        position->addWidget(new QLabel(QStringLiteral("Y"), this));
        position->addWidget(y_, 1);
        position->addWidget(fromPlayer);
        direction_ = new QComboBox(this);
        direction_->addItem(QStringLiteral("그대로"), QString());
        for (const char* key : {"up", "down", "left", "right"}) {
            direction_->addItem(directionLabel(key), QString::fromLatin1(key));
        }
        direction_->setCurrentIndex(std::max(0, direction_->findData(QString::fromStdString(command.direction))));
        form->addRow(QStringLiteral("장면"), scene_);
        form->addRow(QString(), setPosition_);
        form->addRow(QStringLiteral("도착 위치"), position);
        form->addRow(QStringLiteral("방향"), direction_);
        form->addRow(new QLabel(QStringLiteral("위치는 플레이어 객체의 왼쪽 위 좌표입니다 (인스펙터의 X, Y와 같음).\n"
                                               "장면이 바뀌면 이 이벤트의 남은 명령은 실행되지 않습니다.\n"
                                               "도착한 뒤 할 일은 새 장면의 자동 실행 이벤트에 넣으세요.\n"
                                               "F6 테스트 실행에서는 장면을 바꾸지 않고 로그만 남깁니다."),
                                this));
        auto fillFromPlayer = [this, project] {
            const naru::Scene* s = project ? project->findScene(scene_->currentData().toString().toStdString()) : nullptr;
            const naru::SceneObject* p = s ? s->findObject(s->playerObjectId) : nullptr;
            if (p) {
                x_->setValue(p->x);
                y_->setValue(p->y);
            }
        };
        connect(fromPlayer, &QPushButton::clicked, this, fillFromPlayer);
        auto sync = [this, fromPlayer] {
            x_->setEnabled(setPosition_->isChecked());
            y_->setEnabled(setPosition_->isChecked());
            fromPlayer->setEnabled(setPosition_->isChecked());
        };
        connect(setPosition_, &QCheckBox::toggled, this, sync);
        sync();
        if (command.scene.empty()) { // 새 명령: 첫 장면과 그 플레이어 위치로 시작
            fillFromPlayer();
        }
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
        form->addRow(QStringLiteral("데이터 종류"), dataType_);
        form->addRow(QStringLiteral("항목"), record_);
        form->addRow(QStringLiteral("속성"), field_);
        addVariableRow(QStringLiteral("넣을 변수"), QString());
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
    resize(command.type == EventCommandType::ShowChoices ? 620 : command.type == EventCommandType::Say ? 520 : command.type == EventCommandType::ChangeScene ? 600 : 440,
           sizeHint().height());
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
    case EventCommandType::ShowChoices: {
        c.speaker = speaker_->text().trimmed().toStdString();
        c.text = text_->toPlainText().toStdString();
        c.choices.clear();
        c.defaultChoice = 0;
        c.cancelChoice = -1;
        for (int row = 0; row < choices_->count(); ++row) {
            const auto* item = choices_->item(row);
            QString label = item->text().trimmed();
            if (label.isEmpty() || label.contains('\n') || label.contains('\r')) {
                QMessageBox::warning(this, windowTitle(), QStringLiteral("선택지 글자는 비어 있지 않은 한 줄이어야 합니다."));
                return;
            }
            int id = item->data(Qt::UserRole).toInt();
            auto branch = command_.choices[static_cast<std::size_t>(id)];
            branch.text = label.toStdString();
            c.choices.push_back(std::move(branch));
            if (id == defaultChoice_->currentData().toInt()) c.defaultChoice = row;
            if (id == cancelChoice_->currentData().toInt()) c.cancelChoice = row;
        }
        break;
    }
    case EventCommandType::Say:
        c.speaker = speaker_->text().trimmed().toStdString();
        c.text = text_->toPlainText().toStdString();
        break;
    case EventCommandType::Wait:
        c.seconds = seconds_->value();
        break;
    case EventCommandType::SetVariable:
    case EventCommandType::IfVariable: {
        QString name = variable_->currentText().trimmed();
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
        c.variable = variable_->currentText().trimmed().toStdString();
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
        c.variable = variable_->currentText().trimmed().toStdString();
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
    case EventCommandType::ChangeScene:
        c.scene = scene_->currentData().toString().toStdString();
        if (c.scene.empty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("이동할 장면을 고르세요."));
            return;
        }
        c.setPosition = setPosition_->isChecked();
        c.x = x_->value();
        c.y = y_->value();
        c.direction = direction_->currentData().toString().toStdString();
        break;
    }
    command_ = c;
    QDialog::accept();
}
