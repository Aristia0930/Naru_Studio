#include "EventEditor.h"

#include "CommandDialog.h"
#include "ProjectDocument.h"
#include "UndoCommands.h"

#include "naru/events/EventCompiler.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/Session.h"

#include <QComboBox>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <functional>
#include <optional>

using naru::EventCommand;
using naru::EventCommandType;

namespace {

constexpr int kKindRole = Qt::UserRole;
constexpr int kListRole = Qt::UserRole + 1;
constexpr int kIndexRole = Qt::UserRole + 2;
constexpr int kIdRole = Qt::UserRole + 3;

enum ItemKind { CommandItem = 1, BranchItem = 2, InsertItem = 3 };

QString q(const std::string& s) {
    return QString::fromStdString(s);
}

QString valueText(const naru::Value& v) {
    if (const auto* s = std::get_if<std::string>(&v)) {
        return QStringLiteral("\"%1\"").arg(q(*s));
    }
    return q(naru::toDisplayString(v));
}

QString firstLine(const std::string& text) {
    QString t = q(text);
    int nl = t.indexOf('\n');
    return nl < 0 ? t : t.left(nl) + QStringLiteral(" …");
}

QString describe(const EventCommand& c, const naru::Scene* scene, const naru::Project* project) {
    switch (c.type) {
    case EventCommandType::Say:
        return QStringLiteral("대사: %1%2")
            .arg(c.speaker.empty() ? QString() : QStringLiteral("[%1] ").arg(q(c.speaker)), firstLine(c.text));
    case EventCommandType::Wait:
        return QStringLiteral("대기: %1초").arg(c.seconds);
    case EventCommandType::SetVariable: {
        static const char* ops[] = {"=", "+=", "-="};
        return QStringLiteral("변수: %1 %2 %3")
            .arg(q(c.variable), QString::fromLatin1(ops[static_cast<int>(c.op)]), valueText(c.value));
    }
    case EventCommandType::IfVariable:
        return QStringLiteral("조건 분기: %1 %2 %3").arg(q(c.variable), QString::fromLatin1(naru::toKey(c.compare)),
                                                         valueText(c.value));
    case EventCommandType::Log:
        return QStringLiteral("로그: %1").arg(q(c.text));
    case EventCommandType::CallScript:
        return c.variable.empty() ? QStringLiteral("스크립트 호출: %1 · %2()").arg(q(c.script), q(c.function))
                                  : QStringLiteral("스크립트 호출: %1 = %2 · %3()").arg(q(c.variable), q(c.script), q(c.function));
    case EventCommandType::Comment:
        return QStringLiteral("주석: %1").arg(q(c.text));
    case EventCommandType::GetData:
        return QStringLiteral("데이터 → 변수: %1 = %2[%3].%4").arg(q(c.variable), q(c.dataType), q(c.record), q(c.field));
    case EventCommandType::MoveObject:
        return QStringLiteral("객체 이동: %1 → %2 %3px (%4%5)")
            .arg(CommandDialog::targetLabel(c.target, scene), CommandDialog::directionLabel(c.direction))
            .arg(c.distance)
            .arg(c.speed > 0 ? QStringLiteral("속도 %1").arg(c.speed) : QStringLiteral("순간 이동"),
                 c.waitForEnd ? QStringLiteral(", 끝날 때까지 기다림") : QString());
    case EventCommandType::SetVisible:
        return QStringLiteral("%1: %2").arg(c.show ? QStringLiteral("보이기") : QStringLiteral("숨기기"),
                                            CommandDialog::targetLabel(c.target, scene));
    case EventCommandType::ChangeScene:
        return QStringLiteral("장면 이동: %1%2%3")
            .arg(CommandDialog::sceneLabel(c.scene, project),
                 c.setPosition ? QStringLiteral(" (%1, %2)").arg(c.x).arg(c.y) : QStringLiteral(" (놓인 위치)"),
                 c.direction.empty() ? QString() : QStringLiteral(", ") + CommandDialog::directionLabel(c.direction));
    }
    return {};
}

// "2/t/0/e" 같은 위치 문자열로 명령 목록을 찾는다. 잘못된 위치면 nullptr.
std::vector<EventCommand>* listAt(std::vector<EventCommand>& root, const std::string& prefix) {
    std::vector<EventCommand>* list = &root;
    if (prefix.empty()) {
        return list;
    }
    QStringList parts = q(prefix).split('/');
    if (parts.size() % 2 != 0) {
        return nullptr;
    }
    for (int i = 0; i < parts.size(); i += 2) {
        bool ok = false;
        int index = parts[i].toInt(&ok);
        if (!ok || index < 0 || index >= static_cast<int>(list->size())) {
            return nullptr;
        }
        EventCommand& c = (*list)[static_cast<std::size_t>(index)];
        if (c.type != EventCommandType::IfVariable) {
            return nullptr;
        }
        if (parts[i + 1] == QLatin1String("t")) {
            list = &c.thenCommands;
        } else if (parts[i + 1] == QLatin1String("e")) {
            list = &c.elseCommands;
        } else {
            return nullptr;
        }
    }
    return list;
}

std::optional<EventCommandType> chooseType(QWidget* parent) {
    QMenu menu(parent);
    for (auto type : {EventCommandType::Say, EventCommandType::Wait, EventCommandType::SetVariable,
                      EventCommandType::IfVariable, EventCommandType::GetData, EventCommandType::MoveObject,
                      EventCommandType::SetVisible, EventCommandType::ChangeScene, EventCommandType::CallScript,
                      EventCommandType::Log,
                      EventCommandType::Comment}) {
        menu.addAction(CommandDialog::typeLabel(type))->setData(static_cast<int>(type));
    }
    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen) {
        return std::nullopt;
    }
    return static_cast<EventCommandType>(chosen->data().toInt());
}

} // namespace

