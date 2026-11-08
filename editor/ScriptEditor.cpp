#include "ScriptEditor.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextOption>

namespace {
class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(ScriptEditor* editor) : QWidget(editor), editor_(editor) {
        setObjectName(QStringLiteral("scriptLineNumbers"));
    }
    QSize sizeHint() const override { return {editor_->lineNumberAreaWidth(), 0}; }
protected:
    void paintEvent(QPaintEvent* event) override { editor_->paintLineNumbers(event); }
private:
    ScriptEditor* editor_;
};
}

ScriptEditor::ScriptEditor(QWidget* parent) : QPlainTextEdit(parent) {
    setObjectName(QStringLiteral("scriptEditor"));
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    auto option = document()->defaultTextOption();
    option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
    document()->setDefaultTextOption(option);
    setToolTip(QStringLiteral("왼쪽 숫자는 줄 번호입니다. 점은 공백, 화살표는 탭입니다. Tab은 공백 4칸 들여쓰기, Shift+Tab은 내어쓰기입니다."));
    lineNumberArea_ = new LineNumberArea(this);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &ScriptEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &ScriptEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, lineNumberArea_, QOverload<>::of(&QWidget::update));
    updateLineNumberAreaWidth();
}

int ScriptEditor::lineNumberAreaWidth() const {
    int digits = 1;
    for (int n = qMax(1, blockCount()); n >= 10; n /= 10) ++digits;
    return 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void ScriptEditor::updateLineNumberAreaWidth() { setViewportMargins(lineNumberAreaWidth(), 0, 0, 0); }

void ScriptEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) lineNumberArea_->scroll(0, dy);
    else lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    if (rect.contains(viewport()->rect())) updateLineNumberAreaWidth();
}

void ScriptEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    lineNumberArea_->setGeometry(area.left(), area.top(), lineNumberAreaWidth(), area.height());
}

void ScriptEditor::paintLineNumbers(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), palette().alternateBase());
    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid() && top <= event->rect().bottom()) {
        const int height = qRound(blockBoundingRect(block).height());
        if (block.isVisible() && top + height >= event->rect().top()) {
            painter.setPen(number == textCursor().blockNumber() ? palette().text().color() : palette().placeholderText().color());
            painter.drawText(0, top, lineNumberArea_->width() - 5, fontMetrics().height(), Qt::AlignRight, QString::number(number + 1));
        }
        top += height;
        block = block.next();
        ++number;
    }
}

void ScriptEditor::keyPressEvent(QKeyEvent* event) {
    const bool shiftTab = event->key() == Qt::Key_Backtab || (event->key() == Qt::Key_Tab && event->modifiers() == Qt::ShiftModifier);
    if (shiftTab || (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier)) {
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection() && !shiftTab) {
            const int spaces = 4 - cursor.positionInBlock() % 4;
            cursor.insertText(QString(spaces, QLatin1Char(' ')));
            setTextCursor(cursor);
            return;
        }
        const int start = cursor.selectionStart();
        const int end = cursor.selectionEnd();
        QTextBlock first = document()->findBlock(start);
        QTextBlock last = document()->findBlock(end);
        if (end > start && last.position() == end) last = last.previous();
        const int lastNumber = last.blockNumber();
        cursor.beginEditBlock();
        for (QTextBlock block = first; block.isValid() && block.blockNumber() <= lastNumber; block = block.next()) {
            QTextCursor line(block);
            if (shiftTab) {
                const QString text = block.text();
                int remove = 0;
                if (text.startsWith(QLatin1Char('\t'))) remove = 1;
                else while (remove < 4 && remove < text.size() && text.at(remove) == QLatin1Char(' ')) ++remove;
                line.setPosition(block.position() + remove, QTextCursor::KeepAnchor);
                line.removeSelectedText();
            } else line.insertText(QStringLiteral("    "));
        }
        cursor.endEditBlock();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier) {
        QTextCursor cursor = textCursor();
        cursor.beginEditBlock();
        if (cursor.hasSelection()) cursor.removeSelectedText();
        const QString before = cursor.block().text().left(cursor.positionInBlock());
        const auto match = QRegularExpression(QStringLiteral("^[ \\t]*")).match(before);
        QString indent = match.captured();
        if (before.trimmed().endsWith(QLatin1Char(':'))) indent += QStringLiteral("    ");
        cursor.insertText(QStringLiteral("\n") + indent);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
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
