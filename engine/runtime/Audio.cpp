#include "naru/runtime/Audio.h"
#include "naru/project/Project.h"
#include <functional>

namespace naru {
namespace {
template<class Commands, class Callback>
void visit(Commands& commands, const Callback& callback) {
    for (auto& c : commands) {
        callback(c);
        visit(c.thenCommands, callback); visit(c.elseCommands, callback);
        for (auto& choice : c.choices) visit(choice.commands, callback);
    }
}
}
std::vector<std::string> audioUses(const Project& project, const std::string& id) {
    std::vector<std::string> result;
    const auto scan = [&](const std::vector<EventCommand>& commands, const std::string& location) {
        visit(commands, [&](const EventCommand& c) {
            if (c.type == EventCommandType::Audio && (c.audioAction == "play_bgm" || c.audioAction == "play_se") && c.audioAsset == id)
                result.push_back(location + " / 소리 " + c.audioAction);
        });
    };
    if (!id.empty()) {
        for (const auto& scene : project.scenes) for (const auto& object : scene.objects) scan(object.event.commands, scene.name + " / " + object.name);
        for (const auto& common : project.commonEvents) scan(common.event.commands, "공통 이벤트 / " + common.name);
    }
    return result;
}
void renameAudioReferences(std::vector<EventCommand>& commands, const std::string& before, const std::string& after) {
    visit(commands, [&](EventCommand& c) {
        if (c.type == EventCommandType::Audio && c.audioAsset == before) c.audioAsset = after;
    });
}
}
