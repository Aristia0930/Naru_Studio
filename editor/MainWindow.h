#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"

#include <QByteArray>
#include <QMainWindow>
#include <QString>
#include <QStringList>

class DatabaseEditor;
class EventEditor;
class Inspector;
class ProjectDocument;
class QAction;
class QListWidget;
class QProcess;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class SceneView;
class ScriptEditor;

namespace naru::python {
class PythonRuntime;
}

// 프로젝트 작업 창: 장면 트리·2D 캔버스·인스펙터·스크립트 탭·로그.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent = nullptr);
    ~MainWindow() override;

    // "프로젝트 닫기"로 창을 닫았으면 true. main()이 시작 화면을 다시 띄운다.
    bool returnToStart() const { return returnToStart_; }

    // 창 없이 확인하기 어려운 편집 흐름(객체 추가·이동·Undo·저장·재로드·스크립트 실행)을 자동 검사한다.
    bool runSelfTest();
    // 객체를 선택하고 이벤트 탭을 연다 (명령행 --event 옵션, 스크린샷 확인용).
    void showObjectEvent(const QString& objectId);
    void startEventTest();
    // 데이터베이스 탭을 열고 종류/항목을 선택한다. fieldsTab이면 "속성 정의" 탭을 보여 준다.
    void showDatabase(const QString& typeKey, const QString& recordId, bool fieldsTab);
    // 저장한 뒤 게임 실행 화면(NaruPlayer)을 현재 장면부터 띄운다. extraArgs는 자동 검사·촬영용 player 옵션.
    bool runGame(const QStringList& extraArgs = {});
    void stopGame();

private:
    void createActions();
    void createDocks();

    void rebuildTree();
    void rebuildScriptList();
    void showScene(const QString& sceneId);
    void onTreeSelection();
    void onCanvasSelection(const QString& objectId);
    void onObjectMoved(const QString& objectId, double x, double y);
    void setSelectionTarget(const QString& sceneId, const QString& objectId);
    void openEventTab();

    void addScene();
    void removeCurrentScene();
    void setStartScene();
    void addObject();
    void deleteSelection();

    void newScript();
    void editGlobalScripts();
    void toggleGlobalScript(const QString& fileName);
    bool checkGlobalScripts();
    ScriptEditor* openScript(const QString& path);
    ScriptEditor* currentScript() const;
    void closeTab(int index);
    bool saveAll();
    void checkSyntax();
    void runScript();

    void appendLog(const naru::LogEntry& entry);
    void readGameOutput();
    void updateGameActions();
    void showVariables(const naru::Variables& variables);
    void jumpToLog(QTreeWidgetItem* item);
    void updateTitle();
    bool hasUnsavedChanges() const;
    bool confirmClose();

    void closeEvent(QCloseEvent* event) override;

    ProjectDocument* doc_;
    naru::python::PythonRuntime* python_;

    QTreeWidget* tree_ = nullptr;
    QListWidget* scripts_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    SceneView* sceneView_ = nullptr;
    EventEditor* eventEditor_ = nullptr;
    DatabaseEditor* databaseEditor_ = nullptr;
    Inspector* inspector_ = nullptr;
    QTreeWidget* log_ = nullptr;
    QTableWidget* variablesTable_ = nullptr;

    QAction* addObjectAction_ = nullptr;
    QAction* deleteAction_ = nullptr;
    QAction* runGameAction_ = nullptr;
    QAction* stopGameAction_ = nullptr;

    QProcess* game_ = nullptr;
    QByteArray gameOutput_;
    int lastGameExitCode_ = -1;

    QString currentSceneId_;
    bool syncing_ = false;
    bool returnToStart_ = false;
    QStringList logMessages_;
};
