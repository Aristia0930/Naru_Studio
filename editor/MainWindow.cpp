#include "MainWindow.h"

#include "naru/events/BuiltinCommands.h"
#include "naru/python/PythonRuntime.h"
#include "naru/runtime/Session.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextBlock>
#include <QToolBar>
#include <QTreeWidget>

#include <memory>
#include <vector>

namespace {

constexpr int kFileRole = Qt::UserRole;
constexpr int kLineRole = Qt::UserRole + 1;

const char* kNewScriptTemplate =
    "def run(ctx):\n"
    "    count = ctx.variables.get(\"visits\", 0) + 1\n"
    "    ctx.variables.set(\"visits\", count)\n"
    "    ctx.log(f\"방문 횟수: {count}\")\n";

QString levelLabel(naru::LogLevel level) {
    switch (level) {
    case naru::LogLevel::Info: return QStringLiteral("정보");
    case naru::LogLevel::Warning: return QStringLiteral("경고");
    case naru::LogLevel::Error: return QStringLiteral("오류");
    }
    return {};
}

} // namespace

MainWindow::MainWindow(naru::python::PythonRuntime* python, QWidget* parent)
    : QMainWindow(parent), python_(python) {
    editor_ = new QPlainTextEdit(this);
    editor_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    editor_->setLineWrapMode(QPlainTextEdit::NoWrap);
    editor_->setTabStopDistance(editor_->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    setCentralWidget(editor_);
    connect(editor_->document(), &QTextDocument::modificationChanged, this, &MainWindow::updateTitle);

    createActions();
    createDocks();

    statusLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel_);
    statusLabel_->setText(QStringLiteral("Python %1").arg(QString::fromStdString(python_->version()).section(' ', 0, 0)));

    newScript();
    resize(1100, 720);
}

void MainWindow::createActions() {
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("파일(&F)"));
    auto* runMenu = menuBar()->addMenu(QStringLiteral("실행(&R)"));
    auto* toolbar = addToolBar(QStringLiteral("기본"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto add = [this](QMenu* menu, const QString& text, QKeySequence key, auto slot) {
        auto* action = new QAction(text, this);
        action->setShortcut(key);
        connect(action, &QAction::triggered, this, slot);
        menu->addAction(action);
        return action;
    };

    auto* newAction = add(fileMenu, QStringLiteral("새 스크립트"), QKeySequence::New, &MainWindow::newScript);
    auto* openAction = add(fileMenu, QStringLiteral("열기..."), QKeySequence::Open, &MainWindow::openScriptDialog);
    auto* saveAction = add(fileMenu, QStringLiteral("저장"), QKeySequence::Save, &MainWindow::saveScript);
    add(fileMenu, QStringLiteral("다른 이름으로 저장..."), QKeySequence::SaveAs, &MainWindow::saveScriptAs);
    fileMenu->addSeparator();
    add(fileMenu, QStringLiteral("종료"), QKeySequence::Quit, &QWidget::close);

    auto* checkAction = add(runMenu, QStringLiteral("문법 검사"), QKeySequence(Qt::Key_F7), &MainWindow::checkSyntax);
    auto* runAction = add(runMenu, QStringLiteral("스크립트 실행"), QKeySequence(Qt::Key_F5), &MainWindow::runScript);

    toolbar->addAction(newAction);
    toolbar->addAction(openAction);
    toolbar->addAction(saveAction);
    toolbar->addSeparator();
    toolbar->addAction(checkAction);
    toolbar->addAction(runAction);
}

void MainWindow::createDocks() {
    log_ = new QTreeWidget(this);
    log_->setColumnCount(3);
    log_->setHeaderLabels({QStringLiteral("수준"), QStringLiteral("메시지"), QStringLiteral("위치")});
    log_->setRootIsDecorated(false);
    log_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    log_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    log_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    log_->header()->setStretchLastSection(false);
    connect(log_, &QTreeWidget::itemDoubleClicked, this, &MainWindow::jumpToLogLocation);

    auto* logDock = new QDockWidget(QStringLiteral("로그"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logDock->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);

    variablesTable_ = new QTableWidget(0, 2, this);
    variablesTable_->setHorizontalHeaderLabels({QStringLiteral("변수"), QStringLiteral("값")});
    variablesTable_->horizontalHeader()->setStretchLastSection(true);
    variablesTable_->verticalHeader()->hide();
    variablesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* varDock = new QDockWidget(QStringLiteral("실행 결과 변수"), this);
    varDock->setObjectName(QStringLiteral("variablesDock"));
    varDock->setWidget(variablesTable_);
    addDockWidget(Qt::RightDockWidgetArea, varDock);
}

void MainWindow::newScript() {
    if (!confirmDiscard()) {
        return;
    }
    editor_->setPlainText(QString::fromUtf8(kNewScriptTemplate));
    editor_->document()->setModified(false);
    currentPath_.clear();
    updateTitle();
}

void MainWindow::openScriptDialog() {
    if (!confirmDiscard()) {
        return;
    }
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("스크립트 열기"), QString(),
                                                QStringLiteral("Python 스크립트 (*.py)"));
    if (!path.isEmpty()) {
        openScript(path);
    }
}

bool MainWindow::openScript(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("열기 실패"), file.errorString());
        return false;
    }
    editor_->setPlainText(QString::fromUtf8(file.readAll()));
    editor_->document()->setModified(false);
    currentPath_ = QFileInfo(path).absoluteFilePath();
    updateTitle();
    return true;
}

