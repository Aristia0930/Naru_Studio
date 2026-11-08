#pragma once
#include "naru/project/ImageLayout.h"
#include "naru/project/Nameplate.h"
#include "naru/core/Log.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <map>

class ImageRenderer {
public:
    ImageRenderer(SDL_Renderer* renderer, const naru::Project& project, std::filesystem::path directory, naru::Logger& logger);
    ~ImageRenderer();
    ImageRenderer(const ImageRenderer&) = delete;
    ImageRenderer& operator=(const ImageRenderer&) = delete;
    void draw(const naru::Scene& scene);
    void clear();
    naru::NameplateAnchor nameplateAnchor(const naru::SceneObject& object, bool below = false);
private:
    SDL_Texture* texture(const std::string& id);
    SDL_Renderer* renderer_;
    const naru::Project& project_;
    std::filesystem::path directory_;
    naru::Logger& logger_;
    std::map<std::string, SDL_Texture*> cache_;
    std::string scene_;
    bool comInitialized_ = false;
};
