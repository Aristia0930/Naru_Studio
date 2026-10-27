#include "UndoCommands.h"

#include "ProjectDocument.h"

#include <QCoreApplication>

namespace {

QString label(const char* text, const std::string& name) {
    return QString::fromUtf8(text).arg(QString::fromStdString(name));
}

} // namespace

AddSceneCommand::AddSceneCommand(ProjectDocument* doc, naru::Scene scene, int index)
    : doc_(doc), scene_(std::move(scene)), index_(index) {
    setText(label("장면 추가: %1", scene_.name));
}

void AddSceneCommand::redo() { doc_->insertScene(index_, scene_); }
void AddSceneCommand::undo() { doc_->removeScene(scene_.id); }

RemoveSceneCommand::RemoveSceneCommand(ProjectDocument* doc, const std::string& sceneId)
    : doc_(doc), index_(doc->project().indexOfScene(sceneId)), previousStart_(doc->project().startSceneId) {
    if (const naru::Scene* scene = doc->project().findScene(sceneId)) {
        scene_ = *scene;
    }
    setText(label("장면 삭제: %1", scene_.name));
}

void RemoveSceneCommand::redo() { doc_->removeScene(scene_.id); }

void RemoveSceneCommand::undo() {
    doc_->insertScene(index_, scene_);
    doc_->setStartScene(previousStart_);
}

SetSceneInfoCommand::SetSceneInfoCommand(ProjectDocument* doc, naru::Scene before, naru::Scene after, int mergeKey)
    : doc_(doc), before_(std::move(before)), after_(std::move(after)), mergeKey_(mergeKey) {
    before_.objects.clear();
    after_.objects.clear();
    setText(label("장면 속성: %1", after_.name));
}

void SetSceneInfoCommand::redo() { doc_->setSceneInfo(after_); }
void SetSceneInfoCommand::undo() { doc_->setSceneInfo(before_); }

bool SetSceneInfoCommand::mergeWith(const QUndoCommand* other) {
    auto* o = static_cast<const SetSceneInfoCommand*>(other);
    if (o->after_.id != after_.id || o->mergeKey_ != mergeKey_) {
        return false;
    }
    after_ = o->after_;
    return true;
}

SetStartSceneCommand::SetStartSceneCommand(ProjectDocument* doc, std::string sceneId)
    : doc_(doc), before_(doc->project().startSceneId), after_(std::move(sceneId)) {
    setText(QStringLiteral("시작 장면 변경"));
}

void SetStartSceneCommand::redo() { doc_->setStartScene(after_); }
void SetStartSceneCommand::undo() { doc_->setStartScene(before_); }

AddObjectCommand::AddObjectCommand(ProjectDocument* doc, std::string sceneId, naru::SceneObject object, int index)
    : doc_(doc), sceneId_(std::move(sceneId)), object_(std::move(object)), index_(index) {
    setText(label("객체 추가: %1", object_.name));
}

void AddObjectCommand::redo() { doc_->insertObject(sceneId_, index_, object_); }
void AddObjectCommand::undo() { doc_->removeObject(sceneId_, object_.id); }

RemoveObjectCommand::RemoveObjectCommand(ProjectDocument* doc, std::string sceneId, const std::string& objectId)
    : doc_(doc), sceneId_(std::move(sceneId)), index_(-1) {
    if (const naru::Scene* scene = doc->project().findScene(sceneId_)) {
        index_ = scene->indexOfObject(objectId);
        if (index_ >= 0) {
            object_ = scene->objects[static_cast<std::size_t>(index_)];
            wasPlayer_ = scene->playerObjectId == objectId;
        }
    }
    setText(label("객체 삭제: %1", object_.name));
}

void RemoveObjectCommand::redo() { doc_->removeObject(sceneId_, object_.id); }
void RemoveObjectCommand::undo() {
    doc_->insertObject(sceneId_, index_, object_);
    if (const naru::Scene* scene = doc_->project().findScene(sceneId_); scene && wasPlayer_) {
        naru::Scene info = *scene;
        info.playerObjectId = object_.id;
        doc_->setSceneInfo(info);
    }
}

SetDatabaseCommand::SetDatabaseCommand(ProjectDocument* doc, naru::Database before, naru::Database after,
                                       const QString& text, QString mergeKey)
    : doc_(doc), before_(std::move(before)), after_(std::move(after)), mergeKey_(std::move(mergeKey)) {
    setText(text);
}

void SetDatabaseCommand::redo() { doc_->setDatabase(after_); }
void SetDatabaseCommand::undo() { doc_->setDatabase(before_); }

bool SetDatabaseCommand::mergeWith(const QUndoCommand* other) {
    auto* o = static_cast<const SetDatabaseCommand*>(other);
    if (o->mergeKey_ != mergeKey_) {
        return false;
    }
    after_ = o->after_;
    return true;
}

SetObjectCommand::SetObjectCommand(ProjectDocument* doc, std::string sceneId, naru::SceneObject before,
                                   naru::SceneObject after, int mergeKey)
    : doc_(doc), sceneId_(std::move(sceneId)), before_(std::move(before)), after_(std::move(after)),
      mergeKey_(mergeKey) {
    setText(label(before_.x != after_.x || before_.y != after_.y ? "객체 이동: %1" : "객체 속성: %1", after_.name));
}

void SetObjectCommand::redo() { doc_->setObject(sceneId_, after_); }
void SetObjectCommand::undo() { doc_->setObject(sceneId_, before_); }

bool SetObjectCommand::mergeWith(const QUndoCommand* other) {
    auto* o = static_cast<const SetObjectCommand*>(other);
    if (o->sceneId_ != sceneId_ || o->after_.id != after_.id || o->mergeKey_ != mergeKey_) {
        return false;
    }
    after_ = o->after_;
    return true;
}

SetGlobalScriptsCommand::SetGlobalScriptsCommand(ProjectDocument* doc, std::vector<std::string> after)
    : doc_(doc), before_(doc->project().globalScripts), after_(std::move(after)) {
    setText(QStringLiteral("전역 스크립트 변경"));
}

void SetGlobalScriptsCommand::redo() { doc_->setGlobalScripts(after_); }
void SetGlobalScriptsCommand::undo() { doc_->setGlobalScripts(before_); }
