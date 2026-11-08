#include "ImageRenderer.h"
#include <cstdio>
#include <charconv>
#include <fstream>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

namespace {
SDL_Surface* decodeNative(std::vector<unsigned char>& bytes) {
#ifdef _WIN32
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size()))) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame))) return nullptr;
    UINT width = 0, height = 0;
    if (FAILED(frame->GetSize(&width, &height)) || !width || !height || static_cast<std::uint64_t>(width) * height > 32 * 1024 * 1024 ||
        FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return nullptr;
    auto* surface = SDL_CreateSurface(static_cast<int>(width), static_cast<int>(height), SDL_PIXELFORMAT_RGBA32);
    if (surface && FAILED(converter->CopyPixels(nullptr, surface->pitch, surface->h * surface->pitch, static_cast<BYTE*>(surface->pixels)))) {
        SDL_DestroySurface(surface); surface = nullptr;
    }
    return surface;
#else
    (void)bytes;
    SDL_SetError("JPG decoding currently requires Windows WIC");
    return nullptr;
#endif
}
SDL_Color color(const std::string& text) {
    unsigned int r = 74, g = 144, b = 217;
    if (text.size() == 7 && text[0] == '#') {
        std::from_chars(text.data() + 1, text.data() + 3, r, 16);
        std::from_chars(text.data() + 3, text.data() + 5, g, 16);
        std::from_chars(text.data() + 5, text.data() + 7, b, 16);
    }
    return {static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b), 255};
}
}

ImageRenderer::ImageRenderer(SDL_Renderer* renderer, const naru::Project& project, std::filesystem::path directory, naru::Logger& logger)
    : renderer_(renderer), project_(project), directory_(std::move(directory)), logger_(logger) {
#ifdef _WIN32
    comInitialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
#endif
}
ImageRenderer::~ImageRenderer() {
    clear();
#ifdef _WIN32
    if (comInitialized_) CoUninitialize();
#endif
}
void ImageRenderer::clear() {
    for (auto& [id, image] : cache_) { (void)id; if (image) SDL_DestroyTexture(image); }
    cache_.clear();
}

SDL_Texture* ImageRenderer::texture(const std::string& id) {
    if (id.empty()) return nullptr;
    if (auto found = cache_.find(id); found != cache_.end()) return found->second;
    SDL_Texture* texture = nullptr;
    const auto* asset = naru::findImageAsset(project_, id);
    if (asset && naru::validateAssets({*asset}, directory_).empty()) {
        const auto path = directory_ / std::filesystem::path(std::u8string(asset->path.begin(), asset->path.end()));
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        const auto length = input ? static_cast<std::streamoff>(input.tellg()) : 0;
        if (length > 0 && length <= 64 * 1024 * 1024) {
            std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
            input.seekg(0);
            if (input.read(reinterpret_cast<char*>(bytes.data()), length)) {
                SDL_Surface* surface = nullptr;
                if (bytes.size() >= 24 && bytes[0] == 137 && bytes[1] == 'P') {
                    const auto be32 = [&](std::size_t n) { return (std::uint64_t(bytes[n]) << 24) | (std::uint64_t(bytes[n+1]) << 16) | (std::uint64_t(bytes[n+2]) << 8) | bytes[n+3]; };
                    if (be32(16) && be32(20) && be32(16) * be32(20) <= 32 * 1024 * 1024)
                        surface = SDL_LoadSurface_IO(SDL_IOFromConstMem(bytes.data(), bytes.size()), true);
                } else surface = decodeNative(bytes);
                if (surface) {
                    if (std::uint64_t(surface->w) * surface->h <= 32 * 1024 * 1024) texture = SDL_CreateTextureFromSurface(renderer_, surface);
                    SDL_DestroySurface(surface);
                }
            }
        }
    }
    if (texture) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    else logger_.warning("이미지를 읽지 못해 색으로 표시합니다: " + id);
    cache_[id] = texture;
    return texture;
}

naru::NameplateAnchor ImageRenderer::nameplateAnchor(const naru::SceneObject& object, bool below) {
    if (object.visible) if (auto* image = texture(object.image.asset))
        return naru::objectNameplateAnchor(object, image->w, image->h, below);
    return naru::objectNameplateAnchor(object, 0, 0, below);
}

void ImageRenderer::draw(const naru::Scene& scene) {
    if (scene_ != scene.id) { clear(); scene_ = scene.id; }
    SDL_SetRenderLogicalPresentation(renderer_, scene.width, scene.height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_Rect clip{0, 0, scene.width, scene.height}; SDL_SetRenderClipRect(renderer_, &clip);
    if (auto* image = texture(scene.backgroundImage.asset)) {
        const auto r = naru::backgroundImageRect(scene, image->w, image->h);
        const auto& b = scene.backgroundImage;
        SDL_SetTextureAlphaModFloat(image, static_cast<float>(b.opacity));
        SDL_SetTextureScaleMode(image, b.smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
        SDL_FRect rect{static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.width), static_cast<float>(r.height)};
        if (b.mode == "repeat") {
            rect.w = static_cast<float>(scene.width - r.x); rect.h = static_cast<float>(scene.height - r.y);
            SDL_RenderTextureTiled(renderer_, image, nullptr, static_cast<float>(b.scale), &rect);
        } else SDL_RenderTexture(renderer_, image, nullptr, &rect);
    }
    for (const auto* object : naru::drawingOrder(scene)) {
        if (!object->visible) continue;
        const auto& o = *object;
        if (auto* image = texture(o.image.asset)) {
            const auto r = naru::objectImageRect(o, image->w, image->h);
            SDL_FRect rect{static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.width), static_cast<float>(r.height)};
            SDL_FPoint pivot{static_cast<float>(r.width * o.image.anchorX), static_cast<float>(r.height * o.image.anchorY)};
            SDL_SetTextureAlphaModFloat(image, static_cast<float>(o.image.opacity));
            SDL_SetTextureScaleMode(image, o.image.smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
            auto flip = static_cast<SDL_FlipMode>((o.image.flipX ? SDL_FLIP_HORIZONTAL : 0) | (o.image.flipY ? SDL_FLIP_VERTICAL : 0));
            SDL_RenderTextureRotated(renderer_, image, nullptr, &rect, o.image.rotation, &pivot, flip);
        } else {
            const auto c = color(o.color); SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            const SDL_FRect rect{static_cast<float>(o.x), static_cast<float>(o.y), static_cast<float>(o.width), static_cast<float>(o.height)};
            SDL_RenderFillRect(renderer_, &rect);
        }
    }
    SDL_SetRenderClipRect(renderer_, nullptr);
}
