#include "MainWindow.h"

#include "EventEditor.h"
#include "Inspector.h"
#include "ProjectDocument.h"
#include "SceneView.h"
#include "ScriptEditor.h"
#include "UndoCommands.h"

#include "naru/events/BuiltinCommands.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/Session.h"

#include <QAction>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLineEdit>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

constexpr int kKindRole = Qt::UserRole;
constexpr int kSceneRole = Qt::UserRole + 1;
constexpr int kObjectRole = Qt::UserRole + 2;
constexpr int kFileRole = Qt::UserRole + 3;
constexpr int kLineRole = Qt::UserRole + 4;

enum ItemKind { SceneItem = 1, ObjectItem = 2 };

const char* kNewScriptTemplate =
    "def run(ctx):\n"
    "    ctx.log(\"%1 실행\")\n";

QString levelLabel(naru::LogLevel level) {
    switch (level) {
    case naru::LogLevel::Info: return QStringLiteral("정보");
    case naru::LogLevel::Warning: return QStringLiteral("경고");
    case naru::LogLevel::Error: return QStringLiteral("오류");
    }
    return {};
}

QString q(const std::string& s) {
    return QString::fromStdString(s);
}

} // namespace

MainWindow::MainWindow(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent)
    : QMainWindow(parent), doc_(doc), python_(python) {
    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setDocumentMode(true);
    sceneView_ = new SceneView(doc_, this);
    tabs_->addTab(sceneView_, QStringLiteral("장면"));
    eventEditor_ = new EventEditor(doc_, python_, this);
    tabs_->addTab(eventEditor_, QStringLiteral("이벤트"));
    // 장면·이벤트 탭은 닫을 수 없다. 스크립트 탭은 그 뒤에 열린다.
    for (int i = 0; i < 2; ++i) {
        tabs_->tabBar()->setTabButton(i, QTabBar::RightSide, nullptr);
        tabs_->tabBar()->setTabButton(i, QTabBar::LeftSide, nullptr);
    }
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    createDocks();
    createActions();

    eventEditor_->setLogSink([this](const naru::LogEntry& entry) { appendLog(entry); });
    eventEditor_->setVariablesSink([this](const naru::Variables& v) { showVariables(v); });

    connect(sceneView_, &SceneView::selectionChangedTo, this, &MainWindow::onCanvasSelection);
    connect(sceneView_, &SceneView::objectMoved, this, &MainWindow::onObjectMoved);
    connect(sceneView_, &SceneView::objectActivated, this, &MainWindow::openEventTab);
    connect(doc_, &ProjectDocument::scenesChanged, this, &MainWindow::rebuildTree);
    connect(doc_, &ProjectDocument::sceneChanged, this, &MainWindow::rebuildTree);
    connect(doc_, &ProjectDocument::objectChanged, this, &MainWindow::rebuildTree);
    connect(doc_, &ProjectDocument::modifiedChanged, this, &MainWindow::updateTitle);

    statusBar()->showMessage(QDir::toNativeSeparators(doc_->directory()));

    rebuildTree();
    rebuildScriptList();
    QString start = q(doc_->project().startSceneId);
    if (start.isEmpty() && !doc_->project().scenes.empty()) {
        start = q(doc_->project().scenes.front().id);
    }
    showScene(start);
    updateTitle();
    resize(1400, 860);
}

