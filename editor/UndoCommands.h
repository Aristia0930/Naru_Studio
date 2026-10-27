#pragma once

#include "naru/project/Project.h"

#include <QUndoCommand>

class ProjectDocument;

class AddSceneCommand : public QUndoCommand {
public:
    AddSceneCommand(ProjectDocument* doc, naru::Scene scene, int index);
    void redo() override;
    void undo() override;

private:
    ProjectDocument* doc_;
    naru::Scene scene_;
    int index_;
};

class RemoveSceneCommand : public QUndoCommand {
public:
    RemoveSceneCommand(ProjectDocument* doc, const std::string& sceneId);
    void redo() override;
    void undo() override;

private:
    ProjectDocument* doc_;
    naru::Scene scene_;
    int index_;
    std::string previousStart_;
};

class SetSceneInfoCommand : public QUndoCommand {
public:
    SetSceneInfoCommand(ProjectDocument* doc, naru::Scene before, naru::Scene after, int mergeKey);
    void redo() override;
    void undo() override;
    int id() const override { return mergeKey_ >= 0 ? 2000 : -1; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    ProjectDocument* doc_;
    naru::Scene before_;
    naru::Scene after_;
    int mergeKey_;
};

class SetStartSceneCommand : public QUndoCommand {
public:
    SetStartSceneCommand(ProjectDocument* doc, std::string sceneId);
    void redo() override;
    void undo() override;

private:
    ProjectDocument* doc_;
    std::string before_;
    std::string after_;
};

class AddObjectCommand : public QUndoCommand {
public:
    AddObjectCommand(ProjectDocument* doc, std::string sceneId, naru::SceneObject object, int index);
    void redo() override;
    void undo() override;

private:
    ProjectDocument* doc_;
    std::string sceneId_;
    naru::SceneObject object_;
    int index_;
};

class RemoveObjectCommand : public QUndoCommand {
public:
    RemoveObjectCommand(ProjectDocument* doc, std::string sceneId, const std::string& objectId);
    void redo() override;
    void undo() override;

private:
    ProjectDocument* doc_;
    std::string sceneId_;
    naru::SceneObject object_;
    int index_;
};

// 객체 속성 변경. mergeKey가 같은 연속 변경(예: 숫자 칸을 연달아 조절)은 Undo 한 번으로 묶는다.
// mergeKey < 0이면 묶지 않는다 (예: 캔버스 드래그는 놓을 때 한 번만 기록).
class SetObjectCommand : public QUndoCommand {
public:
    SetObjectCommand(ProjectDocument* doc, std::string sceneId, naru::SceneObject before,
                     naru::SceneObject after, int mergeKey = -1);
    void redo() override;
    void undo() override;
    int id() const override { return mergeKey_ >= 0 ? 1000 : -1; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    ProjectDocument* doc_;
    std::string sceneId_;
    naru::SceneObject before_;
    naru::SceneObject after_;
    int mergeKey_;
};
