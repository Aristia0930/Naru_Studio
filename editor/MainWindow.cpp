#include "MainWindow.h"
#include "AssetBrowser.h"

#include "DatabaseEditor.h"
#include "CommandDialog.h"
#include "EventEditor.h"
#include "CommonEventsEditor.h"
#include "GlobalScriptsDialog.h"
#include "Inspector.h"
#include "ProjectDocument.h"
#include "SceneView.h"
#include "ScriptEditor.h"
#include "UndoCommands.h"
#include "VariablesDialog.h"
#include "InputSettingsDialog.h"

#include "naru/events/BuiltinCommands.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/Session.h"

#include <QAction>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidget>
#include <QMenu>
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
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QTimer>

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
constexpr int kCommandRole = Qt::UserRole + 5;
constexpr int kCommonRole = Qt::UserRole + 6;
constexpr int kEventSnapshotRole = Qt::UserRole + 7;

// 닫을 수 없는 기본 탭 수 (장면·이벤트·데이터베이스). 스크립트 탭은 이 뒤에 열린다.
constexpr int kFixedTabs = 4;

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
    inputDiskSource_ = doc_->project().inputSource;
    connect(doc_, &ProjectDocument::inputSettingsChanged, this, &MainWindow::refreshInputSettingsEditor);
    tabs_ = new QTabWidget(this);
    tabs_->setTabsClosable(true);
    tabs_->setDocumentMode(true);
    sceneView_ = new SceneView(doc_, this);
    tabs_->addTab(sceneView_, QStringLiteral("장면"));
    eventEditor_ = new EventEditor(doc_, python_, this);
    tabs_->addTab(eventEditor_, QStringLiteral("이벤트"));
    databaseEditor_ = new DatabaseEditor(doc_, this);
    tabs_->addTab(databaseEditor_, QStringLiteral("데이터베이스"));
    commonEventsEditor_ = new CommonEventsEditor(doc_, python_, this);
    tabs_->addTab(commonEventsEditor_, QStringLiteral("공통 이벤트"));
    // 장면·이벤트·데이터베이스·공통 이벤트 탭은 닫을 수 없다. 스크립트 탭은 그 뒤에 열린다.
    for (int i = 0; i < kFixedTabs; ++i) {
        tabs_->tabBar()->setTabButton(i, QTabBar::RightSide, nullptr);
        tabs_->tabBar()->setTabButton(i, QTabBar::LeftSide, nullptr);
    }
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    createDocks();
    createActions();

    eventEditor_->setLogSink([this](const naru::LogEntry& entry) { appendLog(entry); });
    commonEventsEditor_->editor()->setLogSink([this](const naru::LogEntry& entry) { appendLog(entry); });
    commonEventsEditor_->editor()->setVariablesSink([this](const naru::Variables& v) { showVariables(v); });
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

MainWindow::~MainWindow() {
    // 창이 사라지는 중에 player 종료 신호가 로그 패널에 닿지 않도록 먼저 끊고 게임을 닫는다.
    if (game_) {
        game_->disconnect(this);
        game_->kill();
        game_->waitForFinished(2000);
    }
}

