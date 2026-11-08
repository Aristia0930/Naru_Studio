#include "NameplateRenderer.h"
#include "ImageRenderer.h"
#include "naru/project/Nameplate.h"
#include <algorithm>
#include <set>

namespace {
// Bound old object names as well, and remove line/control characters from each displayed row.
std::string lineText(std::string text) {
    for (char& c : text) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
    if (text.size() > 512) {
        std::size_t end=512;
        while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
        text.resize(end); text += "...";
    }
    return text;
}
void popCodepoint(std::string& text) {
    if (text.empty()) return;
    auto index=text.size()-1;
    while (index && (static_cast<unsigned char>(text[index]) & 0xc0)==0x80) --index;
    text.resize(index);
}
std::string fitLine(TTF_Font* font, std::string text, int width) {
    text=lineText(std::move(text));
    auto fits=[&](const std::string& value) {
        if (!font) return value.size()*SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE <= static_cast<std::size_t>(width);
        int w=0,h=0; return TTF_GetStringSize(font,value.c_str(),0,&w,&h) && w<=width;
    };
    if (fits(text)) return text;
    do { popCodepoint(text); } while (!text.empty() && !fits(text+"..."));
    return text+"...";
}
}
struct NameplateRenderer::Label {
    SDL_Texture* texture=nullptr;
    std::string source, displayed;
    int width=0, height=0, limit=0;
    bool debug=false, hidden=false;
    ~Label() { if (texture) SDL_DestroyTexture(texture); }
};
NameplateRenderer::NameplateRenderer() = default;
NameplateRenderer::~NameplateRenderer() = default;
void NameplateRenderer::clear() { cache_.clear(); }
void NameplateRenderer::draw(SDL_Renderer* renderer, TTF_Font* font, const naru::Scene& scene, bool debug, ImageRenderer& images) {
    if (scene_ != scene.id) { clear(); scene_=scene.id; }
    std::set<std::string> used;
    const int limit=std::max(1,std::min(240,scene.width-16));
    for (const auto* o : naru::drawingOrder(scene)) {
        const auto source=naru::objectNameplateText(*o,debug);
        if (source.empty()) continue;
        const bool below = !debug && o->namePosition == "below";
        auto anchor=images.nameplateAnchor(*o, below);
        // Completely offscreen objects do not pile labels onto the screen edge.
        if (anchor.x < 0 || anchor.x > scene.width || o->y+o->height < 0 || o->y > scene.height) continue;
        used.insert(o->id);
        auto& entry=cache_[o->id];
        if (!entry || entry->source != source || entry->debug != debug || entry->hidden != !o->visible || entry->limit != limit) {
            entry=std::make_unique<Label>();
            entry->source=source; entry->debug=debug; entry->hidden=!o->visible; entry->limit=limit;
            const auto color=debug ? (o->visible ? SDL_Color{255,220,120,255} : SDL_Color{190,190,190,255}) : SDL_Color{255,255,255,255};
            if (debug) entry->displayed=fitLine(font,o->name+(o->visible ? "" : " (숨김)"),limit)+"\n"+fitLine(font,"["+o->id+"]",limit);
            else entry->displayed=fitLine(font,source,limit);
            if (font) {
                auto* surface=TTF_RenderText_Blended_Wrapped(font,entry->displayed.c_str(),0,color,limit);
                if (surface) {
                    entry->width=surface->w; entry->height=surface->h;
                    entry->texture=SDL_CreateTextureFromSurface(renderer,surface); SDL_DestroySurface(surface);
                }
            } else {
                entry->width=static_cast<int>(std::min<std::size_t>(limit,entry->displayed.size()*SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE));
                entry->height=SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE*(debug ? 2 : 1);
            }
        }
        const float w=static_cast<float>(entry->width+8), h=static_cast<float>(entry->height+6);
        const float x=static_cast<float>(std::clamp(anchor.x-w/2,0.0,std::max<double>(0.0,scene.width-w)));
        const float y=static_cast<float>(std::clamp(below ? anchor.y : anchor.y-h,0.0,std::max<double>(0.0,scene.height-h)));
        SDL_BlendMode previous; SDL_GetRenderDrawBlendMode(renderer,&previous); SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        if (debug || !o->nameBackgroundTransparent) {
            const auto hex = std::stoul(debug ? "0c1018" : o->nameBackgroundColor.substr(1), nullptr, 16);
            SDL_SetRenderDrawColor(renderer, static_cast<Uint8>((hex >> 16) & 255), static_cast<Uint8>((hex >> 8) & 255), static_cast<Uint8>(hex & 255), 210);
            const SDL_FRect box{x,y,w,h}; SDL_RenderFillRect(renderer,&box);
        }
        SDL_SetRenderDrawBlendMode(renderer,previous);
        if (entry->texture) { SDL_FRect destination{x+4,y+3,static_cast<float>(entry->width),static_cast<float>(entry->height)}; SDL_RenderTexture(renderer,entry->texture,nullptr,&destination); }
        else if (!font) { SDL_SetRenderDrawColor(renderer,255,255,255,255); SDL_RenderDebugText(renderer,x+4,y+3,entry->displayed.c_str()); }
    }
    std::erase_if(cache_,[&](const auto& pair) { return !used.contains(pair.first); });
}
