#pragma once

#include "naru/project/Project.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUndoStack>

#include <memory>

// 열린 프로젝트 하나. 모든 변경은 UndoCommands를 통해 아래 변경 함수로 들어오고, 화면은 시그널로 갱신한다.
class ProjectDocument : public QObject {
    Q_OBJECT

public:
    static std::unique_ptr<ProjectDocument> open(const QString& directory, QString* error);
    static std::unique_ptr<ProjectDocument> create(const QString& directory, const QString& name, QString* error);

    const naru::Project& project() const { return project_; }
    QString directory() const { return directory_; }
    QString scriptsDirectory() const;
    QString name() const { return QString::fromStdString(project_.name); }
    QUndoStack* undoStack() { return &undoStack_; }
    bool isModified() const { return !undoStack_.isClean(); }

    bool save(QString* error);

    // 아래 함수는 Undo 명령에서만 호출한다.
    void insertScene(int index, const naru::Scene& scene);
    void removeScene(const std::string& sceneId);
    void setSceneInfo(const naru::Scene& info); // 객체 목록을 제외한 장면 속성
    void setStartScene(const std::string& sceneId);
    void insertObject(const std::string& sceneId, int index, const naru::SceneObject& object);
    void removeObject(const std::string& sceneId, const std::string& objectId);
    void setObject(const std::string& sceneId, const naru::SceneObject& object);
    void setDatabase(const naru::Database& database);
    void setGlobalScripts(const std::vector<std::string>& scripts);

    // scripts/ 안의 .py 파일 이름 (이름순).
    QStringList scriptFiles() const;
    bool isGlobalScript(const QString& fileName) const;

signals:
    void globalScriptsChanged();
    void databaseChanged();
    void scenesChanged();
    void sceneChanged(const QString& sceneId);
    void objectChanged(const QString& sceneId, const QString& objectId);
    void modifiedChanged(bool modified);

private:
    ProjectDocument(QString directory, naru::Project project);

    QString directory_;
    naru::Project project_;
    QUndoStack undoStack_;
};
