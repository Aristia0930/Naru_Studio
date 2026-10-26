#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"

#include <QMainWindow>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace naru::python {
class PythonRuntime;
}

// M0 제작툴 창: Python 게임 스크립트 편집·문법 검사·즉시 실행과 로그/변수 확인.
// 장면·이벤트·인스펙터 편집은 아직 구현하지 않았으며 화면에 가짜 버튼을 두지 않는다.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(naru::python::PythonRuntime* python, QWidget* parent = nullptr);

    bool openScript(const QString& path);

private slots:
    void newScript();
    void openScriptDialog();
    bool saveScript();
    bool saveScriptAs();
    void checkSyntax();
    void runScript();
    void jumpToLogLocation(QTreeWidgetItem* item, int column);

private:
    void createActions();
    void createDocks();
    void appendLog(const naru::LogEntry& entry);
    void showVariables(const naru::Variables& variables);
    void updateTitle();
    bool confirmDiscard();
    void moveCursorToLine(int line);

    void closeEvent(QCloseEvent* event) override;

    naru::python::PythonRuntime* python_;
    QPlainTextEdit* editor_ = nullptr;
    QTreeWidget* log_ = nullptr;
    QTableWidget* variablesTable_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QString currentPath_;
    // 편집 문서의 초기 변수. 실행은 항상 이 값의 복사본으로 시작한다.
    naru::Variables documentVariables_;
};