void MainWindow::showAssets() {
    auto* dialog = findChild<QDialog*>(QStringLiteral("assetsDialog"));
    if (!dialog) {
        dialog = new QDialog(this);
        dialog->setObjectName(QStringLiteral("assetsDialog"));
        dialog->setWindowTitle(QStringLiteral("에셋 목록"));
        dialog->resize(960, 680);
        auto* layout = new QVBoxLayout(dialog);
        auto* browser = new AssetBrowser(doc_, dialog);
        layout->addWidget(browser);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
        buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        connect(dialog, &QDialog::finished, browser, &AssetBrowser::stopAudio);
    }
    dialog->findChild<AssetBrowser*>()->refresh();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
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
    // 오른쪽 클릭: 장면을 시작 장면으로 지정
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = tree_->itemAt(pos);
        if (!item || item->data(0, kKindRole).toInt() != SceneItem) {
            return;
        }
        tree_->setCurrentItem(item);
        bool isStart = item->data(0, kSceneRole).toString().toStdString() == doc_->project().startSceneId;
        QMenu menu(this);
        QAction* start = menu.addAction(isStart ? QStringLiteral("시작 장면 (지금 지정됨)")
                                                : QStringLiteral("시작 장면으로 지정"));
        start->setEnabled(!isStart);
        if (menu.exec(tree_->viewport()->mapToGlobal(pos)) == start) {
            setStartScene();
        }
    });
    auto* treeDock = new QDockWidget(QStringLiteral("장면"), this);
    treeDock->setObjectName(QStringLiteral("sceneTreeDock"));
    treeDock->setWidget(tree_);
    addDockWidget(Qt::LeftDockWidgetArea, treeDock);

    scripts_ = new QListWidget(this);
    connect(scripts_, &QListWidget::itemActivated, this,
            [this](QListWidgetItem* item) { openScript(item->data(kFileRole).toString()); });
    // 오른쪽 클릭: 전역 스크립트로 사용 켜기/끄기
    scripts_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(scripts_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = scripts_->itemAt(pos);
        if (!item) {
            return;
        }
        QString fileName = QFileInfo(item->data(kFileRole).toString()).fileName();
        QMenu menu(this);
        QAction* open = menu.addAction(QStringLiteral("열기"));
        QAction* global = menu.addAction(QStringLiteral("전역 스크립트로 사용"));
        global->setCheckable(true);
        global->setChecked(doc_->isGlobalScript(fileName));
        QAction* chosen = menu.exec(scripts_->viewport()->mapToGlobal(pos));
        if (chosen == open) {
            openScript(item->data(kFileRole).toString());
        } else if (chosen == global) {
            toggleGlobalScript(fileName);
        }
    });
    connect(doc_, &ProjectDocument::globalScriptsChanged, this, &MainWindow::rebuildScriptList);
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
    auto* varDock = new QDockWidget(QStringLiteral("실행 변수 (F5/F6 결과)"), this);
    varDock->setObjectName(QStringLiteral("variablesDock"));
    varDock->setWidget(variablesTable_);
    addDockWidget(Qt::RightDockWidgetArea, varDock);
    tabifyDockWidget(inspectorDock, varDock);
    inspectorDock->raise();

    log_ = new QTreeWidget(this);
    log_->setColumnCount(3);
    log_->setHeaderLabels({QStringLiteral("수준"), QStringLiteral("메시지"), QStringLiteral("위치")});
    log_->setRootIsDecorated(false);
    log_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* copyLog = new QAction(QStringLiteral("선택한 로그 복사"), log_);
    copyLog->setObjectName(QStringLiteral("copyLogAction"));
    copyLog->setShortcut(QKeySequence::Copy);
    copyLog->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    log_->addAction(copyLog);
    connect(copyLog, &QAction::triggered, this, [this] { copySelectedLogs(); });
    log_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(log_, &QWidget::customContextMenuRequested, this, [this, copyLog](const QPoint& pos) {
        auto* item = log_->itemAt(pos);
        if (!item) return;
        if (!item->isSelected()) {
            log_->clearSelection();
            log_->setCurrentItem(item);
            item->setSelected(true);
        }
        QMenu menu(this);
        auto* detail = menu.addAction(QStringLiteral("로그 상세 보기"));
        menu.addAction(copyLog);
        if (menu.exec(log_->viewport()->mapToGlobal(pos)) == detail) showLogDetail(item);
    });
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
    // QKeySequence::Quit은 Windows에서 비어 있으므로 Ctrl+Q를 직접 지정한다.
    auto* quit = make(QStringLiteral("종료"), QKeySequence(Qt::CTRL | Qt::Key_Q), &QWidget::close);

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
    auto* globalScripts = make(QStringLiteral("전역 스크립트..."), {}, &MainWindow::editGlobalScripts);
    globalScripts->setToolTip(QStringLiteral("게임 전체에서 실행할 스크립트를 등록하고 순서를 정합니다"));
    auto* check = make(QStringLiteral("문법 검사"), QKeySequence(Qt::Key_F7), &MainWindow::checkSyntax);
    auto* run = make(QStringLiteral("스크립트 실행"), QKeySequence(Qt::Key_F5), &MainWindow::runScript);
    run->setToolTip(QStringLiteral("열린 스크립트 탭을, 장면 탭에서는 scripts/main.py를 실행합니다 (F5)"));

    auto* eventTest = make(QStringLiteral("이벤트 테스트 실행"), QKeySequence(Qt::Key_F6), [this] {
        startEventTest();
    });
    auto* eventStop = make(QStringLiteral("이벤트 테스트 중지"), QKeySequence(Qt::Key_F8),
                           [this] {
                               eventEditor_->stopTest();
                               commonEventsEditor_->editor()->stopTest();
                           });
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

    auto* assets = menuBar()->addMenu(QStringLiteral("에셋"));
    assets->addAction(make(QStringLiteral("에셋 목록..."), {}, &MainWindow::showAssets));

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

    auto* dbAction = make(QStringLiteral("데이터베이스"), QKeySequence(Qt::Key_F9),
                          [this] { tabs_->setCurrentWidget(databaseEditor_); });
    scene->addSeparator();
    scene->addAction(dbAction);

    runGameAction_ = make(QStringLiteral("게임 실행"), QKeySequence(Qt::CTRL | Qt::Key_R), [this] { runGame(); });
    runGameAction_->setToolTip(QStringLiteral("저장한 뒤 시작 장면부터 게임 실행 화면을 띄웁니다 (Ctrl+R)"));
    runCurrentSceneAction_ = make(QStringLiteral("현재 장면부터 실행"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R),
                                  [this] { runGame(true); });
    runCurrentSceneAction_->setToolTip(QStringLiteral("저장한 뒤 지금 편집 중인 장면부터 게임 실행 화면을 띄웁니다 (Ctrl+Shift+R)"));
    stopGameAction_ = make(QStringLiteral("게임 중지"), {}, &MainWindow::stopGame);
    auto* variables = make(QStringLiteral("변수·스위치..."), {}, &MainWindow::editVariables);
    variables->setToolTip(QStringLiteral("게임 전체에서 쓰는 변수·스위치와 처음 값을 정합니다"));
    updateGameActions();
    auto* game = menuBar()->addMenu(QStringLiteral("게임(&G)"));
    game->addAction(runGameAction_);
    game->addAction(runCurrentSceneAction_);
    game->addAction(stopGameAction_);
    debugObjectNamesAction_ = make(QStringLiteral("게임 시작 시 객체 이름·ID 표시"), {}, [] {});
    debugObjectNamesAction_->setObjectName("debugObjectNames"); debugObjectNamesAction_->setCheckable(true);
    debugObjectNamesAction_->setToolTip(QStringLiteral("다음 게임 실행을 테스트 표시로 시작합니다. 실행 중에는 게임 창에서 F9로 켜고 끕니다. 프로젝트에는 저장하지 않습니다."));
    game->addAction(debugObjectNamesAction_);
    game->addSeparator();
    game->addAction(variables);
    auto* inputSettings = make(QStringLiteral("키 설정..."), {}, &MainWindow::editInputSettings);
    inputSettings->setObjectName(QStringLiteral("editInputSettings"));
    game->addAction(inputSettings);
    game->addSeparator();
    game->addAction(make(QStringLiteral("실행 변수 보기"), {}, [this] {
        auto* dock = findChild<QDockWidget*>(QStringLiteral("variablesDock"));
        dock->show(); dock->raise();
    }));
    game->addAction(make(QStringLiteral("게임 로그 폴더 열기"), {}, [this] {
        QString dir = qApp->property("gameLogDirectory").toString();
        if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
        if (QDir().mkpath(dir)) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    }));

    auto* events = menuBar()->addMenu(QStringLiteral("이벤트(&V)"));
    events->addAction(eventOpen);
    events->addAction(eventTest);
    events->addAction(eventStop);

    auto* script = menuBar()->addMenu(QStringLiteral("스크립트(&P)"));
    script->addAction(newScript);
    script->addAction(globalScripts);
    script->addAction(check);
    script->addAction(run);

    auto* toolbar = addToolBar(QStringLiteral("기본"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(saveAction);
    toolbar->addAction(undo);
    toolbar->addAction(redo);
    toolbar->addSeparator();
    toolbar->addAction(runGameAction_);
    toolbar->addSeparator();
    toolbar->addAction(addScene);
    toolbar->addAction(addObjectAction_);
    toolbar->addAction(deleteAction_);
    toolbar->addSeparator();
    toolbar->addAction(newScript);
    toolbar->addAction(check);
    toolbar->addAction(run);
    toolbar->addSeparator();
    toolbar->addAction(dbAction);
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
            objectItem->setText(0, (object.id == scene.playerObjectId ? QStringLiteral("★ ") : QString()) + q(object.name));
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
    if (tabs_->currentIndex() >= kFixedTabs) {
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

void MainWindow::showDatabase(const QString& typeKey, const QString& recordId, bool fieldsTab) {
    tabs_->setCurrentWidget(databaseEditor_);
    databaseEditor_->select(typeKey, recordId);
    if (fieldsTab) {
        databaseEditor_->showFieldsTab();
    }
}

void MainWindow::showCommonEvent(const QString& id, bool run) {
    tabs_->setCurrentWidget(commonEventsEditor_);
    commonEventsEditor_->selectEvent(id);
    if (run) commonEventsEditor_->editor()->startTest();
}

void MainWindow::startEventTest() {
    if (tabs_->currentWidget() == commonEventsEditor_) {
        eventEditor_->stopTest();
        commonEventsEditor_->editor()->startTest();
        return;
    }
    commonEventsEditor_->editor()->stopTest();
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
    const auto& globals = doc_->project().globalScripts;
    for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*.py")}, QDir::Files, QDir::Name)) {
        auto* item = new QListWidgetItem(info.fileName(), scripts_);
        item->setData(kFileRole, info.absoluteFilePath());
        auto it = std::find(globals.begin(), globals.end(), info.fileName().toStdString());
        if (it != globals.end()) {
            // 전역 스크립트: [전역 순서] 표시
            item->setText(QStringLiteral("%1   [전역 %2]").arg(info.fileName()).arg(it - globals.begin() + 1));
            QFont bold = item->font();
            bold.setBold(true);
            item->setFont(bold);
            item->setToolTip(QStringLiteral("전역 스크립트 — 게임 전체에서 실행됩니다 (스크립트 → 전역 스크립트...)"));
        }
    }
}

void MainWindow::refreshInputSettingsEditor() {
    if (!tabs_) return;
    refreshingInputSettings_ = true;
    const auto& project = doc_->project();
    for (int i = kFixedTabs; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (!editor || QFileInfo(editor->path()).fileName().toStdString() != project.inputSettingsScript) continue;
        auto source = q(project.inputSource);
        if (editor->toPlainText() != source) {
            auto position = editor->textCursor().position();
            editor->setPlainText(source);
            auto cursor = editor->textCursor();
            cursor.setPosition(std::min(position, static_cast<int>(source.size())));
            editor->setTextCursor(cursor);
            editor->document()->setModified(true);
        }
    }
    refreshingInputSettings_ = false;
}

bool MainWindow::syncInputSettings(QString* error) {
    const auto& project = doc_->project();
    if (project.inputSettingsScript.empty()) return true;
    for (int i = kFixedTabs; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (!editor || QFileInfo(editor->path()).fileName().toStdString() != project.inputSettingsScript) continue;
        auto parsed = naru::parseInputSettings(editor->toPlainText().toStdString());
        if (!parsed.settings) {
            if (error) *error = QStringLiteral("%1:%2: %3").arg(q(project.inputSettingsScript)).arg(parsed.line).arg(q(parsed.error));
            return false;
        }
    }
    QFile input(QDir(doc_->scriptsDirectory()).filePath(q(project.inputSettingsScript)));
    if (!input.exists() && inputDiskSource_.empty()) return true; // 첫 화면 설정, 아직 저장 전
    if (!input.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("입력 설정 파일을 읽을 수 없습니다: %1").arg(input.errorString());
        return false;
    }
    auto disk = QString::fromUtf8(input.readAll()).toStdString();
    if (disk == inputDiskSource_) return true;
    if (project.inputSource != inputDiskSource_ && project.inputSource != disk) {
        if (error) *error = QStringLiteral("입력 설정이 화면과 외부 파일 양쪽에서 바뀌었습니다. 외부 내용을 확인한 뒤 한쪽 변경을 취소하고 다시 저장하세요.");
        return false;
    }
    auto parsed = naru::parseInputSettings(disk);
    if (!parsed.settings) {
        if (error) *error = QStringLiteral("%1:%2: %3").arg(q(project.inputSettingsScript)).arg(parsed.line).arg(q(parsed.error));
        return false;
    }
    if (disk != project.inputSource) doc_->undoStack()->push(new SetInputSettingsCommand(doc_, project.inputSettingsScript, disk));
    inputDiskSource_ = disk;
    return true;
}

void MainWindow::editInputSettings() {
    QString error;
    if (!syncInputSettings(&error)) {
        QMessageBox::warning(this, QStringLiteral("키 설정"), error);
        return;
    }
    InputSettingsDialog dialog(doc_->project().inputSource, this);
    if (dialog.exec() != QDialog::Accepted) return;
    auto script = doc_->project().inputSettingsScript;
    if (script.empty()) {
        script = "input_settings.py";
        int index = 2;
        while (QFileInfo::exists(QDir(doc_->scriptsDirectory()).filePath(q(script))))
            script = "input_settings_" + std::to_string(index++) + ".py";
    }
    if (script != doc_->project().inputSettingsScript || dialog.source() != doc_->project().inputSource)
        doc_->undoStack()->push(new SetInputSettingsCommand(doc_, script, dialog.source()));
}

void MainWindow::editVariables() {
    VariablesDialog dialog(doc_->project().variables, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::vector<naru::VariableDef> after = dialog.variables();
    if (after != doc_->project().variables) {
        doc_->undoStack()->push(new SetVariablesCommand(doc_, std::move(after)));
    }
}

void MainWindow::editGlobalScripts() {
    GlobalScriptsDialog dialog(doc_->scriptFiles(), doc_->project().globalScripts, doc_->scriptsDirectory(), python_, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    std::vector<std::string> after = dialog.selected();
    if (after != doc_->project().globalScripts) {
        doc_->undoStack()->push(new SetGlobalScriptsCommand(doc_, std::move(after)));
    }
}

void MainWindow::toggleGlobalScript(const QString& fileName) {
    std::vector<std::string> after = doc_->project().globalScripts;
    auto it = std::find(after.begin(), after.end(), fileName.toStdString());
    if (it != after.end()) {
        after.erase(it);
    } else {
        after.push_back(fileName.toStdString());
    }
    doc_->undoStack()->push(new SetGlobalScriptsCommand(doc_, std::move(after)));
}

// 등록된 전역 스크립트를 검사해 로그에 남긴다 (변수 이름 겹침 등). 문제가 없으면 true.
bool MainWindow::checkGlobalScripts() {
    std::vector<std::string> paths;
    for (const std::string& name : doc_->project().globalScripts) {
        paths.push_back(QFileInfo(QDir(doc_->scriptsDirectory()).filePath(q(name))).absoluteFilePath().toStdString());
    }
    auto errors = python_->checkGlobalScripts(paths);
    for (const auto& e : errors) {
        appendLog({naru::LogLevel::Error, e.type + ": " + e.message, {e.file, e.line, ""}, e.traceback});
    }
    return errors.empty();
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

void MainWindow::captureScript(const QString& filename, bool showError) {
    if (QFileInfo(filename).fileName() != filename) return;
    if (!openScript(QDir(doc_->scriptsDirectory()).filePath(filename))) return;
    if (showError) {
        checkSyntax();
        if (log_->topLevelItemCount()) showLogDetail(log_->topLevelItem(log_->topLevelItemCount() - 1));
        if (auto* detail = findChild<QPlainTextEdit*>(QStringLiteral("logDetailText"))) {
            QString text = detail->toPlainText();
            const QString directory = QDir(doc_->directory()).absolutePath();
            text.replace(directory, QStringLiteral("프로젝트"), Qt::CaseInsensitive);
            text.replace(QDir::toNativeSeparators(directory), QStringLiteral("프로젝트"), Qt::CaseInsensitive);
            detail->setPlainText(text);
        }
    }
}

ScriptEditor* MainWindow::openScript(const QString& path) {
    QString absolute = QFileInfo(path).absoluteFilePath();
    for (int i = kFixedTabs; i < tabs_->count(); ++i) {
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
    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor] {
        const auto& project = doc_->project();
        if (refreshingInputSettings_ || QFileInfo(editor->path()).fileName().toStdString() != project.inputSettingsScript) return;
        auto source = editor->toPlainText().toStdString();
        auto parsed = naru::parseInputSettings(source);
        if (!parsed.settings) {
            statusBar()->showMessage(QStringLiteral("키 설정 %1행: %2").arg(parsed.line).arg(q(parsed.error)));
            return;
        }
        if (source != project.inputSource)
            doc_->undoStack()->push(new SetInputSettingsCommand(doc_, project.inputSettingsScript, source, true));
    });
    tabs_->setCurrentIndex(index);
    refreshInputSettingsEditor();
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
        if (answer == QMessageBox::Discard && QFileInfo(editor->path()).fileName().toStdString() == doc_->project().inputSettingsScript) {
            QFile input(editor->path());
            if (!input.open(QIODevice::ReadOnly)) {
                QMessageBox::warning(this, QStringLiteral("키 설정 복원 실패"), input.errorString());
                return;
            }
            auto source = QString::fromUtf8(input.readAll()).toStdString();
            auto parsed = naru::parseInputSettings(source);
            if (!parsed.settings) {
                QMessageBox::warning(this, QStringLiteral("키 설정 복원 실패"), q(parsed.error));
                return;
            }
            doc_->undoStack()->push(new SetInputSettingsCommand(doc_, doc_->project().inputSettingsScript, source));
            inputDiskSource_ = source;
        }
        if (answer == QMessageBox::Save && QFileInfo(editor->path()).fileName().toStdString() == doc_->project().inputSettingsScript) {
            if (!saveAll()) return;
        }
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
    if (!syncInputSettings(&error)) {
        QMessageBox::warning(this, QStringLiteral("키 설정 저장 실패"), error);
        return false;
    }
    if (doc_->isModified() && !doc_->save(&error)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), error);
        return false;
    }
    for (int i = kFixedTabs; i < tabs_->count(); ++i) {
        auto* editor = qobject_cast<ScriptEditor*>(tabs_->widget(i));
        if (editor && editor->isModified() && !editor->save(&error)) {
            QMessageBox::warning(this, QStringLiteral("저장 실패"), editor->path() + QStringLiteral("\n") + error);
            return false;
        }
    }
    inputDiskSource_ = doc_->project().inputSource;
    rebuildScriptList();
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
    if (QFileInfo(editor->path()).fileName().toStdString() == doc_->project().inputSettingsScript) {
        auto result = naru::parseInputSettings(editor->toPlainText().toStdString());
        if (result.settings) appendLog({naru::LogLevel::Info, "키 설정 문법·값 검사 통과", {}, {}});
        else {
            appendLog({naru::LogLevel::Error, result.error, {editor->path().toStdString(), result.line, {}}, {}});
            editor->goToLine(result.line);
        }
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
        // 전역 스크립트면 다른 전역 스크립트와 변수 이름이 겹치지 않는지도 본다.
        if (doc_->isGlobalScript(QFileInfo(editor->path()).fileName()) && checkGlobalScripts()) {
            appendLog({naru::LogLevel::Info, "전역 스크립트 검사 통과 (변수 이름 겹침 없음)", {}, ""});
        }
    } else {
        appendLog({naru::LogLevel::Error, result.error.type + ": " + result.error.message,
                   {result.error.file, result.error.line, ""}, result.error.traceback});
        editor->goToLine(result.error.line);
    }
}

void MainWindow::runScript() {
    if (auto* editor = currentScript(); editor && QFileInfo(editor->path()).fileName().toStdString() == doc_->project().inputSettingsScript) {
        if (saveAll()) appendLog({naru::LogLevel::Info, "키 설정을 저장했습니다. 설정 파일은 자동으로 읽으며 게임 재시작 후 반영됩니다.", {}, {}});
        return;
    }
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

    naru::Database database = doc_->project().database; // 실행용 사본
    naru::Session session(doc_->project().initialVariables());
    session.setDatabase(&database);
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

// ---------------------------------------------------------------- 게임 실행

bool MainWindow::runGame(bool fromCurrentScene, const QStringList& extraArgs) {
    if (game_) {
        statusBar()->showMessage(QStringLiteral("게임이 이미 실행 중입니다"), 3000);
        return false;
    }
    // player는 저장된 파일을 읽으므로 먼저 저장한다.
    if (!saveAll()) {
        return false;
    }
    QString exe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("NaruPlayer.exe"));
    if (!QFileInfo::exists(exe)) {
        QMessageBox::warning(this, QStringLiteral("게임 실행"),
                             QStringLiteral("NaruPlayer.exe를 찾지 못했습니다:\n%1\n\nSDL3를 설치하고 다시 빌드하세요 "
                                            "(docs/BUILD.md).")
                                 .arg(QDir::toNativeSeparators(exe)));
        return false;
    }

    QStringList args{doc_->directory()};
    // 장면을 넘기지 않으면 player가 프로젝트의 시작 장면을 연다.
    if (fromCurrentScene && !currentSceneId_.isEmpty()) {
        args << QStringLiteral("--scene") << currentSceneId_;
    }
    if (debugObjectNamesAction_->isChecked()) args << QStringLiteral("--debug-object-names");
    args << extraArgs;
    QString logDir = qApp->property("gameLogDirectory").toString();
    if (logDir.isEmpty()) logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
    if (!QDir().mkpath(logDir)) {
        QMessageBox::warning(this, QStringLiteral("파일 로그"), QStringLiteral("로그 폴더를 만들 수 없습니다: ") + logDir);
        return false;
    }
    gameLogPath_ = QDir(logDir).filePath(QStringLiteral("game-%1.log").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
    args << QStringLiteral("--debug-state") << QStringLiteral("--log-file") << gameLogPath_;
    variablesTable_->setRowCount(0);
    auto* variableDock = findChild<QDockWidget*>(QStringLiteral("variablesDock"));
    variableDock->setWindowTitle(QStringLiteral("실행 변수 — 게임 시작 중"));
    variableDock->show();
    variableDock->raise();

    game_ = new QProcess(this);
    gameProject_ = doc_->project();
    gameOutput_.clear();
    lastGameExitCode_ = -1;
    connect(game_, &QProcess::readyReadStandardOutput, this, &MainWindow::readGameOutput);
    connect(game_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        readGameOutput();
        lastGameExitCode_ = status == QProcess::NormalExit ? code : -1;
        if (status != QProcess::NormalExit) {
            appendLog({naru::LogLevel::Error, "게임이 비정상 종료되었습니다", {}, ""});
        } else if (code == 1) {
            appendLog({naru::LogLevel::Error, "게임을 시작하지 못했습니다 (위 로그 참고)", {}, ""});
        }
        game_->deleteLater();
        game_ = nullptr;
        findChild<QDockWidget*>(QStringLiteral("variablesDock"))->setWindowTitle(QStringLiteral("실행 변수 — 종료 시 마지막 수신 값"));
        updateGameActions();
    });
    connect(game_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && game_) {
            appendLog({naru::LogLevel::Error, "NaruPlayer.exe를 실행하지 못했습니다: " + game_->errorString().toStdString(),
                       {}, ""});
            game_->deleteLater();
            game_ = nullptr;
            findChild<QDockWidget*>(QStringLiteral("variablesDock"))->setWindowTitle(QStringLiteral("실행 변수 — 게임 시작 실패"));
            updateGameActions();
        }
    });
    appendLog({naru::LogLevel::Info, "게임 실행", {}, ""});
    game_->start(exe, args);
    updateGameActions();
    return true;
}

