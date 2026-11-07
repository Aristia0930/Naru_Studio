#include "naru/project/Asset.h"

#include <algorithm>
#include <set>

namespace naru {

std::string validateAssetGroups(const std::vector<std::string>& groups, const std::vector<Asset>& assets) {
    std::set<std::string> names;
    for (const auto& name : groups) {
        if (name.find_first_not_of(" \t\r\n") == std::string::npos || !names.insert(name).second)
            return "에셋 그룹 이름이 비어 있거나 중복됩니다.";
    }
    for (const auto& asset : assets) {
        if (!asset.group.empty() && !names.contains(asset.group))
            return "에셋이 없는 그룹을 참조합니다: " + asset.id;
    }
    return {};
}

namespace {
// Resolve the existing parent explicitly: MSVC can report an error from
// weakly_canonical for a missing final component. Missing assets are repairable.
std::filesystem::path resolveMissing(std::filesystem::path path, std::error_code& ec) {
    namespace fs = std::filesystem;
    path = fs::absolute(path, ec).lexically_normal();
    if (ec) return {};
    fs::path tail;
    while (true) {
        auto status = fs::symlink_status(path, ec);
        if (ec && ec != std::errc::no_such_file_or_directory) return {};
        ec.clear();
        if (fs::exists(status)) break;
        auto parent = path.parent_path();
        if (parent == path || parent.empty()) { ec = std::make_error_code(std::errc::no_such_file_or_directory); return {}; }
        tail = path.filename() / tail;
        path = parent;
    }
    auto resolved = fs::canonical(path, ec);
    return ec ? fs::path{} : (resolved / tail).lexically_normal();
}
}

bool isValidAssetId(const std::string& id) {
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

std::string validateAssets(const std::vector<Asset>& assets, const std::filesystem::path& directory) {
    namespace fs = std::filesystem;
    std::set<std::string> ids, paths;
    for (const auto& a : assets) {
        if (!isValidAssetId(a.id) || !ids.insert(a.id).second) return "에셋 ID가 올바르지 않거나 중복됩니다: " + a.id;
        if (a.name.find_first_not_of(" \t\r\n") == std::string::npos) return "에셋 이름이 비어 있습니다: " + a.id;
        if (a.kind != "image" && a.kind != "audio" && a.kind != "font") return "알 수 없는 에셋 종류: " + a.kind;
        if (a.sha256.size() != 64 || !std::all_of(a.sha256.begin(), a.sha256.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) return "에셋 SHA-256이 올바르지 않습니다: " + a.id;
        if (a.path.find_first_of("\\:") != std::string::npos || a.path.find('\0') != std::string::npos ||
            !a.path.starts_with("assets/") || a.path.size() <= 7 || !paths.insert(a.path).second)
            return "에셋 경로가 올바르지 않거나 중복됩니다: " + a.id;
        if (a.path.find("//") != std::string::npos) return "에셋 경로에 빈 폴더 이름을 쓸 수 없습니다: " + a.id;
        fs::path relative = fs::path(std::u8string(a.path.begin(), a.path.end()));
        for (const auto& part : relative) {
            if (part == ".." || part == "." || part.empty()) return "에셋 경로에 상대 이동을 쓸 수 없습니다: " + a.id;
        }
        std::error_code ec;
        auto base = resolveMissing(directory, ec);
        if (ec) return "프로젝트 경로를 확인할 수 없습니다: " + a.id;
        auto resolved = resolveMissing(directory / relative, ec);
        if (ec) return "에셋 경로를 확인할 수 없습니다: " + a.id;
        auto inside = resolved.lexically_relative(base);
        if (inside.empty() || inside.is_absolute() || *inside.begin() == "..")
            return "에셋은 프로젝트 폴더 안에 있어야 합니다: " + a.id;
        if (a.sourceName.find_first_of("/\\:") != std::string::npos || a.sourceName.find('\0') != std::string::npos)
            return "에셋 원본 이름에는 경로를 쓸 수 없습니다: " + a.id;
    }
    return {};
}

} // namespace naru