void MainWindow::createDocks() {
    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelection);
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        if (item->data(0, kKindRole).toInt() == ObjectItem) {
            openEventTab();
        }
    });
    auto* treeDock = new QDockWidget(QStringLiteral("장면"), this);
    treeDock->setObjectName(QStringLiteral("sceneTreeDock"));
    treeDock->setWidget(tree_);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);

    scripts_ = new QListWidget(this);
    connect(scripts_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) { openScript(item->data(kFileRole).toString()); });
    auto* scriptDock = new QDockWidget(QStringLiteral("스크립트 (더블클릭하여 열기)"), this);
    scriptDock->setObjectName(QStringLiteral("scriptDock"));
    scriptDock->setWidget(scripts_);
    addDockWidget(Qt::LeftDockWidgetArea, scriptDock);

    inspector_ = new Inspector(doc_, this);
    auto* inspectorDock = new QDockWidget(QStringLiteral("인스펙터"), this);
    inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    inspectorDock->setWidget(inspector_);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    variablesTable_ = new QTableWidget(0, 2, this);
    variablesTable_->setHorizontalHeaderLabels({QStringLiteral("변수"), QStringLiteral("값")});
    variablesTable_->horizontalHeader()->setStretchLastSection(true);
    variablesTable_->verticalHeader()->hide();
    variablesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* varDock = new QDockWidget(QStringLiteral("실행 결과 변수"), this);
    varDock->setObjectName(QStringLiteral("variablesDock"));
    varDock->setWidget(variablesTable_);
    addDockWidget(Qt::RightDockWidgetArea, varDock);
    tabifyDockWidget(inspectorDock, varDock);
    inspectorDock->raise();

    log_ = new QTreeWidget(this);
    log_->setColumnCount(3);
    log_->setHeaderLabels({QStringLiteral("수준"), QStringLiteral("메시지"), QStringLiteral("위치")});
    log_->setRootIsDecorated(false);
    log_->header()->setStretchLastSection(false);
    log_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    log_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    log_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connect(log_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) { jumpToLog(item); });
    auto* logDock = new QDockWidget(QStringLiteral("로그"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logDock->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
}

void MainWindow::createActions() {
    auto make = [this](const QString& text, const QKeySequence& key, auto slot) {
        auto* action = new QAction(text, this);
        if (!key.isEmpty()) {
            action->setShortcut(key);
        }
        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    auto* saveAction = make(QStringLiteral("저장"), QKeySequence::Save, &MainWindow::saveAll);
    auto* closeProject = make(QStringLiteral("프로젝트 닫기"), {}, [this] {
        returnToStart_ = true;
        if (!close()) {
            returnToStart_ = false;
        }
    });
    auto* openFolder = make(QStringLiteral("프로젝트 폴더 열기"), {}, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(doc_->directory()));
    });
    auto* quit = make(QStringLiteral("종료"), QKeySequence::Quit, &QWidget::close);

    QAction* undo = doc_->undoStack()->createUndoAction(this, QStringLiteral("실행 취소"));
    undo->setShortcut(QKeySequence::Undo);
    QAction* redo = doc_->undoStack()->createRedoAction(this, QStringLiteral("다시 실행"));
    redo->setShortcut(QKeySequence::Redo);
    deleteAction_ = make(QStringLiteral("삭제"), QKeySequence::Delete, &MainWindow::deleteSelection);
    // 스크립트 편집 중 Delete 키는 글자 삭제로 써야 하므로 장면 관련 위젯에서만 동작하게 한다.
    deleteAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    sceneView_->addAction(deleteAction_);
    tree_->addAction(deleteAction_);

    auto* addScene = make(QStringLiteral("새 장면"), {}, &MainWindow::addScene);
    addObjectAction_ = make(QStringLiteral("객체 추가"), QKeySequence(Qt::CTRL | Qt::Key_N), &MainWindow::addObject);
    auto* startScene = make(QStringLiteral("시작 장면으로 지정"), {}, &MainWindow::setStartScene);
    auto* removeScene = make(QStringLiteral("장면 삭제"), {}, &MainWindow::removeCurrentScene);

    auto* newScript = make(QStringLiteral("새 스크립트..."), {}, &MainWindow::newScript);
    auto* check = make(QStringLiteral("문법 검사"), QKeySequence(Qt::Key_F7), &MainWindow::checkSyntax);
    auto* run = make(QStringLiteral("스크립트 실행"), QKeySequence(Qt::Key_F5), &MainWindow::runScript);
    run->setToolTip(QStringLiteral("열린 스크립트 탭을, 장면 탭에서는 scripts/main.py를 실행합니다 (F5)"));

    auto* eventTest = make(QStringLiteral("이벤트 테스트 실행"), QKeySequence(Qt::Key_F6), [this] {
        openEventTab();
        eventEditor_->startTest();
    });
    auto* eventStop = make(QStringLiteral("이벤트 테스트 중지"), QKeySequence(Qt::Key_F8),
                           [this] { eventEditor_->stopTest(); });
    auto* eventOpen = make(QStringLiteral("선택한 객체의 이벤트 편집"), QKeySequence(Qt::Key_Return),
                           &MainWindow::openEventTab);
    eventOpen->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    sceneView_->addAction(eventOpen);

    auto* file = menuBar()->addMenu(QStringLiteral("파일(&F)"));
    file->addAction(saveAction);
    file->addSeparator();
    file->addAction(openFolder);
    file->addAction(closeProject);
    file->addSeparator();
    file->addAction(quit);

    auto* edit = menuBar()->addMenu(QStringLiteral("편집(&E)"));
    edit->addAction(undo);
    edit->addAction(redo);
    edit->addSeparator();
    edit->addAction(deleteAction_);

    auto* scene = menuBar()->addMenu(QStringLiteral("장면(&S)"));
    scene->addAction(addScene);
    scene->addAction(addObjectAction_);
    scene->addSeparator();
    scene->addAction(startScene);
    scene->addAction(removeScene);

    auto* events = menuBar()->addMenu(QStringLiteral("이벤트(&V)"));
    events->addAction(eventOpen);
    events->addAction(eventTest);
    events->addAction(eventStop);

    auto* script = menuBar()->addMenu(QStringLiteral("스크립트(&P)"));
    script->addAction(newScript);
    script->addAction(check);
    script->addAction(run);

    auto* toolbar = addToolBar(QStringLiteral("기본"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(saveAction);
    toolbar->addAction(undo);
    toolbar->addAction(redo);
    toolbar->addSeparator();
    toolbar->addAction(addScene);
    toolbar->addAction(addObjectAction_);
    toolbar->addAction(deleteAction_);
    toolbar->addSeparator();
    toolbar->addAction(newScript);
    toolbar->addAction(check);
    toolbar->addAction(run);
}

// ---------------------------------------------------------------- 장면 트리

void MainWindow::rebuildTree() {
    // 실행 취소 등으로 보고 있던 장면이 사라지면 시작 장면으로 돌아간다.
    if (!doc_->project().findScene(currentSceneId_.toStdString()) && !doc_->project().scenes.empty()) {
        const auto& p = doc_->project();
        currentSceneId_ = q(p.findScene(p.startSceneId) ? p.startSceneId : p.scenes.front().id);
        sceneView_->setSceneId(currentSceneId_);
        setSelectionTarget(currentSceneId_, QString());
    }
    syncing_ = true;
    QString selectedScene = currentSceneId_;
    QString selectedObject = sceneView_->selectedObjectId();
    tree_->clear();
    const auto& project = doc_->project();
    QTreeWidgetItem* toSelect = nullptr;
    for (const auto& scene : project.scenes) {
        auto* sceneItem = new QTreeWidgetItem(tree_);
        bool isStart = scene.id == project.startSceneId;
        sceneItem->setText(0, (isStart ? QStringLiteral("▶ ") : QString()) + q(scene.name));
        sceneItem->setToolTip(0, isStart ? QStringLiteral("시작 장면 · %1").arg(q(scene.id)) : q(scene.id));
        sceneItem->setData(0, kKindRole, SceneItem);
        sceneItem->setData(0, kSceneRole, q(scene.id));
        for (const auto& object : scene.objects) {
            auto* objectItem = new QTreeWidgetItem(sceneItem);
            objectItem->setText(0, q(object.name));
            objectItem->setToolTip(0, q(object.id));
            objectItem->setData(0, kKindRole, ObjectItem);
            objectItem->setData(0, kSceneRole, q(scene.id));
            objectItem->setData(0, kObjectRole, q(object.id));
            if (q(scene.id) == selectedScene && q(object.id) == selectedObject) {
                toSelect = objectItem;
            }
        }
        if (q(scene.id) == selectedScene) {
            sceneItem->setExpanded(true);
            if (!toSelect && selectedObject.isEmpty()) {
                toSelect = sceneItem;
            }
        }
    }
    if (toSelect) {
        tree_->setCurrentItem(toSelect);
    }
    syncing_ = false;
    addObjectAction_->setEnabled(!currentSceneId_.isEmpty());
    if (auto* sceneInfo = project.findScene(currentSceneId_.toStdString())) {
        tabs_->setTabText(0, QStringLiteral("장면: %1").arg(q(sceneInfo->name)));
    }
}

void MainWindow::showScene(const QString& sceneId) {
    currentSceneId_ = sceneId;
    sceneView_->setSceneId(sceneId);
    setSelectionTarget(sceneId, sceneView_->selectedObjectId());
    rebuildTree();
}

void MainWindow::onTreeSelection() {
    if (syncing_) {
        return;
    }
    QTreeWidgetItem* item = tree_->currentItem();
    if (!item) {
        return;
    }
    QString sceneId = item->data(0, kSceneRole).toString();
    QString objectId = item->data(0, kObjectRole).toString();
    syncing_ = true;
    if (sceneId != currentSceneId_) {
        currentSceneId_ = sceneId;
        sceneView_->setSceneId(sceneId);
        if (auto* s = doc_->project().findScene(sceneId.toStdString())) {
            tabs_->setTabText(0, QStringLiteral("장면: %1").arg(q(s->name)));
        }
    }
    sceneView_->selectObject(objectId);
    syncing_ = false;
    setSelectionTarget(sceneId, objectId);
    addObjectAction_->setEnabled(!currentSceneId_.isEmpty());
    // 스크립트 탭을 보고 있었다면 장면으로 돌아간다. 이벤트 탭은 그대로 두어 다른 객체의 이벤트를 바로 본다.
    if (tabs_->currentIndex() > 1) {
        tabs_->setCurrentIndex(0);
    }
}

void MainWindow::onCanvasSelection(const QString& objectId) {
    if (syncing_) {
        return;
    }
    setSelectionTarget(currentSceneId_, objectId);
    syncing_ = true;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* sceneItem = tree_->topLevelItem(i);
        if (sceneItem->data(0, kSceneRole).toString() != currentSceneId_) {
            continue;
        }
        QTreeWidgetItem* target = sceneItem;
        for (int j = 0; j < sceneItem->childCount(); ++j) {
            if (sceneItem->child(j)->data(0, kObjectRole).toString() == objectId) {
                target = sceneItem->child(j);
            }
        }
        tree_->setCurrentItem(target);
    }
    syncing_ = false;
}

void MainWindow::onObjectMoved(const QString& objectId, double x, double y) {
    const naru::Scene* scene = doc_->project().findScene(currentSceneId_.toStdString());
    const naru::SceneObject* current = scene ? scene->findObject(objectId.toStdString()) : nullptr;
    if (!current) {
        return;
    }
    naru::SceneObject after = *current;
    after.x = x;
    after.y = y;
    if (after != *current) {
        doc_->undoStack()->push(new SetObjectCommand(doc_, currentSceneId_.toStdString(), *current, after));
    }
}

void MainWindow::setSelectionTarget(const QString& sceneId, const QString& objectId) {
    inspector_->setTarget(sceneId, objectId);
    eventEditor_->setTarget(sceneId, objectId);
}

void MainWindow::openEventTab() {
    tabs_->setCurrentWidget(eventEditor_);
}

void MainWindow::showObjectEvent(const QString& objectId) {
    sceneView_->selectObject(objectId);
    onCanvasSelection(objectId);
    openEventTab();
}

void MainWindow::startEventTest() {
    openEventTab();
    eventEditor_->startTest();
}

// ---------------------------------------------------------------- 장면/객체 편집

void MainWindow::addScene() {
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("새 장면"), QStringLiteral("장면 이름"),
                                         QLineEdit::Normal, QStringLiteral("새 장면"), &ok)
                       .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    naru::Scene scene;
    scene.id = doc_->project().newId("scene");
    scene.name = name.toStdString();
    doc_->undoStack()->push(new AddSceneCommand(doc_, scene, static_cast<int>(doc_->project().scenes.size())));
    showScene(q(scene.id));
}

