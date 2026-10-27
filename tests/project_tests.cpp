#include "TestMain.h"

#include "naru/project/ProjectIO.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

using namespace naru;
namespace fs = std::filesystem;

namespace {

// 테스트마다 새 임시 폴더를 만들고 끝나면 지운다. 한글 경로 처리도 함께 검사한다.
struct TempDir {
    fs::path path;
    TempDir() {
        std::random_device rd;
        path = fs::temp_directory_path() / fs::path(u8"naru_테스트_" + std::u8string(
            reinterpret_cast<const char8_t*>(std::to_string(rd()).c_str())));
        fs::remove_all(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void writeText(const fs::path& file, const std::string& text) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
}

bool hasErrorContaining(const ProjectLoadResult& r, const std::string& file, const std::string& text) {
    for (const auto& e : r.errors) {
        if (e.file == file && e.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("새 프로젝트는 폴더 구조·기본 장면·예제 스크립트를 만든다") {
    TempDir dir;
    auto created = ProjectIO::create(dir.path, "내 게임");
    CHECK(created.ok());
    CHECK(ProjectIO::isProjectDirectory(dir.path));
    CHECK(fs::is_regular_file(dir.path / "scenes" / "scene_001.json"));
    CHECK(fs::is_regular_file(dir.path / "scripts" / "main.py"));
    CHECK(fs::is_directory(dir.path / "assets"));
    CHECK(created.project->startSceneId == "scene_001");
    CHECK(created.project->scenes.size() == 1);
    CHECK(created.project->scenes[0].objects.size() == 2);
}

TEST_CASE("저장한 프로젝트를 다시 열면 같은 내용이다") {
    TempDir dir;
    auto created = ProjectIO::create(dir.path, "왕복 테스트");
    Project project = *created.project;
    Scene second;
    second.id = project.newId("scene");
    second.name = "두 번째 장면";
    second.width = 640;
    second.background = "#000000";
    SceneObject o;
    o.id = project.newId("obj");
    o.name = "문";
    o.x = 12.5;
    o.y = -3;
    o.color = "#ffffff";
    // 모든 명령 종류와 중첩 분기를 저장·불러오기 한다.
    o.event.trigger = EventTrigger::Touch;
    EventCommand sayCmd;
    sayCmd.type = EventCommandType::Say;
    sayCmd.speaker = "문지기";
    sayCmd.text = "누구냐?\n두 번째 줄";
    EventCommand waitCmd;
    waitCmd.type = EventCommandType::Wait;
    waitCmd.seconds = 0.75;
    EventCommand setCmd;
    setCmd.type = EventCommandType::SetVariable;
    setCmd.variable = "gold";
    setCmd.op = VariableOp::Subtract;
    setCmd.value = std::int64_t{5};
    EventCommand callCmd;
    callCmd.type = EventCommandType::CallScript;
    callCmd.script = "door.py";
    callCmd.function = "open";
    EventCommand commentCmd;
    commentCmd.type = EventCommandType::Comment;
    commentCmd.text = "메모";
    EventCommand inner;
    inner.type = EventCommandType::IfVariable;
    inner.variable = "name";
    inner.compare = CompareOp::NotEqual;
    inner.value = std::string("리아");
    inner.thenCommands = {sayCmd};
    EventCommand outer;
    outer.type = EventCommandType::IfVariable;
    outer.variable = "rate";
    outer.compare = CompareOp::LessEqual;
    outer.value = 1.5;
    outer.thenCommands = {inner, waitCmd};
    outer.elseCommands = {setCmd, callCmd};
    EventCommand logCmd;
    logCmd.type = EventCommandType::Log;
    logCmd.text = "끝";
    EventCommand boolSet = setCmd;
    boolSet.op = VariableOp::Set;
    boolSet.value = true;
    EventCommand noneSet = boolSet;
    noneSet.value = Value{};
    o.event.commands = {commentCmd, outer, logCmd, boolSet, noneSet};
    second.objects.push_back(o);
    project.scenes.push_back(second);
    project.startSceneId = second.id;

    CHECK(ProjectIO::save(project, dir.path).empty());
    auto loaded = ProjectIO::load(dir.path);
    CHECK(loaded.ok());
    CHECK(loaded.project.has_value() && *loaded.project == project);
}

TEST_CASE("비어 있지 않은 폴더에는 새 프로젝트를 만들지 않는다") {
    TempDir dir;
    fs::create_directories(dir.path);
    writeText(dir.path / "other.txt", "x");
    auto r = ProjectIO::create(dir.path, "x");
    CHECK(!r.ok());
    CHECK(!fs::exists(dir.path / "project.json"));
}

TEST_CASE("삭제한 장면의 파일은 저장 시 정리된다") {
    TempDir dir;
    Project project = *ProjectIO::create(dir.path, "x").project;
    Scene extra;
    extra.id = "scene_002";
    project.scenes.push_back(extra);
    ProjectIO::save(project, dir.path);
    CHECK(fs::exists(dir.path / "scenes" / "scene_002.json"));
    project.scenes.pop_back();
    CHECK(ProjectIO::save(project, dir.path).empty());
    CHECK(!fs::exists(dir.path / "scenes" / "scene_002.json"));
    CHECK(fs::exists(dir.path / "scenes" / "scene_001.json"));
}

TEST_CASE("장면 파일이 없으면 파일 이름과 함께 오류를 보고한다") {
    TempDir dir;
    ProjectIO::create(dir.path, "x");
    fs::remove(dir.path / "scenes" / "scene_001.json");
    auto r = ProjectIO::load(dir.path);
    CHECK(!r.ok());
    CHECK(hasErrorContaining(r, "scenes/scene_001.json", "열 수 없습니다"));
}

TEST_CASE("새 형식 버전·깨진 JSON·ID 불일치·위험한 ID·중복 ID를 거부한다") {
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "project.json", R"({"formatVersion": 99, "scenes": []})");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "project.json", "새로운 형식"));
    }
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "scenes" / "scene_001.json", "{ 깨진");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "JSON"));
    }
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "scenes" / "scene_001.json", R"({"formatVersion": 1, "id": "other"})");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "다릅니다"));
    }
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "project.json", R"({"formatVersion": 1, "scenes": ["../escape"]})");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "project.json", "쓸 수 없는 문자"));
    }
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "scenes" / "scene_001.json",
                  R"({"formatVersion": 1, "id": "scene_001", "objects": [{"id": "a"}, {"id": "a"}]})");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "중복"));
    }
}

