#include "naru/project/Project.h"

#include <cstdio>
#include <cstdlib>
#include <set>

namespace naru {

SceneObject* Scene::findObject(const std::string& objectId) {
    int i = indexOfObject(objectId);
    return i < 0 ? nullptr : &objects[static_cast<std::size_t>(i)];
}

const SceneObject* Scene::findObject(const std::string& objectId) const {
    int i = indexOfObject(objectId);
    return i < 0 ? nullptr : &objects[static_cast<std::size_t>(i)];
}

int Scene::indexOfObject(const std::string& objectId) const {
    for (std::size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == objectId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Scene* Project::findScene(const std::string& sceneId) {
    int i = indexOfScene(sceneId);
    return i < 0 ? nullptr : &scenes[static_cast<std::size_t>(i)];
}

const Scene* Project::findScene(const std::string& sceneId) const {
    int i = indexOfScene(sceneId);
    return i < 0 ? nullptr : &scenes[static_cast<std::size_t>(i)];
}

int Project::indexOfScene(const std::string& sceneId) const {
    for (std::size_t i = 0; i < scenes.size(); ++i) {
        if (scenes[i].id == sceneId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string Project::newId(const std::string& prefix) const {
    std::set<std::string> used;
    for (const auto& scene : scenes) {
        used.insert(scene.id);
        for (const auto& object : scene.objects) {
            used.insert(object.id);
        }
    }
    for (int n = 1;; ++n) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "_%03d", n);
        std::string candidate = prefix + buffer;
        if (!used.contains(candidate)) {
            return candidate;
        }
    }
}

} // namespace naru
