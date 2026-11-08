#include "naru/audio/AudioDecoder.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_libs/dr_mp3.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace naru {
std::string decodeAudio(const void* bytes, std::size_t size, const std::string& extension, DecodedAudio& out) {
    out = {};
    if (!bytes || !size || size > kMaxAudioBytes) return "소리 파일은 0보다 크고 64 MiB 이하여야 합니다.";
    DecodedAudio result;
    if (extension == "mp3") {
        auto mp3 = std::make_unique<drmp3>();
        if (!drmp3_init_memory(mp3.get(), bytes, size, nullptr)) return "MP3 파일을 읽을 수 없습니다.";
        result.spec = {SDL_AUDIO_F32, static_cast<int>(mp3->channels), static_cast<int>(mp3->sampleRate)};
        std::array<float, 8192> block;
        const bool valid = mp3->channels > 0 && mp3->channels <= 2 && mp3->sampleRate > 0;
        if (!valid) { drmp3_uninit(mp3.get()); return "MP3 채널/주파수가 올바르지 않습니다."; }
        while (auto frames = drmp3_read_pcm_frames_f32(mp3.get(), block.size() / mp3->channels, block.data())) {
            const auto count = static_cast<std::size_t>(frames * mp3->channels);
            if ((result.samples.size() + count) * sizeof(float) > kMaxDecodedAudioBytes) {
                drmp3_uninit(mp3.get()); return "소리 디코딩 결과는 128 MiB 이하여야 합니다.";
            }
            result.samples.insert(result.samples.end(), block.begin(), block.begin() + count);
        }
        drmp3_uninit(mp3.get());
    } else if (extension == "wav") {
        if (size < 44 || std::memcmp(bytes, "RIFF", 4) || std::memcmp(static_cast<const char*>(bytes) + 8, "WAVE", 4))
            return "WAV 파일이 손상되었습니다.";
        SDL_AudioSpec source{};
        Uint8* data = nullptr; Uint32 length = 0;
        if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(bytes, size), true, &source, &data, &length))
            return std::string("WAV 파일을 읽을 수 없습니다: ") + SDL_GetError();
        result.spec = {SDL_AUDIO_F32, source.channels, source.freq};
        Uint8* converted = nullptr; int convertedSize = 0;
        // Conversion only changes sample representation, not rate/channel count.
        const bool bounded = length > 0 && source.channels > 0 && source.channels <= 8 && source.freq > 0 &&
            SDL_AUDIO_BYTESIZE(source.format) > 0 &&
            static_cast<std::size_t>(length) / SDL_AUDIO_BYTESIZE(source.format) <= kMaxDecodedAudioBytes / sizeof(float);
        const bool ok = bounded && SDL_ConvertAudioSamples(&source, data, static_cast<int>(length), &result.spec, &converted, &convertedSize);
        SDL_free(data);
        if (!ok || convertedSize <= 0 || static_cast<std::size_t>(convertedSize) > kMaxDecodedAudioBytes) {
            SDL_free(converted); return "WAV 디코딩 실패 또는 디코딩 결과 128 MiB 초과.";
        }
        result.samples.resize(static_cast<std::size_t>(convertedSize) / sizeof(float));
        std::memcpy(result.samples.data(), converted, convertedSize);
        SDL_free(converted);
    } else return "소리는 WAV/MP3 형식만 지원합니다.";
    if (result.samples.empty()) return "소리 샘플이 없습니다.";
    out = std::move(result);
    return {};
}
}
