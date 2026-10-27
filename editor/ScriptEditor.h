#pragma once

#include <QPlainTextEdit>
#include <QString>

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

private:
    QString path_;
};
