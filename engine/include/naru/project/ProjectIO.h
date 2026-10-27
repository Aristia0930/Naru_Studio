#pragma once

#include "naru/project/Project.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace naru {

struct ProjectError {
    std::string file; // 프로젝트 폴더 기준 상대 경로
    std::string message;
};

struct ProjectLoadResult {
    std::optional<Project> project;
    std::vector<ProjectError> errors;

    bool ok() const { return project.has_value() && errors.empty(); }
    std::string errorText() const;
};

// 프로젝트 폴더 구조:
//   project.json        이름·형식 버전·시작 장면·장면 순서
//   scenes/<id>.json    장면과 배치 객체
//   scripts/            Python 게임 스크립트
//   assets/             이미지·음원 (가져오기 기능은 이후 단계)
class ProjectIO {
public:
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kProjectFile = "project.json";

    static bool isProjectDirectory(const std::filesystem::path& dir);

    // 비어 있거나 없는 폴더에 새 프로젝트(기본 장면·예제 스크립트 포함)를 만든다.
    static ProjectLoadResult create(const std::filesystem::path& dir, const std::string& name);
    static ProjectLoadResult load(const std::filesystem::path& dir);
    // 장면 파일은 임시 파일에 쓴 뒤 교체한다. 프로젝트에서 지운 장면의 파일은 scenes/에서 삭제한다.
    static std::vector<ProjectError> save(const Project& project, const std::filesystem::path& dir);

    static std::filesystem::path scenesDirectory(const std::filesystem::path& dir) { return dir / "scenes"; }
    static std::filesystem::path scriptsDirectory(const std::filesystem::path& dir) { return dir / "scripts"; }
    static std::filesystem::path assetsDirectory(const std::filesystem::path& dir) { return dir / "assets"; }
};

} // namespace naru
