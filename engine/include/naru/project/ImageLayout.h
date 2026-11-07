#pragma once
#include "naru/project/Project.h"

namespace naru {
struct ImageRect { double x = 0, y = 0, width = 0, height = 0; };
ImageRect objectImageRect(const SceneObject& object, double pixelWidth, double pixelHeight);
ImageRect backgroundImageRect(const Scene& scene, double pixelWidth, double pixelHeight);
const Asset* findImageAsset(const Project& project, const std::string& id);
std::vector<std::string> imageUses(const Project& project, const std::string& id);
// Missing IDs/files are repairable. Known references of a non-image kind are invalid.
std::string validateImages(const Project& project);
std::vector<const SceneObject*> drawingOrder(const Scene& scene);
} // namespace naru