bool MainWindow::saveScript() {
    if (currentPath_.isEmpty()) {
        return saveScriptAs();
    }
    QSaveFile file(currentPath_);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), file.errorString());
        return false;
    }
    file.write(editor_->toPlainText().toUtf8());
    if (!file.commit()) {
        QMessageBox::warning(this, QStringLiteral("저장 실패"), file.errorString());
        return false;
    }
    editor_->document()->setModified(false);
    statusBar()->showMessage(QStringLiteral("저장했습니다: %1").arg(currentPath_), 3000);
    return true;
}

bool MainWindow::saveScriptAs() {
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("다른 이름으로 저장"), currentPath_,
                                                QStringLiteral("Python 스크립트 (*.py)"));
    if (path.isEmpty()) {
        return false;
    }
    currentPath_ = QFileInfo(path).absoluteFilePath();
    updateTitle();
    return saveScript();
}

void MainWindow::checkSyntax() {
    if (!saveScript()) {
        return;
    }
    auto result = python_->checkSyntax(currentPath_.toStdString());
    if (result.ok) {
        appendLog({naru::LogLevel::Info, "문법 검사 통과", {currentPath_.toStdString(), 0, ""}, ""});
    } else {
        appendLog({naru::LogLevel::Error, result.error.type + ": " + result.error.message,
                   {result.error.file, result.error.line, ""}, result.error.traceback});
        moveCursorToLine(result.error.line);
    }
}

void MainWindow::runScript() {
    // 실행은 저장된 파일을 기준으로 한다. 편집 중인 내용을 먼저 저장해야 최신 코드가 반영된다.
    if (editor_->document()->isModified() || currentPath_.isEmpty()) {
        if (!saveScript()) {
            return;
        }
    }
    const std::string path = currentPath_.toStdString();

    naru::Session session(documentVariables_);
    session.logger().addSink([this](const naru::LogEntry& entry) { appendLog(entry); });
    session.logger().info("실행 시작: " + path);

    std::vector<std::unique_ptr<naru::Command>> commands;
    commands.push_back(std::make_unique<naru::CallCommand>(
        "script", [this, path](naru::CommandContext& ctx) -> std::string {
            auto result = python_->runFile(path, ctx);
            return result.ok ? std::string() : "Python 스크립트 오류 (" + result.error.type + ")";
        }));
    session.start(std::move(commands));

    // 현재 스크립트 명령은 즉시 끝나므로 남은 고정 틱이 없다. 대기형 명령은 M2에서 실시간 루프에 연결한다.
    session.logger().info(std::string("실행 종료: ") + naru::toString(session.state()));
    showVariables(session.variables());

    if (session.state() == naru::SessionState::Failed) {
        for (auto it = session.logger().entries().rbegin(); it != session.logger().entries().rend(); ++it) {
            if (it->source.line > 0 && QString::fromStdString(it->source.file) == currentPath_) {
                moveCursorToLine(it->source.line);
                break;
            }
        }
    }
}

void MainWindow::appendLog(const naru::LogEntry& entry) {
    auto* item = new QTreeWidgetItem(log_);
    item->setText(0, levelLabel(entry.level));
    item->setText(1, QString::fromStdString(entry.message));

    QString location;
    if (!entry.source.file.empty()) {
        location = QFileInfo(QString::fromStdString(entry.source.file)).fileName();
        if (entry.source.line > 0) {
            location += QStringLiteral(":%1").arg(entry.source.line);
        }
    }
    if (!entry.source.commandId.empty()) {
        location += (location.isEmpty() ? QString() : QStringLiteral(" ")) +
                    QStringLiteral("[%1]").arg(QString::fromStdString(entry.source.commandId));
    }
    item->setText(2, location);
    item->setData(0, kFileRole, QString::fromStdString(entry.source.file));
    item->setData(0, kLineRole, entry.source.line);
    if (!entry.detail.empty()) {
        item->setToolTip(1, QString::fromStdString(entry.detail));
    }
    if (entry.level == naru::LogLevel::Error) {
        for (int c = 0; c < 3; ++c) {
            item->setForeground(c, QColor(200, 40, 40));
        }
    } else if (entry.level == naru::LogLevel::Warning) {
        for (int c = 0; c < 3; ++c) {
            item->setForeground(c, QColor(180, 120, 0));
        }
    }
    log_->scrollToItem(item);
}

void MainWindow::showVariables(const naru::Variables& variables) {
    variablesTable_->setRowCount(0);
    for (const auto& [name, value] : variables.all()) {
        int row = variablesTable_->rowCount();
        variablesTable_->insertRow(row);
        variablesTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(name)));
        variablesTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(naru::toDisplayString(value))));
    }
}

void MainWindow::jumpToLogLocation(QTreeWidgetItem* item, int) {
    QString file = item->data(0, kFileRole).toString();
    int line = item->data(0, kLineRole).toInt();
    if (line > 0 && file == currentPath_) {
        moveCursorToLine(line);
    }
}

void MainWindow::moveCursorToLine(int line) {
    if (line <= 0) {
        return;
    }
    QTextBlock block = editor_->document()->findBlockByLineNumber(line - 1);
    if (!block.isValid()) {
        return;
    }
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

void MainWindow::updateTitle() {
    QString name = currentPath_.isEmpty() ? QStringLiteral("새 스크립트") : QFileInfo(currentPath_).fileName();
    setWindowTitle(QStringLiteral("%1%2 — Naru Studio")
                       .arg(name, editor_->document()->isModified() ? QStringLiteral(" *") : QString()));
}

bool MainWindow::confirmDiscard() {
    if (editor_ == nullptr || !editor_->document()->isModified()) {
        return true;
    }
    auto answer = QMessageBox::question(this, QStringLiteral("변경 사항"),
                                        QStringLiteral("저장하지 않은 변경 사항이 있습니다. 저장할까요?"),
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        return saveScript();
    }
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmDiscard()) {
        event->accept();
    } else {
        event->ignore();
    }
}