EventEditor::EventEditor(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent)
    : QWidget(parent), doc_(doc), python_(python) {
    title_ = new QLabel(this);
    trigger_ = new QComboBox(this);
    trigger_->addItems({QStringLiteral("조사 (결정 키·클릭)"), QStringLiteral("플레이어 접촉"), QStringLiteral("자동 실행 (장면 시작)"),
                        QStringLiteral("이벤트 접촉 (이 객체가 플레이어에게 닿을 때)"), QStringLiteral("병렬 처리 (다른 행동과 동시에 반복)")});
    trigger_->setToolTip(QStringLiteral("이벤트가 시작되는 조건입니다. 저장되며, 게임 실행 화면이 만들어지면 적용됩니다.\n"
                                        "지금은 아래 '테스트 실행'으로 바로 실행해 볼 수 있습니다."));

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setIndentation(24);
    tree_->setExpandsOnDoubleClick(false);

    auto* addButton = new QPushButton(QStringLiteral("명령 추가"), this);
    auto* editButton = new QPushButton(QStringLiteral("편집"), this);
    auto* removeButton = new QPushButton(QStringLiteral("삭제"), this);
    auto* upButton = new QPushButton(QStringLiteral("위로"), this);
    auto* downButton = new QPushButton(QStringLiteral("아래로"), this);
    runButton_ = new QPushButton(QStringLiteral("▶ 테스트 실행 (F6)"), this);
    stopButton_ = new QPushButton(QStringLiteral("■ 중지 (F8)"), this);
    stopButton_->setEnabled(false);

    dialogueBox_ = new QFrame(this);
    dialogueBox_->setFrameShape(QFrame::StyledPanel);
    dialogueBox_->setStyleSheet(QStringLiteral("QFrame { background: #1e2430; } QLabel { color: white; }"));
    dialogueSpeaker_ = new QLabel(dialogueBox_);
    dialogueSpeaker_->setStyleSheet(QStringLiteral("font-weight: bold; color: #ffd27a;"));
    dialogueText_ = new QLabel(dialogueBox_);
    dialogueText_->setWordWrap(true);
    dialogueText_->setMinimumHeight(48);
    auto* nextButton = new QPushButton(QStringLiteral("다음 ▶"), dialogueBox_);
    nextButton->setDefault(true);
    auto* dialogueLayout = new QVBoxLayout(dialogueBox_);
    dialogueLayout->addWidget(dialogueSpeaker_);
    dialogueLayout->addWidget(dialogueText_);
    dialogueLayout->addWidget(nextButton, 0, Qt::AlignRight);
    dialogueBox_->hide();

    auto* header = new QHBoxLayout;
    header->addWidget(title_, 1);
    header->addWidget(new QLabel(QStringLiteral("실행 조건"), this));
    header->addWidget(trigger_);

    auto* buttons = new QHBoxLayout;
    for (QPushButton* b : {addButton, editButton, removeButton, upButton, downButton}) {
        buttons->addWidget(b);
    }
    buttons->addStretch();
    buttons->addWidget(runButton_);
    buttons->addWidget(stopButton_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(tree_, 1);
    layout->addLayout(buttons);
    layout->addWidget(dialogueBox_);

    connect(addButton, &QPushButton::clicked, this, &EventEditor::addAtSelection);
    connect(editButton, &QPushButton::clicked, this, &EventEditor::editSelected);
    connect(removeButton, &QPushButton::clicked, this, &EventEditor::removeSelected);
    connect(upButton, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(runButton_, &QPushButton::clicked, this, &EventEditor::startTest);
    connect(stopButton_, &QPushButton::clicked, this, &EventEditor::stopTest);
    connect(nextButton, &QPushButton::clicked, this, &EventEditor::advanceDialogue);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (item->data(0, kKindRole).toInt() == CommandItem) {
            editSelected();
        } else {
            addCommandAt(item->data(0, kListRole).toString().toStdString(), item->data(0, kIndexRole).toInt());
        }
    });
    connect(trigger_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!loading_) {
            editEvent([index](naru::ObjectEvent& e) {
                e.trigger = static_cast<naru::EventTrigger>(index);
                return true;
            });
        }
    });

    connect(doc_, &ProjectDocument::objectChanged, this, [this](const QString& s, const QString& o) {
        if (s == sceneId_ && o == objectId_) {
            rebuild();
        }
    });
    connect(doc_, &ProjectDocument::sceneChanged, this, [this](const QString& s) {
        if (s == sceneId_) {
            rebuild();
        }
    });

    timer_.setInterval(16);
    connect(&timer_, &QTimer::timeout, this, &EventEditor::tick);
    rebuild();
}

