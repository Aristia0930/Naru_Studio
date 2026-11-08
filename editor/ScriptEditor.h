#pragma once

#include <QPlainTextEdit>
#include <QString>

class QPaintEvent;
class QKeyEvent;
class QWidget;

// 프로젝트 scripts/ 안의 Python 파일 하나를 편집하는 탭.
class ScriptEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ScriptEditor(QWidget* parent = nullptr);

    bool load(const QString& path, QString* error);
    bool save(QString* error);
    QString path() const { return path_; }
    bool isModified() const { return document()->isModified(); }
    void goToLine(int line);
    int lineNumberAreaWidth() const;
    void paintLineNumbers(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    QWidget* lineNumberArea_ = nullptr;
    QString path_;
};
