#include "ScriptEditor.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QSaveFile>
#include <QTextBlock>

ScriptEditor::ScriptEditor(QWidget* parent) : QPlainTextEdit(parent) {
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
}

bool ScriptEditor::load(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    setPlainText(QString::fromUtf8(file.readAll()));
    document()->setModified(false);
    path_ = QFileInfo(path).absoluteFilePath();
    return true;
}

bool ScriptEditor::save(QString* error) {
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(toPlainText().toUtf8());
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    document()->setModified(false);
    return true;
}

void ScriptEditor::goToLine(int line) {
    QTextBlock block = document()->findBlockByLineNumber(line - 1);
    if (line <= 0 || !block.isValid()) {
        return;
    }
    QTextCursor cursor(block);
    cursor.select(QTextCursor::LineUnderCursor);
    setTextCursor(cursor);
    setFocus();
}