void MainWindow::removeCurrentScene() {
    if (currentSceneId_.isEmpty()) {
        return;
    }
    if (doc_->project().scenes.size() <= 1) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("장면이 하나뿐이라 삭제할 수 없습니다."));
        return;
    }
    QString removed = currentSceneId_;
    doc_->undoStack()->push(new RemoveSceneCommand(doc_, removed.toStdString()));
    showScene(q(doc_->project().scenes.front().id));
}

void MainWindow::setStartScene() {
    if (!currentSceneId_.isEmpty() && currentSceneId_.toStdString() != doc_->project().startSceneId) {
        doc_->undoStack()->push(new SetStartSceneCommand(doc_, currentSceneId_.toStdString()));
    }
}

void MainWindow::addObject() {
    const naru::Scene* scene = doc_->project().findScene(currentSceneId_.toStdString());
    if (!scene) {
        return;
    }
    naru::SceneObject object;
    object.id = doc_->project().newId("obj");
    object.name = QStringLiteral("객체 %1").arg(scene->objects.size() + 1).toStdString();
    // 보이는 화면 가운데, 장면 범위 안에 놓는다.
    QPointF center = sceneView_->visibleCenter();
    object.x = std::clamp(std::round(center.x() - object.width / 2), 0.0, std::max(0.0, scene->width - object.width));
    object.y = std::clamp(std::round(center.y() - object.height / 2), 0.0, std::max(0.0, scene->height - object.height));
    doc_->undoStack()->push(new AddObjectCommand(doc_, scene->id, object, static_cast<int>(scene->objects.size())));
    sceneView_->selectObject(q(object.id));
    onCanvasSelection(q(object.id));
}