TEST_CASE("알 수 없는 이벤트 명령과 과도한 중첩을 거부한다") {
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        writeText(dir.path / "scenes" / "scene_001.json",
                  R"({"formatVersion": 1, "id": "scene_001", "objects": [{"id": "a",
                      "event": {"commands": [{"type": "explode"}]}}]})");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "explode"));
    }
    {
        TempDir dir;
        ProjectIO::create(dir.path, "x");
        std::string nested = R"({"type": "log", "text": "x"})";
        for (int i = 0; i < 40; ++i) {
            nested = R"({"type": "if_variable", "variable": "v", "then": [)" + nested + "]}";
        }
        writeText(dir.path / "scenes" / "scene_001.json",
                  R"({"formatVersion": 1, "id": "scene_001", "objects": [{"id": "a", "event": {"commands": [)" +
                      nested + "]}}]}");
        CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "너무 깊습니다"));
    }
}

TEST_CASE("필드 형식이 틀리면 형식 오류로 보고한다") {
    TempDir dir;
    ProjectIO::create(dir.path, "x");
    writeText(dir.path / "scenes" / "scene_001.json",
              R"({"formatVersion": 1, "id": "scene_001", "objects": [{"id": "a", "x": "왼쪽"}]})");
    CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "'x'은(는) 숫자이어야 합니다"));
    writeText(dir.path / "scenes" / "scene_001.json", R"({"formatVersion": 1, "id": "scene_001", "objects": "x"})");
    CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "'objects'은(는) 배열"));
    writeText(dir.path / "scenes" / "scene_001.json", R"({"formatVersion": 1, "id": "scene_001", "objects": [3]})");
    CHECK(hasErrorContaining(ProjectIO::load(dir.path), "scenes/scene_001.json", "장면 객체"));
    writeText(dir.path / "project.json", R"({"formatVersion": 1, "scenes": [1]})");
    CHECK(hasErrorContaining(ProjectIO::load(dir.path), "project.json", "장면 ID 문자열"));
}

TEST_CASE("newId는 사용하지 않은 번호를 만든다") {
    Project p;
    CHECK(p.newId("obj") == "obj_001");
    Scene s;
    s.id = "scene_001";
    s.objects.push_back({"obj_001"});
    s.objects.push_back({"obj_002"});
    p.scenes.push_back(s);
    CHECK(p.newId("obj") == "obj_003");
    CHECK(p.newId("scene") == "scene_002");
}

TEST_MAIN()
