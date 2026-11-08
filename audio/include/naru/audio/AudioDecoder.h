#pragma once
#include <SDL3/SDL_audio.h>
#include <cstddef>
#include <string>
#include <vector>

namespace naru {
inline constexpr std::size_t kMaxAudioBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kMaxDecodedAudioBytes = 128 * 1024 * 1024;
struct DecodedAudio {
    SDL_AudioSpec spec{};
    std::vector<float> samples;
};
// Reads the supplied bytes, so import validation and playback use the same decoder.
std::string decodeAudio(const void* bytes, std::size_t size, const std::string& extension, DecodedAudio& out);
}