void MainWindow::deleteSelection() {
    QString objectId = sceneView_->selectedObjectId();
    if (objectId.isEmpty()) {
        return;
    }
    doc_->undoStack()->push(new RemoveObjectCommand(doc_, currentSceneId_.toStdString(), objectId.toStdString()));
    setSelectionTarget(currentSceneId_, QString());
}

// ---------------------------------------------------------------- 스크립트

void MainWindow::rebuildScriptList() {
    scripts_->clear();
    QDir dir(doc_->scriptsDirectory());
    for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*.py")}, QDir::Files, QDir::Name)) {
        auto* item = new QListWidgetItem(info.fileName(), scripts_);
        item->setData(kFileRole, info.absoluteFilePath());
    }
}

void MainWindow::newScript() {
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("새 스크립트"), QStringLiteral("파일 이름 (.py)"),
                                         QLineEdit::Normal, QStringLiteral("new_script.py"), &ok)
                       .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    if (!name.endsWith(QStringLiteral(".py"))) {
        name += QStringLiteral(".py");
    }
    if (name.contains('/') || name.contains('\\')) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("폴더 구분 문자는 쓸 수 없습니다."));
        return;
    }
    QDir().mkpath(doc_->scriptsDirectory());
    QString path = QDir(doc_->scriptsDirectory()).filePath(name);
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("같은 이름의 스크립트가 있습니다."));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, windowTitle(), file.errorString());
        return;
    }
    file.write(QString::fromUtf8(kNewScriptTemplate).arg(QFileInfo(name).completeBaseName()).toUtf8());
    file.close();
    rebuildScriptList();
    openScript(path);
}

