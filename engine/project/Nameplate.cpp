#include "naru/project/Nameplate.h"
#include "naru/project/ImageLayout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace naru {
bool isValidNameText(const std::string& text) {
    return text.size() <= 512 && std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
bool isValidNameBackgroundColor(const std::string& color) {
    return color.size() == 7 && color[0] == '#' && color.find_first_not_of("0123456789abcdefABCDEF", 1) == std::string::npos;
}
bool isValidNamePosition(const std::string& position) { return position == "above" || position == "below"; }
std::string objectNameplateText(const SceneObject& object, bool debug) {
    if (!debug && (!object.visible || !object.showName)) return {};
    if (!debug) return object.nameText.empty() ? object.name : object.nameText;
    return object.name + (object.visible ? "" : " (숨김)") + "\n[" + object.id + "]";
}
NameplateAnchor objectNameplateAnchor(const SceneObject& object, double imageWidth, double imageHeight, bool below) {
    if (object.image.asset.empty() || imageWidth <= 0 || imageHeight <= 0)
        return {object.x + object.width / 2, below ? object.y + object.height + 6 : object.y - 6};
    const auto r = objectImageRect(object, imageWidth, imageHeight);
    const double pivotX = object.x + object.image.offsetX, pivotY = object.y + object.image.offsetY;
    const double angle = object.image.rotation * std::numbers::pi / 180;
    double left = std::numeric_limits<double>::max(), right = -left, top = left, bottom = -left;
    for (double x : {r.x, r.x + r.width}) for (double y : {r.y, r.y + r.height}) {
        const double px = pivotX + (x-pivotX)*std::cos(angle) - (y-pivotY)*std::sin(angle);
        const double py = pivotY + (x-pivotX)*std::sin(angle) + (y-pivotY)*std::cos(angle);
        left = std::min(left,px); right = std::max(right,px); top = std::min(top,py); bottom = std::max(bottom,py);
    }
    return {(left+right)/2, below ? bottom+6 : top-6};
}
}
