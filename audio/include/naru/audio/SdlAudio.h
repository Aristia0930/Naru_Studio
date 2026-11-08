#pragma once
#include "naru/audio/AudioDecoder.h"
#include "naru/runtime/Audio.h"
#include "naru/project/Project.h"
#include <filesystem>
#include <memory>

namespace naru {
class SdlAudio final : public AudioHost {
public:
    SdlAudio(const Project& project, std::filesystem::path directory);
    ~SdlAudio() override;
    std::string playBgm(const std::string&, double volume, bool loop) override;
    std::string stopBgm(double fadeSeconds) override;
    std::string playSe(const std::string&, double volume) override;
    void stopSe() override;
    void stopAll();
    void update(double seconds);
    bool bgmPlaying() const;
    std::size_t seCount() const;
private:
    struct Voice;
    std::string create(const std::string&, double, bool, std::unique_ptr<Voice>&);
    const Project& project_;
    std::filesystem::path directory_;
    bool initialized_ = false;
    std::unique_ptr<Voice> bgm_;
    std::vector<std::unique_ptr<Voice>> effects_;
};
}
