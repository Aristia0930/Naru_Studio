#pragma once
#include "naru/project/Project.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <map>
#include <memory>
#include <string>
class ImageRenderer;

class NameplateRenderer {
public:
    NameplateRenderer();
    ~NameplateRenderer();
    void clear();
    void draw(SDL_Renderer*, TTF_Font*, const naru::Scene&, bool debug, ImageRenderer&);
private:
    struct Label;
    std::map<std::string,std::unique_ptr<Label>> cache_;
    std::string scene_;
};
