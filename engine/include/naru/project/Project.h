#pragma once

#include "naru/data/Database.h"
#include "naru/events/EventData.h"

#include <string>
#include <vector>

namespace naru {

// 장면에 배치된 객체. 이미지·컴포넌트는 이후 단계에서 추가한다.
struct SceneObject {
    std::string id;
    std::string name;
    double x = 0.0;
    double y = 0.0;
    double width = 64.0;
    double height = 64.0;
    std::string color = "#4a90d9";
    // 게임 실행 중 다른 객체(플레이어)가 지나갈 수 있는가. false면 벽처럼 막는다.
    bool passable = false;
    ObjectEvent event;

    bool operator==(const SceneObject&) const = default;
};

struct Scene {
    std::string id;
    std::string name;
    int width = 1280;
    int height = 720;
    std::string background = "#202830";
    // 게임 실행 중 방향키로 조작하는 객체. 비어 있으면 플레이어가 없는 장면(컷신 등)이다.
    std::string playerObjectId;
    std::vector<SceneObject> objects;

    SceneObject* findObject(const std::string& objectId);
    const SceneObject* findObject(const std::string& objectId) const;
    int indexOfObject(const std::string& objectId) const;

    bool operator==(const Scene&) const = default;
};

// 편집 문서로서의 프로젝트. 폴더 위치는 ProjectIO가 다루며 여기에는 저장하지 않는다.
struct Project {
    int formatVersion = 1;
    std::string name;
    std::string startSceneId;
    std::vector<Scene> scenes;
    Database database;

    Scene* findScene(const std::string& sceneId);
    const Scene* findScene(const std::string& sceneId) const;
    int indexOfScene(const std::string& sceneId) const;

    // prefix_001 형식의, 프로젝트 안에서 쓰이지 않은 ID를 만든다 (prefix: "scene", "obj").
    std::string newId(const std::string& prefix) const;

    bool operator==(const Project&) const = default;
};

} // namespace naru