EventEditor::~EventEditor() {
    timer_.stop();
    session_.reset();
}

void EventEditor::setTarget(const QString& sceneId, const QString& objectId) {
    if (sceneId == sceneId_ && objectId == objectId_) {
        return;
    }
    // 다른 객체로 바꾸면 실행 중이던 테스트를 멈춘다 (실행 위치 표시가 맞지 않게 되므로).
    stopTest();
    sceneId_ = sceneId;
    objectId_ = objectId;
    rebuild();
}

const naru::Scene* EventEditor::scene() const {
    return doc_->project().findScene(sceneId_.toStdString());
}

const naru::SceneObject* EventEditor::target() const {
    const naru::Scene* s = scene();
    return s ? s->findObject(objectId_.toStdString()) : nullptr;
}

QStringList EventEditor::scriptNames() const {
    QStringList names;
    for (const QFileInfo& info :
         QDir(doc_->scriptsDirectory()).entryInfoList({QStringLiteral("*.py")}, QDir::Files, QDir::Name)) {
        names << info.fileName();
    }
    return names;
}

// ---------------------------------------------------------------- 표시

void EventEditor::rebuild() {
    loading_ = true;
    std::string selectedList;
    int selectedIndex = -1;
    int selectedKind = 0;
    if (QTreeWidgetItem* current = tree_->currentItem()) {
        selectedList = current->data(0, kListRole).toString().toStdString();
        selectedIndex = current->data(0, kIndexRole).toInt();
        selectedKind = current->data(0, kKindRole).toInt();
    }
    tree_->clear();

    const naru::SceneObject* object = target();
    setEnabled(object != nullptr);
    if (!object) {
        title_->setText(QStringLiteral("장면에서 객체를 선택하면 그 객체의 이벤트를 편집합니다."));
        loading_ = false;
        return;
    }
    title_->setText(QStringLiteral("<b>%1</b> <span style='color:gray'>(%2)</span>").arg(q(object->name), q(object->id)));
    trigger_->setCurrentIndex(static_cast<int>(object->event.trigger));

    QTreeWidgetItem* toSelect = nullptr;
    auto remember = [&](QTreeWidgetItem* item, int kind, const std::string& list, int index) {
        if (kind == selectedKind && list == selectedList && index == selectedIndex) {
            toSelect = item;
        }
    };

    std::function<void(QTreeWidgetItem*, const std::vector<EventCommand>&, const std::string&)> addList =
        [&](QTreeWidgetItem* parent, const std::vector<EventCommand>& list, const std::string& prefix) {
            auto makeItem = [&]() { return parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_); };
            for (std::size_t i = 0; i < list.size(); ++i) {
                const EventCommand& c = list[i];
                std::string id = naru::commandId(prefix, i);
                QTreeWidgetItem* item = makeItem();
                item->setText(0, QStringLiteral("◆ ") + describe(c, scene(), &doc_->project()));
                item->setData(0, kKindRole, CommandItem);
                item->setData(0, kListRole, q(prefix));
                item->setData(0, kIndexRole, static_cast<int>(i));
                item->setData(0, kIdRole, q(id));
                if (c.type == EventCommandType::Comment) {
                    item->setForeground(0, QColor(60, 140, 60));
                } else if (c.type == EventCommandType::IfVariable) {
                    item->setForeground(0, QColor(30, 90, 190));
                }
                if (c.type == EventCommandType::Say && c.text.find('\n') != std::string::npos) {
                    item->setToolTip(0, q(c.text));
                }
                remember(item, CommandItem, prefix, static_cast<int>(i));
                if (c.type == EventCommandType::IfVariable) {
                    for (bool isThen : {true, false}) {
                        std::string branchPrefix = isThen ? naru::thenPrefix(id) : naru::elsePrefix(id);
                        auto* branch = new QTreeWidgetItem(item);
                        branch->setText(0, isThen ? QStringLiteral("▷ 그렇다면") : QStringLiteral("▷ 아니라면"));
                        branch->setForeground(0, QColor(30, 90, 190));
                        branch->setData(0, kKindRole, BranchItem);
                        branch->setData(0, kListRole, q(branchPrefix));
                        branch->setData(0, kIndexRole, 0);
                        remember(branch, BranchItem, branchPrefix, 0);
                        addList(branch, isThen ? c.thenCommands : c.elseCommands, branchPrefix);
                        branch->setExpanded(true);
                    }
                    item->setExpanded(true);
                }
            }
            // 목록 끝의 빈 줄: 더블클릭하면 여기에 명령을 추가한다.
            QTreeWidgetItem* insert = makeItem();
            insert->setText(0, QStringLiteral("◆"));
            insert->setForeground(0, QColor(150, 150, 150));
            insert->setToolTip(0, QStringLiteral("더블클릭하여 명령 추가"));
            insert->setData(0, kKindRole, InsertItem);
            insert->setData(0, kListRole, q(prefix));
            insert->setData(0, kIndexRole, static_cast<int>(list.size()));
            remember(insert, InsertItem, prefix, static_cast<int>(list.size()));
        };
    addList(nullptr, object->event.commands, std::string());

    if (toSelect) {
        tree_->setCurrentItem(toSelect);
    }
    if (!highlighted_.empty()) {
        highlightCommand(highlighted_);
    }
    loading_ = false;
}

