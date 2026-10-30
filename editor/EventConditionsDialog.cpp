#include "EventConditionsDialog.h"

#include "CommandDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString valueText(const naru::Value& v) {
    if (const auto* b = std::get_if<bool>(&v)) {
        return *b ? QStringLiteral("참") : QStringLiteral("거짓");
    }
    if (const auto* s = std::get_if<std::string>(&v)) {
        return QStringLiteral("\"%1\"").arg(QString::fromStdString(*s));
    }
    return QString::fromStdString(naru::toDisplayString(v));
}

} // namespace

EventConditionsDialog::EventConditionsDialog(const std::vector<naru::EventCondition>& conditions,
                                             const naru::Project* project, QWidget* parent)
    : QDialog(parent), project_(project), conditions_(conditions) {
    setWindowTitle(QStringLiteral("이벤트 실행 조건"));
    resize(440, 300);

    auto* help = new QLabel(QStringLiteral("아래 조건이 <b>모두</b> 맞을 때만 이 이벤트를 실행합니다. "
                                           "조건이 없으면 언제나 실행합니다.<br>"
                                           "예: 스위치 <code>door_open == 참</code>, 변수 <code>gold &gt;= 100</code>"),
                            this);
    help->setWordWrap(true);
    list_ = new QListWidget(this);

    auto* addButton = new QPushButton(QStringLiteral("조건 추가"), this);
    auto* editButton = new QPushButton(QStringLiteral("편집"), this);
    auto* removeButton = new QPushButton(QStringLiteral("삭제"), this);
    auto* buttons = new QHBoxLayout;
    for (QPushButton* b : {addButton, editButton, removeButton}) {
        buttons->addWidget(b);
    }
    buttons->addStretch();

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(help);
    layout->addWidget(list_, 1);
    layout->addLayout(buttons);
    layout->addWidget(box);

    connect(addButton, &QPushButton::clicked, this, &EventConditionsDialog::addCondition);
    connect(editButton, &QPushButton::clicked, this, &EventConditionsDialog::editCurrent);
    connect(removeButton, &QPushButton::clicked, this, &EventConditionsDialog::removeCurrent);
    connect(list_, &QListWidget::itemDoubleClicked, this, &EventConditionsDialog::editCurrent);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh(0);
}

QString EventConditionsDialog::describe(const naru::EventCondition& c) {
    return QStringLiteral("%1 %2 %3")
        .arg(QString::fromStdString(c.variable), QString::fromLatin1(naru::toKey(c.compare)), valueText(c.value));
}

QString EventConditionsDialog::summary(const std::vector<naru::EventCondition>& conditions) {
    if (conditions.empty()) {
        return QStringLiteral("없음 (언제나)");
    }
    QStringList parts;
    for (const auto& c : conditions) {
        parts << describe(c);
    }
    return parts.join(QStringLiteral(" 그리고 "));
}

void EventConditionsDialog::refresh(int current) {
    list_->clear();
    for (const auto& c : conditions_) {
        list_->addItem(describe(c));
    }
    if (!conditions_.empty()) {
        list_->setCurrentRow(std::clamp(current, 0, static_cast<int>(conditions_.size()) - 1));
    }
}

bool EventConditionsDialog::editOne(naru::EventCondition& condition) {
    naru::EventCommand command;
    command.type = naru::EventCommandType::IfVariable;
    command.variable = condition.variable;
    command.compare = condition.compare;
    command.value = condition.value;
    while (true) {
        CommandDialog dialog(command, {}, project_, nullptr, this);
        dialog.setWindowTitle(QStringLiteral("실행 조건"));
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        command = dialog.command();
        if (!std::holds_alternative<std::monostate>(command.value)) {
            break;
        }
        QMessageBox::warning(this, windowTitle(), QStringLiteral("실행 조건에는 비교할 값을 정해야 합니다 (값 없음은 쓸 수 없음)."));
    }
    condition = {command.variable, command.compare, command.value};
    return true;
}

void EventConditionsDialog::addCondition() {
    naru::EventCondition condition;
    // 처음 정의한 변수를 미리 골라 둔다 (스위치면 == 참, 아니면 그 처음 값과 같음).
    if (project_ && !project_->variables.empty()) {
        const naru::VariableDef& first = project_->variables.front();
        condition.variable = first.name;
        condition.value = std::holds_alternative<bool>(first.initial) ? naru::Value{true} : first.initial;
    }
    if (editOne(condition)) {
        conditions_.push_back(condition);
        refresh(static_cast<int>(conditions_.size()) - 1);
    }
}

void EventConditionsDialog::editCurrent() {
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(conditions_.size())) {
        return;
    }
    naru::EventCondition condition = conditions_[static_cast<std::size_t>(row)];
    if (editOne(condition)) {
        conditions_[static_cast<std::size_t>(row)] = condition;
        refresh(row);
    }
}

void EventConditionsDialog::removeCurrent() {
    int row = list_->currentRow();
    if (row < 0 || row >= static_cast<int>(conditions_.size())) {
        return;
    }
    conditions_.erase(conditions_.begin() + row);
    refresh(row);
}
