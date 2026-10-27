#pragma once

#include "naru/python/PythonRuntime.h"
#include "naru/runtime/GameScripts.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace naru::python {

// 프로젝트의 전역 스크립트들을 Python 모듈로 읽어 두고 게임 훅을 부른다.
class PythonGameScripts : public GameScripts {
public:
    // paths: 등록 순서대로의 전역 스크립트 경로 (UTF-8 절대 경로).
    PythonGameScripts(PythonRuntime& python, std::vector<std::string> paths);
    ~PythonGameScripts() override;

    int load(CommandContext& ctx) override;
    HookStatus call(const std::string& hook, CommandContext& ctx) override;

    // 읽어 둔 전역 스크립트 중 파일 이름(scripts/ 안, 예: "game.py")이 같은 것의 경로.
    // 이벤트의 "스크립트 호출"이 이 파일이면 새로 읽지 않고 실행 중인 모듈의 함수를 부른다.
    std::optional<std::string> loadedPath(const std::string& fileName) const;

private:
    PythonRuntime& python_;
    std::vector<std::string> paths_;
    std::vector<std::string> loaded_;
    // 오류가 난 (스크립트, 훅). 이번 실행에서는 다시 부르지 않는다.
    std::set<std::pair<std::string, std::string>> failed_;
};

} // namespace naru::python
