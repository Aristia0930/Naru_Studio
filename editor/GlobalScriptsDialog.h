#pragma once

#include <QDialog>
#include <QStringList>

#include <string>
#include <vector>

class QLabel;
class QListWidget;

namespace naru::python {
class PythonRuntime;
}

// 전역 스크립트 등록 창: scripts/의 파일을 체크해 등록하고, 위/아래로 실행 순서를 정한다.
// 체크한 파일들을 바로 검사해 문법 오류·변수 이름 겹침을 보여 준다.
class GlobalScriptsDialog : public QDialog {
    Q_OBJECT

public:
    GlobalScriptsDialog(const QStringList& files, const std::vector<std::string>& registered, const QString& scriptsDir,
                        naru::python::PythonRuntime* python, QWidget* parent = nullptr);

    // 체크한 파일 (위에서부터 실행 순서).
    std::vector<std::string> selected() const;

private:
    void move(int delta);
    void recheck();

    QListWidget* list_;
    QLabel* status_;
    QString scriptsDir_;
    naru::python::PythonRuntime* python_;
};
