#pragma once

#include "naru/core/Value.h"
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
    // false면 게임 실행 중 그리지 않고, 막지도 막히지도 않으며 조사·접촉 이벤트도 실행되지 않는다.
    // (자동 실행·병렬 처리 이벤트는 그대로 실행된다 — 보이지 않는 진행 담당 객체로 쓸 수 있게)
    bool visible = true;
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

// 프로젝트 공용 변수 정의. 게임 실행·이벤트 테스트를 시작할 때 이 처음 값이 들어간다.
// 처음 값의 형식이 변수의 종류다: 정수(int64)·소수(double)·글자(string)·스위치(bool, 참/거짓).
struct VariableDef {
    std::string name;
    Value initial = std::int64_t{0};
    std::string description;

    bool operator==(const VariableDef&) const = default;
};

// 편집 문서로서의 프로젝트. 폴더 위치는 ProjectIO가 다루며 여기에는 저장하지 않는다.
struct Project {
    int formatVersion = 1;
    std::string name;
    std::string startSceneId;
    std::vector<Scene> scenes;
    Database database;
    // 게임 전체에서 실행되는 전역 스크립트 (scripts/ 안의 파일 이름, 등록 순서대로 훅을 부른다).
    std::vector<std::string> globalScripts;
    // 공용 변수·스위치 정의 (편집 화면에 보이는 순서).
    std::vector<VariableDef> variables;

    Scene* findScene(const std::string& sceneId);
    const Scene* findScene(const std::string& sceneId) const;
    int indexOfScene(const std::string& sceneId) const;
    const VariableDef* findVariable(const std::string& variableName) const;
    // 정의한 변수들의 처음 값. 게임·테스트 실행 세션은 이 값에서 시작한다.
    Variables initialVariables() const;

    // prefix_001 형식의, 프로젝트 안에서 쓰이지 않은 ID를 만든다 (prefix: "scene", "obj").
    std::string newId(const std::string& prefix) const;

    bool operator==(const Project&) const = default;
};

} // namespace naru
