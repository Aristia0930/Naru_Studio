#include "CommonEventsEditor.h"
#include "EventEditor.h"
#include "ProjectDocument.h"
#include "UndoCommands.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QShortcut>
#include <algorithm>

CommonEventsEditor::CommonEventsEditor(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent)
    : QWidget(parent), doc_(doc) {
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("여러 장면에서 함께 쓸 명령을 만듭니다. 객체 이벤트의 ‘공통 이벤트 호출’ 또는 Python ctx.run_event(\"ID\")로 실행하세요."), this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto* split = new QSplitter(this);
    auto* panel = new QWidget(split);
    auto* left = new QVBoxLayout(panel);
    list_ = new QListWidget(panel);
    list_->setObjectName(QStringLiteral("commonEventsList"));
    left->addWidget(list_, 1);
    auto button = [&](const QString& title, auto action) {
        auto* result = new QPushButton(title, panel);
        left->addWidget(result);
        connect(result, &QPushButton::clicked, this, action);
    };
    button(QStringLiteral("추가"), [this] {
        bool ok = false;
        auto name = QInputDialog::getText(this, QStringLiteral("공통 이벤트 추가"), QStringLiteral("이름"), QLineEdit::Normal, {}, &ok);
        if (ok) addEvent(name);
    });
    button(QStringLiteral("이름 변경"), [this] {
        auto* event = doc_->project().findCommonEvent(selectedId().toStdString());
        if (!event) return;
        bool ok = false;
        auto name = QInputDialog::getText(this, QStringLiteral("공통 이벤트 이름"), QStringLiteral("이름 (ID는 유지됩니다)"), QLineEdit::Normal, QString::fromStdString(event->name), &ok);
        if (ok) renameEvent(name);
    });
    button(QStringLiteral("삭제"), [this] {
        if (selectedId().isEmpty()) return;
        if (QMessageBox::question(this, QStringLiteral("공통 이벤트 삭제"),
            QStringLiteral("이 이벤트를 삭제할까요? 이 ID를 호출하는 명령·스크립트는 실행할 때 오류가 납니다. Undo로 복구할 수 있습니다.")) == QMessageBox::Yes) removeEvent();
    });
    button(QStringLiteral("위로"), [this] { moveEvent(-1); });
    button(QStringLiteral("아래로"), [this] { moveEvent(1); });
    editor_ = new EventEditor(doc, python, split);
    editor_->setCommonTarget(QStringLiteral("@none"));
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({220, 900});
    layout->addWidget(split, 1);
    connect(list_, &QListWidget::currentRowChanged, this, [this] {
        auto id = selectedId();
        editor_->setCommonTarget(id.isEmpty() ? QStringLiteral("@none") : id);
    });
    connect(doc_, &ProjectDocument::commonEventsChanged, this, &CommonEventsEditor::rebuild);
    rebuild();
}

QString CommonEventsEditor::selectedId() const {
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString() : QString();
}
void CommonEventsEditor::rebuild() {
    auto id = selectedId();
    {
        QSignalBlocker blocker(list_);
        list_->clear();
        for (const auto& event : doc_->project().commonEvents) {
            auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(QString::fromStdString(event.name), QString::fromStdString(event.id)), list_);
            item->setData(Qt::UserRole, QString::fromStdString(event.id));
        }
    }
    selectEvent(id);
}
void CommonEventsEditor::selectEvent(const QString& id) {
    int row = list_->count() ? 0 : -1;
    for (int i = 0; i < list_->count(); ++i) if (list_->item(i)->data(Qt::UserRole).toString() == id) row = i;
    list_->setCurrentRow(row);
    editor_->setCommonTarget(selectedId().isEmpty() ? QStringLiteral("@none") : selectedId());
}
QString CommonEventsEditor::addEvent(const QString& name) {
    if (name.trimmed().isEmpty()) return {};
    auto events = doc_->project().commonEvents;
    int n = 1;
    std::string id;
    do { id = "common_" + std::to_string(n++); } while (doc_->project().findCommonEvent(id));
    events.push_back({id, name.trimmed().toStdString(), {}});
    doc_->undoStack()->push(new SetCommonEventsCommand(doc_, std::move(events)));
    selectEvent(QString::fromStdString(id));
    return QString::fromStdString(id);
}
bool CommonEventsEditor::renameEvent(const QString& name) {
    auto id = selectedId().toStdString();
    if (name.trimmed().isEmpty() || !doc_->project().findCommonEvent(id)) return false;
    auto events = doc_->project().commonEvents;
    for (auto& event : events) if (event.id == id) event.name = name.trimmed().toStdString();
    doc_->undoStack()->push(new SetCommonEventsCommand(doc_, std::move(events)));
    return true;
}
bool CommonEventsEditor::removeEvent() {
    auto id = selectedId().toStdString();
    if (!doc_->project().findCommonEvent(id)) return false;
    editor_->stopTest();
    auto events = doc_->project().commonEvents;
    std::erase_if(events, [&](const auto& event) { return event.id == id; });
    doc_->undoStack()->push(new SetCommonEventsCommand(doc_, std::move(events)));
    return true;
}
bool CommonEventsEditor::moveEvent(int delta) {
    int row = list_->currentRow();
    auto events = doc_->project().commonEvents;
    int to = row + delta;
    if (row < 0 || to < 0 || to >= static_cast<int>(events.size())) return false;
    std::swap(events[static_cast<std::size_t>(row)], events[static_cast<std::size_t>(to)]);
    doc_->undoStack()->push(new SetCommonEventsCommand(doc_, std::move(events)));
    return true;
}