QTreeWidgetItem* EventEditor::findCommandItem(const std::string& id) const {
    QString wanted = q(id);
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        if ((*it)->data(0, kKindRole).toInt() == CommandItem && (*it)->data(0, kIdRole).toString() == wanted) {
            return *it;
        }
    }
    return nullptr;
}

void EventEditor::highlightCommand(const std::string& id) {
    for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
        (*it)->setBackground(0, QBrush());
    }
    highlighted_ = id;
    if (QTreeWidgetItem* item = findCommandItem(id)) {
        item->setBackground(0, QColor(255, 236, 150));
        tree_->scrollToItem(item);
    }
}

// ---------------------------------------------------------------- 편집

bool EventEditor::editEvent(const std::function<bool(naru::ObjectEvent&)>& change) {
    const naru::SceneObject* current = target();
    if (!current) {
        return false;
    }
    naru::SceneObject after = *current;
    if (!change(after.event) || after == *current) {
        return false;
    }
    doc_->undoStack()->push(new SetObjectCommand(doc_, sceneId_.toStdString(), *current, after));
    return true;
}

bool EventEditor::insertCommand(const std::string& listPrefix, int index, const EventCommand& command) {
    return editEvent([&](naru::ObjectEvent& e) {
        auto* list = listAt(e.commands, listPrefix);
        if (!list || index < 0 || index > static_cast<int>(list->size())) {
            return false;
        }
        list->insert(list->begin() + index, command);
        return true;
    });
}

