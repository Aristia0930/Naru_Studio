#include "naru/project/ImageLayout.h"
#include <algorithm>
#include "naru/runtime/Audio.h"
#include <cmath>

namespace naru {
ImageRect objectImageRect(const SceneObject& object, double pixelWidth, double pixelHeight) {
    const auto& s = object.image;
    const double w = (s.width > 0 ? s.width : pixelWidth) * s.scaleX;
    const double h = (s.height > 0 ? s.height : pixelHeight) * s.scaleY;
    return {object.x + s.offsetX - s.anchorX * w, object.y + s.offsetY - s.anchorY * h, w, h};
}

ImageRect backgroundImageRect(const Scene& scene, double pixelWidth, double pixelHeight) {
    if (pixelWidth <= 0 || pixelHeight <= 0) return {};
    const auto& b = scene.backgroundImage;
    double w = pixelWidth, h = pixelHeight, x = 0, y = 0;
    if (b.mode == "stretch") { w = scene.width; h = scene.height; }
    if (b.mode == "fit" || b.mode == "fill") {
        const double rx = scene.width / w, ry = scene.height / h;
        const double ratio = b.mode == "fit" ? std::min(rx, ry) : std::max(rx, ry);
        w *= ratio; h *= ratio;
    }
    w *= b.scale; h *= b.scale;
    if (b.mode != "original" && b.mode != "repeat") { x = (scene.width - w) / 2; y = (scene.height - h) / 2; }
    x += b.offsetX; y += b.offsetY;
    if (b.mode == "repeat") { x = std::fmod(x, w); y = std::fmod(y, h); if (x > 0) x -= w; if (y > 0) y -= h; }
    return {x, y, w, h};
}

const Asset* findImageAsset(const Project& p, const std::string& id) {
    for (const auto& a : p.assets) if (a.id == id && a.kind == "image") return &a;
    return nullptr;
}

std::vector<std::string> imageUses(const Project& p, const std::string& id) {
    auto uses = audioUses(p, id);
    const auto dbUses = databaseAssetUses(p.database, id);
    uses.insert(uses.end(), dbUses.begin(), dbUses.end());
    if (id.empty()) return uses;
    for (const auto& s : p.scenes) {
        if (s.backgroundImage.asset == id) uses.push_back(s.name + " / 장면 배경");
        for (const auto& o : s.objects) if (o.image.asset == id) uses.push_back(s.name + " / " + o.name + " / 객체 이미지");
    }
    return uses;
}

std::string validateImages(const Project& p) {
    const auto range = [](double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    const auto reference = [&](const std::string& id) {
        for (const auto& a : p.assets) if (a.id == id && a.kind != "image") return false;
        return true;
    };
    for (const auto& s : p.scenes) {
        const auto& b = s.backgroundImage;
        if (!reference(b.asset) || (b.mode != "fit" && b.mode != "fill" && b.mode != "stretch" && b.mode != "original" && b.mode != "repeat") ||
            !range(b.offsetX, -100000, 100000) || !range(b.offsetY, -100000, 100000) || !range(b.scale, .01, 100) || !range(b.opacity, 0, 1))
            return s.id + ": 장면 배경 이미지 설정이 올바르지 않습니다.";
        for (const auto& o : s.objects) {
            const auto& i = o.image;
            if (!reference(i.asset) || !range(i.width, 0, 100000) || !range(i.height, 0, 100000) ||
                !range(i.offsetX, -100000, 100000) || !range(i.offsetY, -100000, 100000) ||
                !range(i.scaleX, .01, 100) || !range(i.scaleY, .01, 100) || !range(i.anchorX, 0, 1) || !range(i.anchorY, 0, 1) ||
                !range(i.rotation, -360, 360) || !range(i.opacity, 0, 1) || i.order < -10000 || i.order > 10000)
                return s.id + "/" + o.id + ": 객체 이미지 설정이 올바르지 않습니다.";
        }
    }
    return {};
}

std::vector<const SceneObject*> drawingOrder(const Scene& s) {
    std::vector<const SceneObject*> out;
    for (const auto& o : s.objects) out.push_back(&o);
    std::stable_sort(out.begin(), out.end(), [](const auto* a, const auto* b) { return a->image.order < b->image.order; });
    return out;
}
} // namespace naru