ScriptEditor* MainWindow::openScript(const QString& path) {
    QString absolute = QFileInfo(path).absoluteFilePath();
    for (int i = 2; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (editor && editor->path() == absolute) {
            tabs_->setCurrentIndex(i);
            return editor;
        }
    }
    auto* editor = new ScriptEditor(this);
    QString error;
    if (!editor->load(absolute, &error)) {
        delete editor;
        QMessageBox::warning(this, windowTitle(), error);
        return nullptr;
    }
    int index = tabs_->addTab(editor, QFileInfo(absolute).fileName());
    tabs_->setTabToolTip(index, QDir::toNativeSeparators(absolute));
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, editor](bool modified) {
        int i = tabs_->indexOf(editor);
        if (i >= 0) {
            tabs_->setTabText(i, QFileInfo(editor->path()).fileName() + (modified ? QStringLiteral(" *") : QString()));
        }
        updateTitle();
    });
    tabs_->setCurrentIndex(index);
    return editor;
}

ScriptEditor* MainWindow::currentScript() const {
    return qobject_cast<ScriptEditor*>(tabs_->currentWidget());
}

void MainWindow::closeTab(int index) {
    auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(index));
    if (!editor) {
        return;
    }
    if (editor->isModified()) {
        auto answer = QMessageBox::question(
            this, windowTitle(), QStringLiteral("%1을(를) 저장할까요?").arg(QFileInfo(editor->path()).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (answer == QMessageBox::Cancel) {
            return;
        }
        QString error;
        if (answer == QMessageBox::Save && !editor->save(&error)) {
            QMessageBox::warning(this, windowTitle(), error);
            return;
        }
    }
    tabs_->removeTab(index);
    editor->deleteLater();
    updateTitle();
}

bool MainWindow::saveAll() {
    QString error;
    if (doc_->isModified() && !doc_->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
        return false;
    }
    for (int i = 2; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (editor && editor->isModified() && !editor->save(&error)) {
            QMessageBox::warning(this, QStringLiteral("저장 실패"), editor->path() + QStringLiteral("\n") + error);
            return false;
        }
    }
    statusBar()->showMessage(QStringLiteral("저장했습니다"), 3000);
    updateTitle();
    return true;
}

void MainWindow::checkSyntax() {
    ScriptEditor* editor = currentScript();
    if (!editor) {
        statusBar()->showMessage(QStringLiteral("문법 검사할 스크립트 탭을 여세요"), 3000);
        return;
    }
    QString error;
    if (editor->isModified() && !editor->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
        return;
    }
    auto result = python_->checkSyntax(editor->path().toStdString());
    if (result.ok) {
        appendLog({naru::LogLevel::Info, "문법 검사 통과", {editor->path().toStdString(), 0, ""}, ""});
    } else {
        appendLog({naru::LogLevel::Error, result.error.type + ": " + result.error.message,
                   {result.error.file, result.error.line, ""}, result.error.traceback});
        editor->goToLine(result.error.line);
    }
}

void MainWindow::runScript() {
    // 스크립트 탭이면 그 파일, 장면 탭이면 scripts/main.py를 실행한다.
    QString path;
    if (ScriptEditor* editor = currentScript()) {
        QString error;
        if (editor->isModified() && !editor->save(&error)) {
            QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
            return;
        }
        path = editor->path();
    } else {
        path = QDir(doc_->scriptsDirectory()).filePath(QStringLiteral("main.py"));
        if (!QFileInfo::exists(path)) {
            statusBar()->showMessage(QStringLiteral("scripts/main.py가 없습니다. 실행할 스크립트 탭을 여세요"), 4000);
            return;
        }
    }
    const std::string pathUtf8 = path.toStdString();

    naru::Session session(naru::Variables{});
    session.logger().addSink([this](const naru::LogEntry& entry) { appendLog(entry); });
    session.logger().info("실행 시작: " + QFileInfo(path).fileName().toStdString());

    std::vector<std::unique_ptr<naru::Command>> commands;
    commands.push_back(std::make_unique<naru::CallCommand>(
        "script", [this, pathUtf8](naru::CommandContext& ctx) -> std::string {
            auto result = python_->runFile(pathUtf8, ctx);
            return result.ok ? std::string() : "Python 스크립트 오류 (" + result.error.type + ")";
        }));
    session.start(std::move(commands));
    // 현재 명령은 즉시 끝난다. 대기형 명령과 실시간 루프·중지는 다음 단계에서 연결한다.
    session.logger().info(std::string("실행 종료: ") + naru::toString(session.state()));
    showVariables(session.variables());

    if (session.state() == naru::SessionState::Failed) {
        const auto& entries = session.logger().entries();
        for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
            if (it->source.line > 0 && !it->source.file.empty()) {
                if (ScriptEditor* editor = openScript(q(it->source.file))) {
                    editor->goToLine(it->source.line);
                }
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- 로그·상태

void MainWindow::appendLog(const naru::LogEntry& entry) {
    logMessages_ << q(entry.message);
    auto* item = new QTreeWidgetItem(log_);
    item->setText(0, levelLabel(entry.level));
    item->setText(1, q(entry.message));
    QString location;
    if (!entry.source.file.empty()) {
        location = QFileInfo(q(entry.source.file)).fileName();
        if (entry.source.line > 0) {
            location += QStringLiteral(":%1").arg(entry.source.line);
        }
    }
    if (!entry.source.commandId.empty()) {
        location += (location.isEmpty() ? QString() : QStringLiteral(" ")) +
                    QStringLiteral("[%1]").arg(q(entry.source.commandId));
    }
    item->setText(2, location);
    item->setData(0, kFileRole, q(entry.source.file));
    item->setData(0, kLineRole, entry.source.line);
    if (!entry.detail.empty()) {
        item->setToolTip(1, q(entry.detail));
    }
    QColor color = entry.level == naru::LogLevel::Error     ? QColor(200, 40, 40)
                   : entry.level == naru::LogLevel::Warning ? QColor(180, 120, 0)
                                                            : QColor();
    if (color.isValid()) {
        for (int c = 0; c < 3; ++c) {
            item->setForeground(c, color);
        }
    }
    log_->scrollToItem(item);
}

void MainWindow::showVariables(const naru::Variables& variables) {
    variablesTable_->setRowCount(0);
    for (const auto& [name, value] : variables.all()) {
        int row = variablesTable_->rowCount();
        variablesTable_->insertRow(row);
        variablesTable_->setItem(row, 0, new QTableWidgetItem(q(name)));
        variablesTable_->setItem(row, 1, new QTableWidgetItem(q(naru::toDisplayString(value))));
    }
}

void MainWindow::jumpToLog(QTreeWidgetItem* item) {
    QString file = item->data(0, kFileRole).toString();
    int line = item->data(0, kLineRole).toInt();
    if (file.isEmpty() || !QFileInfo::exists(file)) {
        return;
    }
    if (ScriptEditor* editor = openScript(file)) {
        editor->goToLine(line);
    }
}

bool MainWindow::hasUnsavedChanges() const {
    if (doc_->isModified()) {
        return true;
    }
    for (int i = 2; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (editor && editor->isModified()) {
            return true;
        }
    }
    return false;
}

void MainWindow::updateTitle() {
    setWindowTitle(QStringLiteral("%1%2 — Naru Studio")
                       .arg(doc_->name(), hasUnsavedChanges() ? QStringLiteral(" *") : QString()));
}

bool MainWindow::confirmClose() {
    if (!hasUnsavedChanges()) {
        return true;
    }
    auto answer = QMessageBox::question(this, QStringLiteral("변경 사항"),
                                        QStringLiteral("저장하지 않은 변경 사항이 있습니다. 저장할까요?"),
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        return saveAll();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmClose()) {
        event->accept();
    } else {
        event->ignore();
    }
}

// ---------------------------------------------------------------- 자동 검사

bool MainWindow::runSelfTest() {
    int failures = 0;
    auto check = [&failures](bool ok, const char* what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
        std::fflush(stdout);
        if (!ok) {
            ++failures;
        }
    };
    auto objectCount = [this] {
        const naru::Scene* s = doc_->project().findScene(currentSceneId_.toStdString());
        return s ? s->objects.size() : std::size_t{0};
    };

    check(!currentSceneId_.isEmpty(), "프로젝트를 열면 시작 장면이 표시된다");
    std::size_t before = objectCount();
    addObjectAction_->trigger();
    check(objectCount() == before + 1, "객체 추가");
    QString added = sceneView_->selectedObjectId();
    check(!added.isEmpty(), "추가한 객체가 선택된다");

    onObjectMoved(added, 100, 200);
    const naru::SceneObject* moved =
        doc_->project().findScene(currentSceneId_.toStdString())->findObject(added.toStdString());
    check(moved && moved->x == 100 && moved->y == 200, "객체 이동");
    check(doc_->isModified(), "변경 후 저장 필요 상태");

    doc_->undoStack()->undo();
    moved = doc_->project().findScene(currentSceneId_.toStdString())->findObject(added.toStdString());
    check(moved && !(moved->x == 100 && moved->y == 200), "이동 실행 취소");
    doc_->undoStack()->redo();

    check(saveAll() && !doc_->isModified(), "저장");
    QString error;
    auto reloaded = ProjectDocument::open(doc_->directory(), &error);
    check(reloaded && reloaded->project() == doc_->project(), "저장한 프로젝트를 다시 열면 같은 내용");

    // 다음 실행을 위해 추가한 객체를 지우고 저장한다.
    sceneView_->selectObject(added);
    deleteSelection();
    check(objectCount() == before, "객체 삭제");
    saveAll();

    // 이벤트 편집기: 명령 추가(중첩 분기 포함) → Undo/Redo → 테스트 실행(대사 넘기기) → 저장·재로드
    {
        const naru::Scene* scene = doc_->project().findScene(currentSceneId_.toStdString());
        QString target = q(scene->objects.front().id);
        sceneView_->selectObject(target);
        onCanvasSelection(target);
        openEventTab();
        naru::ObjectEvent original = scene->objects.front().event;

        naru::EventCommand sayCmd;
        sayCmd.type = naru::EventCommandType::Say;
        sayCmd.speaker = "촌장";
        sayCmd.text = "어서 오게";
        naru::EventCommand addCmd;
        addCmd.type = naru::EventCommandType::SetVariable;
        addCmd.variable = "visits";
        addCmd.op = naru::VariableOp::Add;
        addCmd.value = std::int64_t{1};
        naru::EventCommand ifCmd;
        ifCmd.type = naru::EventCommandType::IfVariable;
        ifCmd.variable = "visits";
        ifCmd.value = std::int64_t{1};
        naru::EventCommand logCmd;
        logCmd.type = naru::EventCommandType::Log;
        logCmd.text = "분기 성공";

        bool edited = eventEditor_->insertCommand("", 0, sayCmd) && eventEditor_->insertCommand("", 1, addCmd) &&
                      eventEditor_->insertCommand("", 2, ifCmd) && eventEditor_->insertCommand("2/t", 0, logCmd);
        auto event = [this, &target]() -> const naru::ObjectEvent& {
            return doc_->project().findScene(currentSceneId_.toStdString())->findObject(target.toStdString())->event;
        };
        check(edited && event().commands.size() == original.commands.size() + 3 &&
                  event().commands[2].thenCommands.size() == 1,
              "이벤트 명령 추가 (중첩 분기)");
        doc_->undoStack()->undo();
        check(event().commands[2].thenCommands.empty(), "이벤트 편집 실행 취소");
        doc_->undoStack()->redo();

        logMessages_.clear();
        eventEditor_->startTest();
        check(eventEditor_->isRunning() && !eventEditor_->messageClosed(), "테스트 실행: 대사에서 기다린다");
        eventEditor_->advanceDialogue();
        QElapsedTimer waited;
        waited.start();
        while (eventEditor_->isRunning() && waited.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        check(!eventEditor_->isRunning() && logMessages_.contains(QStringLiteral("[촌장] 어서 오게")) &&
                  logMessages_.contains(QStringLiteral("분기 성공")),
              "테스트 실행: 대사 넘김 후 변수·조건 분기 실행");

        check(saveAll(), "이벤트 저장");
        auto reloadedEvents = ProjectDocument::open(doc_->directory(), &error);
        check(reloadedEvents && reloadedEvents->project() == doc_->project(), "이벤트 재로드 동일");

        // 다음 실행을 위해 원래대로 되돌리고 저장한다.
        while (event().commands.size() > original.commands.size()) {
            eventEditor_->removeCommand("", 0);
        }
        saveAll();
    }

    logMessages_.clear();
    tabs_->setCurrentIndex(0);
    runScript();
    bool scriptRan = false;
    for (const QString& m : logMessages_) {
        scriptRan = scriptRan || m.startsWith(QStringLiteral("방문 횟수"));
    }
    check(scriptRan, "scripts/main.py 실행");

    std::printf("\n%d failed\n", failures);
    std::fflush(stdout);
    return failures == 0;
}