bool EventEditor::replaceCommand(const std::string& listPrefix, int index, const EventCommand& command) {
    return editEvent([&](naru::ObjectEvent& e) {
        auto* list = listAt(e.commands, listPrefix);
        if (!list || index < 0 || index >= static_cast<int>(list->size())) {
            return false;
        }
        (*list)[static_cast<std::size_t>(index)] = command;
        return true;
    });
}

bool EventEditor::removeCommand(const std::string& listPrefix, int index) {
    return editEvent([&](naru::ObjectEvent& e) {
        auto* list = listAt(e.commands, listPrefix);
        if (!list || index < 0 || index >= static_cast<int>(list->size())) {
            return false;
        }
        list->erase(list->begin() + index);
        return true;
    });
}

bool EventEditor::moveCommand(const std::string& listPrefix, int index, int delta) {
    return editEvent([&](naru::ObjectEvent& e) {
        auto* list = listAt(e.commands, listPrefix);
        int to = index + delta;
        if (!list || index < 0 || index >= static_cast<int>(list->size()) || to < 0 ||
            to >= static_cast<int>(list->size())) {
            return false;
        }
        std::swap((*list)[static_cast<std::size_t>(index)], (*list)[static_cast<std::size_t>(to)]);
        return true;
    });
}

bool EventEditor::selectedPosition(std::string& listPrefix, int& index) const {
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item) {
        return false;
    }
    listPrefix = item->data(0, kListRole).toString().toStdString();
    index = item->data(0, kIndexRole).toInt();
    return true;
}

void EventEditor::addCommandAt(const std::string& listPrefix, int index) {
    auto type = chooseType(this);
    if (!type) {
        return;
    }
    EventCommand command;
    command.type = *type;
    if (*type == EventCommandType::Wait) {
        command.seconds = 1.0;
    }
    if (*type == EventCommandType::ChangeScene) {
        command.direction.clear(); // 방향 그대로
    }
    if (*type == EventCommandType::IfVariable || *type == EventCommandType::SetVariable) {
        command.value = std::int64_t{0};
    }
    CommandDialog dialog(command, scriptNames(), &doc_->project(), scene(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (insertCommand(listPrefix, index, dialog.command())) {
        // 새로 넣은 명령을 선택한다.
        for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
            if ((*it)->data(0, kKindRole).toInt() == CommandItem &&
                (*it)->data(0, kListRole).toString().toStdString() == listPrefix &&
                (*it)->data(0, kIndexRole).toInt() == index) {
                tree_->setCurrentItem(*it);
                break;
            }
        }
    }
}

void EventEditor::addAtSelection() {
    std::string list;
    int index = 0;
    if (!selectedPosition(list, index)) {
        const naru::SceneObject* object = target();
        list.clear();
        index = object ? static_cast<int>(object->event.commands.size()) : 0;
    }
    // 선택한 명령 바로 위에 넣는다 (RPG Maker와 같은 방식).
    addCommandAt(list, index);
}

void EventEditor::editSelected() {
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item || item->data(0, kKindRole).toInt() != CommandItem) {
        return;
    }
    const naru::SceneObject* object = target();
    if (!object) {
        return;
    }
    std::string listPrefix = item->data(0, kListRole).toString().toStdString();
    int index = item->data(0, kIndexRole).toInt();
    naru::ObjectEvent copy = object->event;
    auto* list = listAt(copy.commands, listPrefix);
    if (!list || index >= static_cast<int>(list->size())) {
        return;
    }
    CommandDialog dialog((*list)[static_cast<std::size_t>(index)], scriptNames(), &doc_->project(), scene(), this);
    if (dialog.exec() == QDialog::Accepted) {
        replaceCommand(listPrefix, index, dialog.command());
    }
}

void EventEditor::removeSelected() {
    QTreeWidgetItem* item = tree_->currentItem();
    if (item && item->data(0, kKindRole).toInt() == CommandItem) {
        removeCommand(item->data(0, kListRole).toString().toStdString(), item->data(0, kIndexRole).toInt());
    }
}

