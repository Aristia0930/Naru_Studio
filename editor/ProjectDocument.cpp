#include "ProjectDocument.h"

#include "naru/project/ProjectIO.h"

#include <QDir>

#include <algorithm>
#include <filesystem>

namespace {

std::filesystem::path toPath(const QString& dir) {
    return std::filesystem::path(dir.toStdWString());
}

QString joinErrors(const std::vector<naru::ProjectError>& errors) {
    QStringList lines;
    for (const auto& e : errors) {
        lines << (e.file.empty() ? QString() : QString::fromStdString(e.file) + QStringLiteral(": ")) +
                     QString::fromStdString(e.message);
    }
    return lines.join('\n');
}

} // namespace

ProjectDocument::ProjectDocument(QString directory, naru::Project project)
    : directory_(std::move(directory)), project_(std::move(project)) {
    connect(&undoStack_, &QUndoStack::cleanChanged, this, [this](bool clean) { emit modifiedChanged(!clean); });
}

std::unique_ptr<ProjectDocument> ProjectDocument::open(const QString& directory, QString* error) {
    QString dir = QDir(directory).absolutePath();
    auto result = naru::ProjectIO::load(toPath(dir));
    if (!result.ok()) {
        if (error) {
            *error = joinErrors(result.errors);
        }
        return nullptr;
    }
    return std::unique_ptr<ProjectDocument>(new ProjectDocument(dir, std::move(*result.project)));
}

std::unique_ptr<ProjectDocument> ProjectDocument::create(const QString& directory, const QString& name,
                                                         QString* error) {
    QString dir = QDir(directory).absolutePath();
    auto result = naru::ProjectIO::create(toPath(dir), name.toStdString());
    if (!result.ok()) {
        if (error) {
            *error = joinErrors(result.errors);
        }
        return nullptr;
    }
    return std::unique_ptr<ProjectDocument>(new ProjectDocument(dir, std::move(*result.project)));
}

QString ProjectDocument::scriptsDirectory() const {
    return QDir(directory_).filePath(QStringLiteral("scripts"));
}

bool ProjectDocument::save(QString* error) {
    auto errors = naru::ProjectIO::save(project_, toPath(directory_));
    if (!errors.empty()) {
        if (error) {
            *error = joinErrors(errors);
        }
        return false;
    }
    undoStack_.setClean();
    return true;
}

void ProjectDocument::insertScene(int index, const naru::Scene& scene) {
    index = std::clamp(index, 0, static_cast<int>(project_.scenes.size()));
    project_.scenes.insert(project_.scenes.begin() + index, scene);
    if (project_.startSceneId.empty()) {
        project_.startSceneId = scene.id;
    }
    emit scenesChanged();
}

void ProjectDocument::removeScene(const std::string& sceneId) {
    int index = project_.indexOfScene(sceneId);
    if (index < 0) {
        return;
    }
    project_.scenes.erase(project_.scenes.begin() + index);
    if (project_.startSceneId == sceneId) {
        project_.startSceneId = project_.scenes.empty() ? std::string() : project_.scenes.front().id;
    }
    emit scenesChanged();
}

void ProjectDocument::setSceneInfo(const naru::Scene& info) {
    naru::Scene* scene = project_.findScene(info.id);
    if (!scene) {
        return;
    }
    scene->name = info.name;
    scene->width = info.width;
    scene->height = info.height;
    scene->background = info.background;
    scene->playerObjectId = info.playerObjectId;
    emit sceneChanged(QString::fromStdString(info.id));
    emit scenesChanged();
}

void ProjectDocument::setStartScene(const std::string& sceneId) {
    project_.startSceneId = sceneId;
    emit scenesChanged();
}

void ProjectDocument::insertObject(const std::string& sceneId, int index, const naru::SceneObject& object) {
    naru::Scene* scene = project_.findScene(sceneId);
    if (!scene) {
        return;
    }
    index = std::clamp(index, 0, static_cast<int>(scene->objects.size()));
    scene->objects.insert(scene->objects.begin() + index, object);
    emit sceneChanged(QString::fromStdString(sceneId));
}

void ProjectDocument::removeObject(const std::string& sceneId, const std::string& objectId) {
    naru::Scene* scene = project_.findScene(sceneId);
    int index = scene ? scene->indexOfObject(objectId) : -1;
    if (index < 0) {
        return;
    }
    scene->objects.erase(scene->objects.begin() + index);
    if (scene->playerObjectId == objectId) {
        scene->playerObjectId.clear(); // 실행 취소하면 RemoveObjectCommand가 되돌린다
    }
    emit sceneChanged(QString::fromStdString(sceneId));
}

void ProjectDocument::setDatabase(const naru::Database& database) {
    project_.database = database;
    emit databaseChanged();
}

void ProjectDocument::setObject(const std::string& sceneId, const naru::SceneObject& object) {
    naru::Scene* scene = project_.findScene(sceneId);
    naru::SceneObject* target = scene ? scene->findObject(object.id) : nullptr;
    if (!target) {
        return;
    }
    *target = object;
    emit objectChanged(QString::fromStdString(sceneId), QString::fromStdString(object.id));
}

void ProjectDocument::setGlobalScripts(const std::vector<std::string>& scripts) {
    project_.globalScripts = scripts;
    emit globalScriptsChanged();
}

QStringList ProjectDocument::scriptFiles() const {
    return QDir(scriptsDirectory()).entryList({QStringLiteral("*.py")}, QDir::Files, QDir::Name);
}

bool ProjectDocument::isGlobalScript(const QString& fileName) const {
    const auto& list = project_.globalScripts;
    return std::find(list.begin(), list.end(), fileName.toStdString()) != list.end();
}
