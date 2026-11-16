#pragma once
#include "naru/project/Project.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace naru {
struct MapResizeSummary { std::size_t croppedTiles = 0, outsideObjects = 0; };
// Keeps grid coordinates anchored at the top left; caller owns confirmation and Undo.
inline MapResizeSummary resizeTileMap(Scene& scene, int width, int height) {
    if (scene.width <= 0 || width < 32 || width > 32768 || height < 32 || height > 32768 || scene.tileSize < 8 || scene.tileSize > 256)
        throw std::invalid_argument("맵 크기는 32~32768, 타일 크기는 8~256이어야 합니다");
    MapResizeSummary result;
    const int oldColumns = (scene.width + scene.tileSize - 1) / scene.tileSize;
    const int columns = (width + scene.tileSize - 1) / scene.tileSize;
    const int rows = (height + scene.tileSize - 1) / scene.tileSize;
    for (auto& layer : scene.tileLayers) {
        std::map<int, int> cells;
        for (const auto& [cell,tile] : layer.cells) {
            const int x=cell%oldColumns, y=cell/oldColumns;
            if (x>=columns || y>=rows) ++result.croppedTiles;
            else cells.emplace(y*columns+x,tile);
        }
        layer.cells=std::move(cells);
    }
    for (const auto& object : scene.objects)
        if (object.x<0 || object.y<0 || object.x+object.width>width || object.y+object.height>height) ++result.outsideObjects;
    scene.width=width;scene.height=height;
    return result;
}
struct MapCamera { double x = 0, y = 0; int width = 0, height = 0; };
inline MapCamera mapCamera(const Scene& scene) {
    MapCamera view{0, 0, scene.cameraWidth ? scene.cameraWidth : scene.width,
        scene.cameraHeight ? scene.cameraHeight : scene.height};
    if (scene.cameraWidth) if (const auto* p = scene.findObject(scene.playerObjectId)) {
        view.x = std::clamp(p->x + p->width / 2 - view.width / 2., 0., std::max(0., scene.width - double(view.width)));
        view.y = std::clamp(p->y + p->height / 2 - view.height / 2., 0., std::max(0., scene.height - double(view.height)));
    }
    return view;
}
template<class Visitor>
void visitSolidTiles(const Scene& scene, double x, double y, double width, double height, Visitor visitor) {
    int size = scene.tileSize, columns = (scene.width + size - 1) / size;
    int x0 = std::max(0, int(std::floor(x / size))), y0 = std::max(0, int(std::floor(y / size)));
    int x1 = std::min(columns - 1, int(std::ceil((x + width) / size)) - 1);
    int y1 = std::min((scene.height + size - 1) / size - 1, int(std::ceil((y + height) / size)) - 1);
    for (const auto& layer : scene.tileLayers) if (layer.visible) {
        for (int row = y0; row <= y1; ++row) for (int col = x0; col <= x1; ++col) {
            auto cell = layer.cells.find(row * columns + col);
            if (cell != layer.cells.end() && std::find(layer.solidTiles.begin(), layer.solidTiles.end(), cell->second) != layer.solidTiles.end())
                visitor(col * size, row * size, size);
        }
    }
}
}
