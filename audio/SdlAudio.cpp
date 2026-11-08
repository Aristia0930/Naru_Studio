#include "naru/audio/SdlAudio.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>

namespace naru {
struct SdlAudio::Voice {
    DecodedAudio pcm;
    SDL_AudioStream* stream = nullptr;
    std::size_t cursor = 0;
    bool loop = false;
    bool flushed = false;
    double volume = 100, fade = 0, remaining = 0;
    ~Voice() { if (stream) SDL_DestroyAudioStream(stream); }
    bool feed() {
        const auto target = static_cast<std::size_t>(pcm.spec.freq * pcm.spec.channels / 4);
        int bytes = SDL_GetAudioStreamQueued(stream);
        if (bytes < 0) return false;
        std::size_t queued = static_cast<std::size_t>(bytes) / sizeof(float);
        while (queued < target) {
            if (cursor == pcm.samples.size()) {
                if (loop) cursor = 0;
                else {
                    if (!flushed) { if (!SDL_FlushAudioStream(stream)) return false; flushed = true; }
                    return queued != 0 || SDL_GetAudioStreamAvailable(stream) > 0;
                }
            }
            auto count = std::min(target - queued, pcm.samples.size() - cursor);
            if (!SDL_PutAudioStreamData(stream, pcm.samples.data() + cursor, static_cast<int>(count * sizeof(float)))) return false;
            cursor += count; queued += count;
        }
        return true;
    }
};
SdlAudio::SdlAudio(const Project& project, std::filesystem::path directory) : project_(project), directory_(std::move(directory)) {}
SdlAudio::~SdlAudio() { stopAll(); if (initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO); }
std::string SdlAudio::create(const std::string& id, double volume, bool loop, std::unique_ptr<Voice>& out) {
    if (!std::isfinite(volume) || volume < 0 || volume > 100) return "음량은 0~100이어야 합니다.";
    const Asset* asset = nullptr;
    for (const auto& a : project_.assets) if (a.id == id && a.kind == "audio") asset = &a;
    if (!asset) return "소리 에셋이 없습니다: " + id;
    if (auto error = validateAssets({*asset}, directory_); !error.empty()) return error;
    const auto path = directory_ / std::filesystem::path(std::u8string(asset->path.begin(), asset->path.end()));
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto length = file ? static_cast<std::streamoff>(file.tellg()) : 0;
    if (length <= 0 || length > static_cast<std::streamoff>(kMaxAudioBytes)) return "소리 파일 누락/크기 오류: " + id;
    std::vector<char> bytes(static_cast<std::size_t>(length));
    file.seekg(0); if (!file.read(bytes.data(), length)) return "소리 파일 읽기 실패: " + id;
    auto voice = std::make_unique<Voice>();
    auto extension = path.extension().string();
    if (!extension.empty()) extension.erase(0, 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto error = decodeAudio(bytes.data(), bytes.size(), extension, voice->pcm);
    if (!error.empty()) return id + ": " + error;
    std::size_t total = voice->pcm.samples.size() * sizeof(float);
    if (bgm_) total += bgm_->pcm.samples.size() * sizeof(float);
    for (const auto& effect : effects_) total += effect->pcm.samples.size() * sizeof(float);
    if (total > 256 * 1024 * 1024) return "동시 소리 메모리 한도 256 MiB 초과.";
    if (!initialized_) initialized_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!initialized_) return std::string("소리 장치 시작 실패: ") + SDL_GetError();
    voice->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &voice->pcm.spec, nullptr, nullptr);
    if (!voice->stream) return std::string("소리 장치 열기 실패: ") + SDL_GetError();
    voice->loop = loop; voice->volume = volume;
    if (!SDL_SetAudioStreamGain(voice->stream, static_cast<float>(volume / 100)) || !voice->feed() ||
        !SDL_ResumeAudioStreamDevice(voice->stream)) return std::string("소리 재생 실패: ") + SDL_GetError();
    out = std::move(voice);
    return {};
}
std::string SdlAudio::playBgm(const std::string& id, double volume, bool loop) {
    std::unique_ptr<Voice> next;
    auto error = create(id, volume, loop, next);
    if (error.empty()) bgm_ = std::move(next); // Preserve previous music if load/device fails.
    return error;
}
std::string SdlAudio::stopBgm(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 60) return "페이드 시간은 0~60초여야 합니다.";
    if (bgm_) {
        if (seconds == 0) bgm_.reset();
        else { if (bgm_->fade > 0) bgm_->volume *= bgm_->remaining / bgm_->fade; bgm_->fade = bgm_->remaining = seconds; }
    }
    return {};
}
std::string SdlAudio::playSe(const std::string& id, double volume) {
    if (effects_.size() >= 16) return "효과음은 최대 16개까지 동시에 재생합니다.";
    std::unique_ptr<Voice> voice;
    auto error = create(id, volume, false, voice);
    if (error.empty()) effects_.push_back(std::move(voice));
    return error;
}
void SdlAudio::stopSe() { effects_.clear(); }
void SdlAudio::stopAll() { bgm_.reset(); stopSe(); }
bool SdlAudio::bgmPlaying() const { return !!bgm_; }
std::size_t SdlAudio::seCount() const { return effects_.size(); }
void SdlAudio::update(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) return;
    if (bgm_ && bgm_->fade > 0) {
        bgm_->remaining = std::max(0.0, bgm_->remaining - seconds);
        if (bgm_->remaining == 0) bgm_.reset();
        else SDL_SetAudioStreamGain(bgm_->stream, static_cast<float>(bgm_->volume / 100 * bgm_->remaining / bgm_->fade));
    }
    if (bgm_ && !bgm_->feed()) bgm_.reset();
    std::erase_if(effects_, [](const auto& v) { return !v->feed(); });
}
}
