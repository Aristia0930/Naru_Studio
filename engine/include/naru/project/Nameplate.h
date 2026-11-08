#pragma once
#include "naru/project/Project.h"

namespace naru {
struct NameplateAnchor { double x = 0, y = 0; };
bool isValidNameText(const std::string& text);
bool isValidNameBackgroundColor(const std::string& color);
bool isValidNamePosition(const std::string& position);
std::string objectNameplateText(const SceneObject& object, bool debug);
// Center above/below the rotated picture (or collision rectangle), with a small gap.
NameplateAnchor objectNameplateAnchor(const SceneObject& object, double imageWidth = 0, double imageHeight = 0, bool below = false);
}
