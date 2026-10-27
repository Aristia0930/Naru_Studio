#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QListWidget;

// 프로그램 시작 화면: 새 프로젝트 만들기 / 프로젝트 열기 / 최근 프로젝트.
class StartDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartDialog(QWidget* parent = nullptr);

    // 선택한 프로젝트 폴더. createRequested()가 true면 아직 만들지 않은 새 프로젝트 폴더다.
    QString projectDirectory() const { return projectDirectory_; }
    QString newProjectName() const { return newProjectName_; }
    bool createRequested() const { return create_; }

    static QStringList recentProjects();
    static void addRecentProject(const QString& directory);

private:
    void newProject();
    void openProject();
    void openRecent();
    void reloadRecent();

    QListWidget* recent_;
    QString projectDirectory_;
    QString newProjectName_;
    bool create_ = false;
};