void MainWindow::stopGame() {
    if (!game_) {
        return;
    }
    // 창 닫기 요청을 보내고, 응답이 없으면 강제 종료한다.
    game_->terminate();
    QPointer<QProcess> process = game_;
    QTimer::singleShot(2000, this, [process] {
        if (process && process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
}

void MainWindow::readGameOutput() {
    if (!game_) {
        return;
    }
    gameOutput_ += game_->readAllStandardOutput();
    auto unescape = [](const QString& text) {
        QString decoded;
        for (qsizetype i = 0; i < text.size(); ++i) {
            if (text[i] == '\\' && i + 1 < text.size()) {
                const auto next = text[++i];
                if (next == 'n') decoded += '\n';
                else if (next == 't') decoded += '\t';
                else if (next == '\\') decoded += '\\';
                else { decoded += '\\'; decoded += next; }
            } else decoded += text[i];
        }
        return decoded;
    };
    qsizetype end;
    while ((end = gameOutput_.indexOf('\n')) >= 0) {
        // trimmed()는 쓰지 않는다: 마지막 빈 필드(부가정보)의 탭까지 지워진다.
        QString line = QString::fromUtf8(gameOutput_.left(end));
        gameOutput_.remove(0, end + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QStringLiteral("NARU_STATE\t"))) {
            QJsonParseError error;
            auto state = QJsonDocument::fromJson(line.mid(11).toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !state.isObject()) continue;
            const auto values = state.object();
            variablesTable_->setRowCount(0);
            for (auto it = values.begin(); it != values.end(); ++it) {
                int row = variablesTable_->rowCount();
                variablesTable_->insertRow(row);
                variablesTable_->setItem(row, 0, new QTableWidgetItem(it.key()));
                variablesTable_->setItem(row, 1, new QTableWidgetItem(it.value().toObject().value(QStringLiteral("value")).toString()));
            }
            findChild<QDockWidget*>(QStringLiteral("variablesDock"))->setWindowTitle(QStringLiteral("실행 변수 — 게임 실행 중 (약 0.2초마다)"));
            continue;
        }
        // 뒤의 장면·객체·공통 이벤트 필드로 실제 편집 위치를 찾는다.
        QStringList f = line.split('\t');
        naru::LogEntry entry;
        if (f.size() >= 7 && f[0] == QStringLiteral("NARU")) {
            entry.level = f[1] == QStringLiteral("error")     ? naru::LogLevel::Error
                          : f[1] == QStringLiteral("warning") ? naru::LogLevel::Warning
                                                              : naru::LogLevel::Info;
            entry.source.commandId = unescape(f[2]).toStdString();
            entry.source.file = unescape(f[3]).toStdString();
            entry.source.line = f[4].toInt();
            entry.message = unescape(f[5]).toStdString();
            entry.detail = unescape(f[6]).toStdString();
            if (f.size() >= 10) {
                entry.source.sceneId = unescape(f[7]).toStdString();
                entry.source.objectId = unescape(f[8]).toStdString();
                entry.source.commonEventId = unescape(f[9]).toStdString();
            }
        } else {
            entry.message = line.toStdString();
        }
        entry.message = "[게임] " + entry.message;
        appendLog(entry);
    }
}

void MainWindow::updateGameActions() {
    runGameAction_->setEnabled(game_ == nullptr);
    runCurrentSceneAction_->setEnabled(game_ == nullptr);
    stopGameAction_->setEnabled(game_ != nullptr);
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
    item->setData(0, kCommandRole, q(entry.source.commandId));
    item->setData(0, kSceneRole, q(entry.source.sceneId));
    item->setData(0, kObjectRole, q(entry.source.objectId));
    item->setData(0, kCommonRole, q(entry.source.commonEventId));
    if (!entry.source.commandId.empty() && q(entry.message).startsWith(QStringLiteral("[게임] "))) {
        const naru::ObjectEvent* event = nullptr;
        if (!entry.source.commonEventId.empty()) {
            if (const auto* common = gameProject_.findCommonEvent(entry.source.commonEventId)) event = &common->event;
        } else if (const auto* scene = gameProject_.findScene(entry.source.sceneId)) {
            if (const auto* object = scene->findObject(entry.source.objectId)) event = &object->event;
        }
        if (event) item->setData(0, kEventSnapshotRole, QVariant::fromValue(*event));
    }
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

void MainWindow::copySelectedLogs() {
    QStringList rows;
    for (int i = 0; i < log_->topLevelItemCount(); ++i) {
        auto* item = log_->topLevelItem(i);
        if (!item->isSelected()) continue;
        QString row = item->text(0) + QLatin1Char('\t') + item->text(1) + QLatin1Char('\t') + item->text(2);
        if (!item->toolTip(1).isEmpty()) row += QLatin1Char('\n') + item->toolTip(1);
        rows << row;
    }
    if (!rows.isEmpty()) QApplication::clipboard()->setText(rows.join(QLatin1Char('\n')));
}

void MainWindow::showLogDetail(QTreeWidgetItem* item) {
    if (!item) return;
    auto* dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("logDetailDialog"));
    dialog->setWindowTitle(QStringLiteral("로그 상세"));
    dialog->resize(760, 480);
    auto* layout = new QVBoxLayout(dialog);
    auto* text = new QPlainTextEdit(dialog);
    text->setObjectName(QStringLiteral("logDetailText"));
    text->setReadOnly(true);
    QString content = item->text(0) + QStringLiteral("  ") + item->text(1);
    if (!item->text(2).isEmpty()) content += QStringLiteral("\n") + item->text(2);
    if (!item->toolTip(1).isEmpty()) content += QStringLiteral("\n\n") + item->toolTip(1);
    text->setPlainText(content);
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("닫기"));
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}

void MainWindow::showVariables(const naru::Variables& variables) {
    if (game_) return;
    findChild<QDockWidget*>(QStringLiteral("variablesDock"))->setWindowTitle(QStringLiteral("실행 변수 — F5/F6 결과"));
    variablesTable_->setRowCount(0);
    for (const auto& [name, value] : variables.all()) {
        int row = variablesTable_->rowCount();
        variablesTable_->insertRow(row);
        variablesTable_->setItem(row, 0, new QTableWidgetItem(q(name)));
        variablesTable_->setItem(row, 1, new QTableWidgetItem(q(naru::toDisplayString(value))));
    }
}

void MainWindow::jumpToLog(QTreeWidgetItem* item) {
    const auto command = item->data(0, kCommandRole).toString();
    const auto common = item->data(0, kCommonRole).toString();
    const auto sceneId = item->data(0, kSceneRole).toString();
    const auto objectId = item->data(0, kObjectRole).toString();
    const auto snapshot = item->data(0, kEventSnapshotRole);
    if (snapshot.isValid()) {
        const naru::ObjectEvent* current = nullptr;
        if (!common.isEmpty()) {
            if (const auto* event = doc_->project().findCommonEvent(common.toStdString())) current = &event->event;
        } else if (const auto* scene = doc_->project().findScene(sceneId.toStdString())) {
            if (const auto* object = scene->findObject(objectId.toStdString())) current = &object->event;
        }
        if (!current || *current != snapshot.value<naru::ObjectEvent>()) {
            statusBar()->showMessage(QStringLiteral("실행 후 이벤트가 변경되었습니다. 게임을 다시 실행하고 새 로그를 확인하세요."), 6000);
            return;
        }
    }
    if (!command.isEmpty()) {
        EventEditor* target = nullptr;
        if (!common.isEmpty() && doc_->project().findCommonEvent(common.toStdString())) {
            showCommonEvent(common);
            target = commonEventsEditor_->editor();
        } else if (common.isEmpty()) {
            const auto* scene = doc_->project().findScene(sceneId.toStdString());
            if (scene && scene->findObject(objectId.toStdString())) {
                showScene(sceneId);
                setSelectionTarget(sceneId, objectId);
                openEventTab();
                target = eventEditor_;
            }
        }
        if (target && target->selectCommand(command.toStdString())) return;
        statusBar()->showMessage(QStringLiteral("실행 후 이벤트가 변경되었거나 로그의 명령 위치가 없습니다."), 5000);
    }
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
    for (int i = kFixedTabs; i < tabs_->count(); ++i) {
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

    // 화면↔Python, 스크립트 탭↔프로젝트, Undo·저장·외부 파일 변경을 함께 검사한다.
    {
        const auto originalScript = doc_->project().inputSettingsScript;
        const auto originalSource = doc_->project().inputSource;
        InputSettingsDialog dialog(originalSource, this);
        auto* enabled = dialog.findChild<QCheckBox*>(QStringLiteral("inputDefaultControl"));
        auto* speed = dialog.findChild<QDoubleSpinBox*>(QStringLiteral("inputPlayerSpeed"));
        auto* code = dialog.findChild<QPlainTextEdit*>(QStringLiteral("inputSettingsSource"));
        check(dialog.valid() && enabled && speed && code, "키 설정 화면 생성");
        auto* addCombination = dialog.findChild<QPushButton*>(QStringLiteral("inputAddCombination"));
        QTimer::singleShot(0, this, [] {
            auto* modal = QApplication::activeModalWidget();
            if (!modal) return;
            auto* list = modal->findChild<QListWidget*>(QStringLiteral("combinationKeys"));
            auto* buttons = modal->findChild<QDialogButtonBox*>(QStringLiteral("combinationButtons"));
            if (!list || !buttons) return;
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->text() == QStringLiteral("Shift") || list->item(i)->text() == QStringLiteral("E"))
                    list->item(i)->setCheckState(Qt::Checked);
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        addCombination->click();
        check(dialog.source().find("Shift + E") != std::string::npos, "동시 키 선택 창 실제 체크·확인으로 Python 설정 갱신");
        auto* actions = dialog.findChild<QListWidget*>(QStringLiteral("inputActions"));
        auto selectedAction = actions->currentItem()->data(Qt::UserRole).toString().toStdString();
        check(dialog.settings().bindings.at(selectedAction).back() == "Shift + E", "화면 조합이 원본 설정 모델과 일치");
        enabled->setChecked(false);
        speed->setValue(180);
        auto fromUi = naru::parseInputSettings(dialog.source());
        check(fromUi.settings && !fromUi.settings->defaultControl && fromUi.settings->playerSpeed == 180,
              "화면 ON/OFF·속도 변경이 Python 변수에 동기화");
        auto settings = dialog.settings();
        settings.defaultControl = true;
        settings.playerSpeed = 90;
        settings.bindings["right"] = {"L"};
        settings.bindings["cancel"].clear();
        code->setPlainText(q(naru::inputSettingsSource(settings, dialog.source())));
        check(enabled->isChecked() && speed->value() == 90 && dialog.settings().bindings.at("cancel").empty(),
              "Python 변수 변경이 화면 ON/OFF·속도·키에 동기화");
        auto good = dialog.source();
        code->setPlainText(q(good + "import os\n"));
        check(!dialog.valid() && !dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled(),
              "설정 파일에 실행 코드를 넣으면 적용 차단");
        code->setPlainText(q(good));
        doc_->undoStack()->push(new SetInputSettingsCommand(doc_, originalScript, good));
        QString path = QDir(doc_->scriptsDirectory()).filePath(q(originalScript));
        auto* editor = openScript(path);
        check(editor && editor->toPlainText().toStdString() == good, "이미 열린 Python 설정 탭에도 같은 값 반영");
        settings.playerSpeed = 120;
        editor->setPlainText(q(naru::inputSettingsSource(settings, good)));
        check(doc_->project().inputSettings.playerSpeed == 120, "스크립트 탭 편집도 프로젝트 설정에 동기화");
        doc_->undoStack()->undo();
        check(doc_->project().inputSettings.playerSpeed == 90 && editor->toPlainText().toStdString() == good,
              "키 설정 Undo는 화면 모델과 Python 내용을 함께 복구");
        QString error;
        editor->setPlainText(q(good + "invalid\n"));
        check(!syncInputSettings(&error) && !error.isEmpty(), "잘못된 설정 초안은 저장·실행 전에 거부");
        editor->setPlainText(q(good));
        check(saveAll(), "동기화한 키 설정 저장");
        auto loaded = ProjectDocument::open(doc_->directory(), &error);
        check(loaded && loaded->project().inputSettings == doc_->project().inputSettings, "키 설정 재열기 일치");
        settings.playerSpeed = 75;
        auto external = naru::inputSettingsSource(settings, good);
        QFile file(path);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(QByteArray::fromStdString(external));
        file.close();
        check(syncInputSettings(&error) && doc_->project().inputSettings.playerSpeed == 75 &&
              editor->toPlainText().toStdString() == external, "외부 Python 설정 변경도 화면에 반영");
        settings.playerSpeed = 80;
        editor->setPlainText(q(naru::inputSettingsSource(settings, external)));
        editor->document()->setModified(true);
        settings.playerSpeed = 85;
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(QByteArray::fromStdString(naru::inputSettingsSource(settings, external)));
        file.close();
        check(!syncInputSettings(&error), "화면과 외부 파일이 동시에 바뀌면 덮어쓰기 차단");
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        file.write(QByteArray::fromStdString(external));
        file.close();
        QTimer::singleShot(0, this, [] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                if (auto* discard = box->button(QMessageBox::Discard)) discard->click();
        });
        closeTab(tabs_->indexOf(editor));
        check(tabs_->indexOf(editor) < 0 && doc_->project().inputSource == external &&
                  doc_->project().inputSettings.playerSpeed == 75, "설정 탭 변경 버리기는 연결된 화면 값도 저장본으로 복구");
        doc_->undoStack()->push(new SetInputSettingsCommand(doc_, originalScript, originalSource));
        check(saveAll(), "입력 테스트 후 원래 설정 복원");
        tabs_->setCurrentIndex(0);
    }

    // 공통 이벤트 편집·Undo·실행용 사본·저장·재로드를 실제 위젯에서 검사한다.
    {
        auto original = doc_->project().commonEvents;
        auto id = commonEventsEditor_->addEvent(QStringLiteral("테스트 회복"));
        auto* common = commonEventsEditor_->editor();
        check(!id.isEmpty() && doc_->project().findCommonEvent(id.toStdString()), "공통 이벤트 추가");
        naru::EventCommand message;
        message.type = naru::EventCommandType::Say;
        message.text = "공통 이벤트 대기";
        naru::EventCommand log;
        log.type = naru::EventCommandType::Log;
        log.text = "공통 이벤트 원본 실행";
        check(common->insertCommand("", 0, message) && common->insertCommand("", 1, log), "공통 이벤트 명령 편집");
        commonEventsEditor_->renameEvent(QStringLiteral("회복 이름 변경"));
        check(doc_->project().findCommonEvent(id.toStdString())->name == "회복 이름 변경", "공통 이벤트 이름 변경은 ID 유지");
        doc_->undoStack()->undo();
        check(doc_->project().findCommonEvent(id.toStdString())->name == "테스트 회복", "공통 이벤트 이름 Undo");
        auto second = commonEventsEditor_->addEvent(QStringLiteral("다른 이벤트"));
        commonEventsEditor_->moveEvent(-1);
        check(doc_->project().commonEvents.front().id == second.toStdString(), "공통 이벤트 순서 변경");
        commonEventsEditor_->selectEvent(id);
        showCommonEvent(id);
        startEventTest();
        check(common->isRunning() && !common->messageClosed(), "공통 이벤트 F6 대사 대기");
        log.text = "편집 후 새 내용";
        common->replaceCommand("", 1, log);
        common->advanceDialogue();
        QElapsedTimer waited;
        waited.start();
        while (common->isRunning() && waited.elapsed() < 2000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        check(!common->isRunning() && logMessages_.contains(QStringLiteral("공통 이벤트 원본 실행")) &&
                  !logMessages_.contains(QStringLiteral("편집 후 새 내용")), "F6는 편집 데이터와 분리된 사본 실행");
        common->startTest();
        common->stopTest();
        check(!common->isRunning() && common->messageClosed(), "공통 이벤트 F8 중지");
        commonEventsEditor_->removeEvent();
        check(!doc_->project().findCommonEvent(id.toStdString()), "공통 이벤트 삭제");
        doc_->undoStack()->undo();
        check(doc_->project().findCommonEvent(id.toStdString()) != nullptr, "공통 이벤트 삭제 Undo");
        check(saveAll(), "공통 이벤트 저장");
        QString commonError;
        auto loaded = ProjectDocument::open(doc_->directory(), &commonError);
        check(loaded && loaded->project().commonEvents == doc_->project().commonEvents, "공통 이벤트 저장 후 재열기");
        doc_->undoStack()->push(new SetCommonEventsCommand(doc_, original));
        tabs_->setCurrentIndex(0);
    }

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

        // 실행 조건·반복: 조건이 맞지 않아도 테스트 실행은 경고만 하고 실행한다.
        check(eventEditor_->setConditions({{"visits", naru::CompareOp::GreaterEqual, std::int64_t{3}}}, true) &&
                  event().conditions.size() == 1 && event().repeat,
              "이벤트 실행 조건 지정");
        doc_->undoStack()->undo();
        check(event().conditions == original.conditions && event().repeat == original.repeat, "실행 조건 실행 취소");
        doc_->undoStack()->redo();
        logMessages_.clear();
        eventEditor_->startTest();
        eventEditor_->advanceDialogue();
        waited.restart();
        while (eventEditor_->isRunning() && waited.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        check(std::any_of(logMessages_.begin(), logMessages_.end(),
                          [](const QString& m) { return m.contains(QStringLiteral("실행 조건이 처음 값으로는 맞지 않습니다")); }) &&
                  logMessages_.contains(QStringLiteral("분기 성공")),
              "조건이 맞지 않아도 테스트 실행은 경고 후 실행");

        check(saveAll(), "이벤트 저장");
        auto reloadedEvents = ProjectDocument::open(doc_->directory(), &error);
        check(reloadedEvents && reloadedEvents->project() == doc_->project(), "이벤트 재로드 동일");

        // 다음 실행을 위해 원래대로 되돌리고 저장한다.
        while (event().commands.size() > original.commands.size()) {
            eventEditor_->removeCommand("", 0);
        }
        eventEditor_->setConditions(original.conditions, original.repeat);
        saveAll();
    }

    // 선택지: 순서를 바꿔도 분기 명령·처음 선택·취소 대상이 함께 이동하고, F6에서 실제 버튼으로 선택한다.
    {
        const auto& scene = *doc_->project().findScene(currentSceneId_.toStdString());
        const QString target = q(scene.objects.front().id);
        sceneView_->selectObject(target);
        onCanvasSelection(target);
        openEventTab();
        naru::EventCommand firstLog;
        firstLog.type = naru::EventCommandType::Log;
        firstLog.text = "첫 선택 실행";
        auto secondLog = firstLog;
        secondLog.text = "둘째 선택 실행";
        naru::EventCommand choice;
        choice.type = naru::EventCommandType::ShowChoices;
        choice.text = "어느 쪽인가요?";
        choice.defaultChoice = 1;
        choice.cancelChoice = 1;
        choice.choices = {{"첫째", {firstLog}}, {"둘째", {secondLog}}};
        CommandDialog dialog(choice, {}, &doc_->project(), &scene);
        auto* options = dialog.findChild<QListWidget*>(QStringLiteral("choiceOptions"));
        auto* up = dialog.findChild<QPushButton*>(QStringLiteral("choiceUp"));
        check(options && up, "선택지 입력 창의 편집 위젯");
        if (options && up) {
            options->setCurrentRow(1);
            up->click();
            QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
            const auto reordered = dialog.command();
            check(reordered.choices[0].text == "둘째" && reordered.choices[0].commands == choice.choices[1].commands &&
                      reordered.defaultChoice == 0 && reordered.cancelChoice == 0,
                  "선택지 순서 변경은 분기 내용·처음 선택·취소 대상을 보존");
        }
        check(eventEditor_->insertCommand("", 0, choice), "선택지 명령 추가");
        check(eventEditor_->insertCommand("0/c0", 1, secondLog), "선택지 안에 명령 추가");
        doc_->undoStack()->undo();
        const auto& stored = doc_->project().findScene(currentSceneId_.toStdString())->findObject(target.toStdString())->event.commands[0];
        check(stored.choices[0].commands.size() == 1, "선택지 안의 명령 추가 Undo");
        logMessages_.clear();
        eventEditor_->startTest();
        auto* preview = eventEditor_->findChild<QListWidget*>(QStringLiteral("previewChoices"));
        auto* choose = eventEditor_->findChild<QPushButton*>(QStringLiteral("previewChoose"));
        check(eventEditor_->choices() && preview && preview->currentRow() == 1, "F6 선택지 처음 선택 표시");
        eventEditor_->advanceDialogue();
        check(eventEditor_->choices() != nullptr, "선택지는 대사 넘기기로 닫히지 않음");
        if (choose) choose->click();
        QElapsedTimer waited;
        waited.start();
        while (eventEditor_->isRunning() && waited.elapsed() < 2000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        check(!eventEditor_->isRunning() && logMessages_.contains(QStringLiteral("둘째 선택 실행")) &&
                  !logMessages_.contains(QStringLiteral("첫 선택 실행")), "F6 실제 선택 버튼은 선택한 분기만 실행");
        eventEditor_->stopTest();
        eventEditor_->startTest();
        logMessages_.clear();
        if (preview) preview->setCurrentRow(0);
        if (auto* cancel = eventEditor_->findChild<QPushButton*>(QStringLiteral("previewCancelChoice"))) cancel->click();
        waited.restart();
        while (eventEditor_->isRunning() && waited.elapsed() < 2000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        check(!eventEditor_->isRunning() && logMessages_.contains(QStringLiteral("둘째 선택 실행")) &&
                  !logMessages_.contains(QStringLiteral("첫 선택 실행")), "F6 취소 버튼은 지정한 취소 항목을 실행");
        eventEditor_->stopTest();
        eventEditor_->startTest();
        eventEditor_->stopTest();
        check(!eventEditor_->choices() && eventEditor_->messageClosed(), "F8은 선택 요청·미리보기를 닫음");
        check(saveAll(), "선택지 이벤트 저장");
        auto reloadedChoices = ProjectDocument::open(doc_->directory(), &error);
        check(reloadedChoices && reloadedChoices->project() == doc_->project(), "선택지 이벤트 저장·재로드 동일");
        eventEditor_->removeCommand("", 0);
        saveAll();
    }

    // 데이터베이스: 템플릿으로 종류 추가 → 항목 추가 → 값 변경·Undo → 속성 형식 변경(값 변환) → 이벤트에서 읽기 → 저장·재로드
    {
        tabs_->setCurrentWidget(databaseEditor_);
        naru::DataType chars = naru::makeTemplate(naru::DataTemplate::Character, doc_->project().database);
        QString typeKey = q(chars.key);
        check(databaseEditor_->addType(chars) && doc_->project().database.findType(chars.key),
              "데이터 종류 추가 (캐릭터 템플릿)");
        check(databaseEditor_->addRecord(typeKey, naru::DataRecord{"ria", "리아", {}}), "데이터 항목 추가");
        auto hpOf = [this, &chars]() -> naru::Value {
            const naru::DataType* t = doc_->project().database.findType(chars.key);
            return t->valueOf(*t->findRecord("ria"), *t->findField("hp"));
        };
        check(hpOf() == naru::Value{std::int64_t{100}}, "값이 없으면 템플릿 기본값");
        databaseEditor_->setRecordValue(typeKey, QStringLiteral("ria"), QStringLiteral("hp"), std::int64_t{250});
        check(hpOf() == naru::Value{std::int64_t{250}}, "데이터 값 변경");
        doc_->undoStack()->undo();
        check(hpOf() == naru::Value{std::int64_t{100}}, "데이터 값 변경 실행 취소");
        doc_->undoStack()->redo();

        const naru::DataType* t = doc_->project().database.findType(chars.key);
        int hpIndex = t->indexOfField("hp");
        naru::FieldDef asText = t->fields[static_cast<std::size_t>(hpIndex)];
        asText.type = naru::FieldType::Text;
        asText.min.reset();
        asText.max.reset();
        asText.defaultValue = std::string("100");
        databaseEditor_->setField(typeKey, hpIndex, asText, false);
        check(hpOf() == naru::Value{std::string("250")}, "속성 형식 변경 시 기존 값 변환");
        doc_->undoStack()->undo();
        check(hpOf() == naru::Value{std::int64_t{250}}, "속성 변경 실행 취소");

        // 이벤트의 "데이터 → 변수"로 읽는다
        const naru::Scene* scene = doc_->project().findScene(currentSceneId_.toStdString());
        QString target = q(scene->objects.front().id);
        showObjectEvent(target);
        naru::EventCommand getData;
        getData.type = naru::EventCommandType::GetData;
        getData.dataType = chars.key;
        getData.record = "ria";
        getData.field = "hp";
        getData.variable = "ria_hp";
        bool inserted = eventEditor_->insertCommand("", 0, getData);
        eventEditor_->startTest();
        QElapsedTimer waited;
        waited.start();
        while (eventEditor_->isRunning() && waited.elapsed() < 3000) {
            if (!eventEditor_->messageClosed()) {
                eventEditor_->advanceDialogue();
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        bool found = false;
        for (int row = 0; row < variablesTable_->rowCount(); ++row) {
            found = found || (variablesTable_->item(row, 0)->text() == QStringLiteral("ria_hp") &&
                              variablesTable_->item(row, 1)->text() == QStringLiteral("250"));
        }
        check(inserted && found, "이벤트에서 데이터 읽기 (데이터 → 변수)");
        eventEditor_->removeCommand("", 0);

        check(saveAll(), "데이터베이스 저장");
        auto reloadedDb = ProjectDocument::open(doc_->directory(), &error);
        check(reloadedDb && reloadedDb->project().database == doc_->project().database, "데이터베이스 재로드 동일");
        tabs_->setCurrentIndex(0);
    }

    // 플레이어 지정: 새 프로젝트의 "주인공"이 플레이어. 해제·실행 취소, 플레이어 객체 삭제·실행 취소 시 복원
    {
        auto playerId = [this] {
            return doc_->project().findScene(currentSceneId_.toStdString())->playerObjectId;
        };
        const std::string hero = playerId();
        check(!hero.empty(), "새 프로젝트의 주인공이 플레이어로 지정됨");
        naru::Scene info = *doc_->project().findScene(currentSceneId_.toStdString());
        naru::Scene cleared = info;
        cleared.playerObjectId.clear();
        doc_->undoStack()->push(new SetSceneInfoCommand(doc_, info, cleared, -1));
        bool clearedOk = playerId().empty();
        doc_->undoStack()->undo();
        check(clearedOk && playerId() == hero, "플레이어 지정 해제·실행 취소");
        doc_->undoStack()->push(new RemoveObjectCommand(doc_, currentSceneId_.toStdString(), hero));
        bool removedOk = playerId().empty();
        doc_->undoStack()->undo();
        check(removedOk && playerId() == hero, "플레이어 객체 삭제 시 지정 해제, 실행 취소 시 복원");
    }

    // 전역 스크립트: 새 프로젝트의 game.py가 등록됨, 해제·실행 취소, 다른 전역 스크립트와 변수 이름이 겹치면 오류
    {
        const std::vector<std::string> registered = doc_->project().globalScripts;
        check(registered == std::vector<std::string>{"game.py"}, "새 프로젝트의 game.py가 전역 스크립트로 등록됨");
        toggleGlobalScript(QStringLiteral("game.py"));
        bool removed = doc_->project().globalScripts.empty();
        doc_->undoStack()->undo();
        check(removed && doc_->project().globalScripts == registered, "전역 스크립트 등록 해제·실행 취소");

        QString conflictPath = QDir(doc_->scriptsDirectory()).filePath(QStringLiteral("selftest_conflict.py"));
        QFile conflict(conflictPath);
        conflict.open(QIODevice::WriteOnly);
        conflict.write("DASH_ENABLED = True\n_private = 2\n");
        conflict.close();
        toggleGlobalScript(QStringLiteral("selftest_conflict.py"));
        logMessages_.clear();
        bool conflictFound = !checkGlobalScripts();
        bool mentioned = false;
        for (const QString& m : logMessages_) {
            mentioned = mentioned || (m.contains(QStringLiteral("DASH_ENABLED")) && !m.contains(QStringLiteral("_private")));
        }
        doc_->undoStack()->undo();
        QFile::remove(conflictPath);
        check(conflictFound && mentioned, "전역 스크립트 변수 이름 겹침을 오류로 알림 (_로 시작하는 이름은 제외)");
        check(checkGlobalScripts(), "겹침이 없으면 전역 스크립트 검사 통과");
    }

    // 게임 실행: 저장 → NaruPlayer가 현재 장면을 연다 → 주인공을 오른쪽으로 움직여 상자 앞에서 결정 키    // → 상자의 조사 이벤트가 실행된다 → 120프레임 뒤 스스로 닫히고 로그가 제작툴에 들어온다
    {
        logMessages_.clear();
        debugObjectNamesAction_->setChecked(true);
        bool started = runGame(true, {QStringLiteral("--frames"), QStringLiteral("120"), QStringLiteral("--hold"),
                                QStringLiteral("right:0-80"), QStringLiteral("--hold"), QStringLiteral("confirm:85-85")});
        check(started && game_ && game_->arguments().contains(QStringLiteral("--debug-object-names")), "전체 객체 이름·ID 메뉴의 게임 시작 인자 전달");
        debugObjectNamesAction_->setChecked(false);
        QElapsedTimer waited;
        waited.start();
        while (game_ && waited.elapsed() < 15000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        bool sceneLogged = false;
        bool chestOpened = false;
        bool globalStarted = false;
        bool globalSceneHook = false;
        for (const QString& m : logMessages_) {
            sceneLogged = sceneLogged || m.startsWith(QStringLiteral("[게임] 장면 시작"));
            chestOpened = chestOpened || m.startsWith(QStringLiteral("[게임] 상자 ▸"));
            globalStarted = globalStarted || m == QStringLiteral("[게임] 게임 시작 (전역 스크립트)");
            globalSceneHook = globalSceneHook || m.startsWith(QStringLiteral("[게임] 장면 진입: "));
        }
        check(started && !game_ && lastGameExitCode_ == 0 && sceneLogged, "게임 실행 (NaruPlayer, 로그 수신)");
        check(chestOpened, "게임에서 플레이어 이동 → 상자 앞 결정 키 → 조사 이벤트 실행");
        check(globalStarted && globalSceneHook, "전역 스크립트 on_game_start · on_scene_enter 실행");
        bool gameVariable = false;
        for (int row = 0; row < variablesTable_->rowCount(); ++row)
            gameVariable = gameVariable || variablesTable_->item(row, 0)->text() == QStringLiteral("box_opened");
        check(gameVariable && QFileInfo(gameLogPath_).size() > 0, "실제 게임 변수 수신·종료 후 유지·파일 로그 생성");
        auto* oldClipboard = new QMimeData;
        const auto* previousMime = QApplication::clipboard()->mimeData();
        if (previousMime) for (const auto& format : previousMime->formats()) oldClipboard->setData(format, previousMime->data(format));
        appendLog({naru::LogLevel::Error, "copy first", {"", 0, ""}, "Traceback\n  detail line"});
        auto* first = log_->topLevelItem(log_->topLevelItemCount() - 1);
        appendLog({naru::LogLevel::Info, "copy second", {"", 0, ""}, {}});
        auto* second = log_->topLevelItem(log_->topLevelItemCount() - 1);
        log_->clearSelection();
        first->setSelected(true);
        second->setSelected(true);
        log_->findChild<QAction*>(QStringLiteral("copyLogAction"))->trigger();
        const QString copied = QApplication::clipboard()->text();
        check(copied.contains(QStringLiteral("copy first")) && copied.contains(QStringLiteral("Traceback\n  detail line")) &&
              copied.indexOf(QStringLiteral("copy first")) < copied.indexOf(QStringLiteral("copy second")),
              "선택 로그와 traceback을 순서대로 복사");
        showLogDetail(first);
        auto* dialog = findChild<QDialog*>(QStringLiteral("logDetailDialog"));
        auto* text = dialog->findChild<QPlainTextEdit*>(QStringLiteral("logDetailText"));
        QTextCursor cursor = text->document()->find(QStringLiteral("detail line"));
        text->setTextCursor(cursor);
        text->copy();
        check(text->isReadOnly() && QApplication::clipboard()->text() == QStringLiteral("detail line"),
              "로그 상세에서 traceback 일부 선택 복사");
        dialog->close();
        QApplication::clipboard()->setMimeData(oldClipboard);
        naru::LogEntry link{naru::LogLevel::Error, "[게임] 위치 이동 검사", {"", 0, "0"}, {}};
        link.source.sceneId = currentSceneId_.toStdString();
        link.source.objectId = "obj_002";
        appendLog(link);
        jumpToLog(log_->topLevelItem(log_->topLevelItemCount() - 1));
        check(tabs_->currentWidget() == eventEditor_ && eventEditor_->selectCommand("0"), "객체 이벤트 로그 더블클릭 위치 이동");
        auto* oldLog = log_->topLevelItem(log_->topLevelItemCount() - 1);
        if (const auto* object = doc_->project().findScene(currentSceneId_.toStdString())->findObject("obj_002")) {
            auto command = object->event.commands.front();
            command.text += " (편집 후)";
            eventEditor_->replaceCommand("", 0, command);
            tabs_->setCurrentIndex(0);
            jumpToLog(oldLog);
            check(tabs_->currentIndex() == 0 &&
                  statusBar()->currentMessage().contains(QStringLiteral("변경되었습니다")), "실행 후 바뀐 이벤트에 옛 로그로 이동하지 않음");
            doc_->undoStack()->undo();
        }
        if (!doc_->project().commonEvents.empty()) {
            link.source.commonEventId = doc_->project().commonEvents.front().id;
            appendLog(link);
            jumpToLog(log_->topLevelItem(log_->topLevelItemCount() - 1));
            check(tabs_->currentWidget() == commonEventsEditor_, "공통 이벤트 로그 위치 이동");
        }
        if (!sceneLogged || !chestOpened || !globalStarted || lastGameExitCode_ != 0) {
            std::printf("  started=%d running=%d exit=%d\n", started, game_ != nullptr, lastGameExitCode_);
            for (const QString& m : logMessages_) {
                std::printf("  log: %s\n", m.toUtf8().constData());
            }
        }
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
