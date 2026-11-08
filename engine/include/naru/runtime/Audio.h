#pragma once
#include <string>
#include <vector>
#include "naru/events/EventData.h"

namespace naru {
struct Project;
std::vector<std::string> audioUses(const Project&, const std::string& id);
void renameAudioReferences(std::vector<EventCommand>&, const std::string& before, const std::string& after);
// Device-independent contract. Calls return an error string, empty on success.
class AudioHost {
public:
    virtual ~AudioHost() = default;
    virtual std::string playBgm(const std::string& asset, double volume, bool loop) = 0;
    virtual std::string stopBgm(double fadeSeconds) = 0;
    virtual std::string playSe(const std::string& asset, double volume) = 0;
    virtual void stopSe() = 0;
};
}