void EventEditor::moveSelected(int delta) {
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item || item->data(0, kKindRole).toInt() != CommandItem) {
        return;
    }
    std::string list = item->data(0, kListRole).toString().toStdString();
    int index = item->data(0, kIndexRole).toInt();
    if (moveCommand(list, index, delta)) {
        for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
            if ((*it)->data(0, kKindRole).toInt() == CommandItem &&
                (*it)->data(0, kListRole).toString().toStdString() == list &&
                (*it)->data(0, kIndexRole).toInt() == index + delta) {
                tree_->setCurrentItem(*it);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- 테스트 실행

void EventEditor::startTest() {
    stopTest();
    const naru::SceneObject* object = target();
    if (!object) {
        return;
    }
    QString scriptsDir = doc_->scriptsDirectory();
    // 테스트 실행은 게임 밖이므로 전역 스크립트도 매번 새로 읽는다 (모듈 전역 변수는 유지되지 않고 ctx.player 등은 None).
    naru::ScriptRunner runner = [this, scriptsDir](const std::string& script, const std::string& function,
                                                   naru::CommandContext& ctx) -> naru::ScriptCallResult {
        QString name = q(script);
        if (name.contains('/') || name.contains('\\') || name.contains(QStringLiteral(".."))) {
            return {"scripts 폴더 밖의 파일은 호출할 수 없습니다: " + script, {}};
        }
        QString path = QDir(scriptsDir).filePath(name);
        if (!QFileInfo::exists(path)) {
            return {"스크립트 파일이 없습니다: scripts/" + script, {}};
        }
        naru::ScriptCallResult call;
        auto result = python_->runFile(QFileInfo(path).absoluteFilePath().toStdString(), ctx, function, &call.returned);
        if (!result.ok) {
            call.error = "Python 스크립트 오류 (" + result.error.type + ")";
        }
        return call;
    };

    // 실행 중 편집해도 테스트가 영향을 받지 않도록 데이터베이스 사본을 쓴다.
    runDatabase_ = doc_->project().database;
    session_ = std::make_unique<naru::Session>(doc_->project().initialVariables(), 1.0 / 60.0, this);
    session_->setDatabase(&runDatabase_);
    session_->logger().addSink([this](const naru::LogEntry& entry) {
        if (logSink_) {
            logSink_(entry);
        }
    });
    session_->logger().info("이벤트 테스트 시작: " + object->name);
    runButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    session_->start(naru::compileEvent(object->event.commands, runner));
    if (session_->state() != naru::SessionState::Running) {
        finishTest();
        return;
    }
    if (const naru::Command* current = session_->runner().current()) {
        highlightCommand(current->id());
    }
    clock_.restart();
    timer_.start();
}

void EventEditor::tick() {
    if (!session_) {
        timer_.stop();
        return;
    }
    session_->advance(clock_.restart() / 1000.0);
    if (session_->state() != naru::SessionState::Running) {
        finishTest();
        return;
    }
    if (const naru::Command* current = session_->runner().current()) {
        if (current->id() != highlighted_) {
            highlightCommand(current->id());
        }
    }
}

void EventEditor::stopTest() {
    if (session_) {
        session_->stop();
        finishTest();
    }
}

void EventEditor::finishTest() {
    timer_.stop();
    closeMessage();
    std::unique_ptr<naru::Session> session = std::move(session_);
    session->logger().info(std::string("이벤트 테스트 종료: ") + naru::toString(session->state()));
    if (variablesSink_) {
        variablesSink_(session->variables());
    }
    highlightCommand(std::string());
    if (session->state() == naru::SessionState::Failed) {
        if (QTreeWidgetItem* item = findCommandItem(session->runner().failedCommandId())) {
            tree_->setCurrentItem(item);
            item->setBackground(0, QColor(255, 190, 190));
        }
    }
    runButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

void EventEditor::showMessage(const std::string& speaker, const std::string& text) {
    dialogueSpeaker_->setText(q(speaker));
    dialogueSpeaker_->setVisible(!speaker.empty());
    dialogueText_->setText(q(text));
    dialogueBox_->show();
    messageOpen_ = true;
    if (auto* next = dialogueBox_->findChild<QPushButton*>()) {
        next->setFocus();
    }
}

void EventEditor::advanceDialogue() {
    closeMessage();
}

void EventEditor::closeMessage() {
    messageOpen_ = false;
    dialogueBox_->hide();
}
