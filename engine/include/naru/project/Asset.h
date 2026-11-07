#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace naru {

// Paths are project-relative. Import sources store only the original filename.
struct Asset {
    std::string id;
    std::string name;
    std::string kind; // image, audio, font
    std::string path;
    std::string sourceName;
    std::string sha256;
    std::string group; // Empty means ungrouped; independent of the copied file path.
    bool operator==(const Asset&) const = default;
};

// Missing files are allowed, so the editor can open a project and repair them.
// Unsafe paths (including symlinks outside the project) are rejected.
std::string validateAssets(const std::vector<Asset>& assets, const std::filesystem::path& directory);
std::string validateAssetGroups(const std::vector<std::string>& groups, const std::vector<Asset>& assets);
bool isValidAssetId(const std::string& id);

} // namespace naru
