#include "TestMain.h"
#include "ScriptEditor.h"
#include <QApplication>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextOption>

namespace {
void press(ScriptEditor& editor, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&editor, &event);
}
}

TEST_CASE("스크립트 줄 번호·공백 표시와 Python 들여쓰기") {
    ScriptEditor editor;
    CHECK(editor.findChild<QWidget*>("scriptLineNumbers") != nullptr);
    CHECK(editor.document()->defaultTextOption().flags().testFlag(QTextOption::ShowTabsAndSpaces));
    editor.setPlainText("def run(ctx):");
    auto cursor = editor.textCursor(); cursor.movePosition(QTextCursor::End); editor.setTextCursor(cursor);
    press(editor, Qt::Key_Return);
    CHECK(editor.toPlainText() == "def run(ctx):\n    ");
    editor.insertPlainText("if True:"); press(editor, Qt::Key_Return);
    CHECK(editor.toPlainText().endsWith("if True:\n        "));
    editor.insertPlainText("ctx.log('ok')");
    press(editor, Qt::Key_Backtab, Qt::ShiftModifier);
    CHECK(editor.toPlainText().endsWith("if True:\n    ctx.log('ok')"));
    editor.undo(); CHECK(editor.toPlainText().endsWith("if True:\n        ctx.log('ok')"));
    editor.setPlainText("x\ny\nz");
    cursor = editor.textCursor(); cursor.setPosition(0); cursor.setPosition(4, QTextCursor::KeepAnchor); editor.setTextCursor(cursor);
    press(editor, Qt::Key_Tab);
    CHECK(editor.toPlainText() == "    x\n    y\nz");
    editor.undo(); CHECK(editor.toPlainText() == "x\ny\nz");
    editor.goToLine(2); CHECK(editor.textCursor().blockNumber() == 1);
    editor.setPlainText("x"); cursor = editor.textCursor(); cursor.movePosition(QTextCursor::End); editor.setTextCursor(cursor);
    press(editor, Qt::Key_Tab); CHECK(editor.toPlainText() == "x   ");
}
