#include "TestMain.h"
#include "naru/audio/SdlAudio.h"
#include "naru/events/EventCompiler.h"
#include "naru/events/EventRunner.h"
#include "naru/project/ProjectIO.h"
#include "naru/runtime/GameWorld.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>

using namespace naru;
namespace {
std::filesystem::path directory;
Project project;
EventCommand command(const std::string& action, const std::string& asset = "test_bgm") {
    EventCommand c; c.type = EventCommandType::Audio; c.audioAction = action; c.audioAsset = asset; return c;
}
void runFor(SdlAudio& audio, int milliseconds) {
    for (int i = 0; i < milliseconds; i += 10) { SDL_Delay(10); audio.update(.01); }
}
}

TEST_CASE("WAV 재생·반복·효과음 겹침·페이드·잘못된 파일 실패 보존") {
    SdlAudio audio(project, directory);
    CHECK(audio.playBgm("test_bgm", 20, true).empty());
    CHECK(audio.bgmPlaying());
    CHECK(!audio.playBgm("missing", 100, true).empty()); CHECK(audio.bgmPlaying());
    CHECK(!audio.playBgm("test_bgm", std::numeric_limits<double>::quiet_NaN(), true).empty());
    CHECK(audio.playBgm("test_se", 20, true).empty()); // 0.35s clip must still play after 0.65s.
    CHECK(audio.playSe("test_se", 50).empty()); CHECK(audio.playSe("test_se", 80).empty());
    CHECK(audio.seCount() == 2);
    runFor(audio, 650);
    CHECK(audio.bgmPlaying()); CHECK(audio.seCount() == 0);
    CHECK(audio.stopBgm(.15).empty()); audio.update(.10); CHECK(audio.bgmPlaying());
    audio.update(.10); CHECK(!audio.bgmPlaying());
    CHECK(audio.playBgm("test_se", 100, false).empty()); runFor(audio,650); CHECK(!audio.bgmPlaying());
    for (int i=0; i<16; ++i) CHECK(audio.playSe("test_se",10).empty());
    CHECK(!audio.playSe("test_se",10).empty()); CHECK(audio.seCount() == 16);
    audio.stopSe(); CHECK(audio.seCount() == 0);
    auto missing = directory / "assets/missing.wav";
    auto broken = project; auto a = project.assets.back(); a.id = "broken"; a.path = "assets/missing.wav"; broken.assets.push_back(a);
    SdlAudio brokenAudio(broken, directory);
    CHECK(!brokenAudio.playBgm("broken",100,true).empty());
    DecodedAudio decoded; const char invalid[] = "not audio";
    CHECK(!decodeAudio(invalid,sizeof(invalid),"mp3",decoded).empty());
    CHECK(!decodeAudio(invalid,sizeof(invalid),"wav",decoded).empty());
    CHECK(!decodeAudio(nullptr,kMaxAudioBytes+1,"wav",decoded).empty());
}

TEST_CASE("실제 이벤트 소리·F6 로그·오류 위치·장면 전환 유지와 JSON 왕복") {
    auto p = project;
    auto list = std::vector<EventCommand>{command("play_bgm"), command("play_se","test_se"), command("stop_bgm"), command("stop_se")};
    p.scenes[0].objects[1].event.commands = list;
    auto target = directory / "roundtrip";
    std::filesystem::create_directories(target);
    CHECK(ProjectIO::save(p,target).empty());
    auto loaded = ProjectIO::load(target); CHECK(loaded.errors.empty());
    CHECK(loaded.project.has_value());
    if (!loaded.project) return;
    CHECK(loaded.project->scenes[0].objects[1].event.commands == list);
    GameWorld world(p,{}); SdlAudio audio(world.project(),directory); world.setAudio(&audio);
    CHECK(world.startGame(p.startSceneId));
    world.update(1.0/60, {});
    CHECK(!audio.bgmPlaying() && audio.seCount() == 0);
    Variables vars; Logger logger; CommandContext preview{vars,logger};
    EventRunner runner(compileEvent(list,{})); runner.start(preview); CHECK(runner.status() == CommandStatus::Completed);
    CHECK(!logger.entries().empty()); CHECK(!audio.bgmPlaying());
    CommandContext ctx{vars,logger,nullptr,std::nullopt,nullptr,&world};
    EventRunner real(compileEvent({command("play_bgm")},{})); real.start(ctx); CHECK(audio.bgmPlaying());
    CHECK(world.enterScene(p.startSceneId)); // startup list explicitly stops music
    world.update(1.0/60, {});
    CHECK(!audio.bgmPlaying());
    CHECK(audio.playBgm("test_bgm",30,true).empty());
    // No implicit scene stop: explicitly re-enter a scene whose events do not stop BGM.
    auto quiet = project; for (auto& o : quiet.scenes[0].objects) o.event.commands.clear();
    GameWorld silent(quiet,{}); silent.setAudio(&audio); CHECK(silent.startGame(quiet.startSceneId));
    silent.update(1.0/60,{}); CHECK(silent.enterScene(quiet.startSceneId)); silent.update(1.0/60,{});
    CHECK(audio.bgmPlaying());
    EventRunner bad(compileEvent({command("play_se","absent")},{})); bad.start(ctx);
    CHECK(bad.status()==CommandStatus::Failed); CHECK(bad.failedCommandId()=="0"); CHECK(audio.bgmPlaying());
    audio.stopAll();
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc < 2) return 2;
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER,"dummy");
    directory = std::filesystem::path(argv[1]);
    auto loaded = ProjectIO::load(directory); if (!loaded.errors.empty()) return 2;
    if (!loaded.project) return 2;
    project = *loaded.project;
    const int result = test::runAll();
    if (argc > 2) {
        std::ifstream input(std::filesystem::path(argv[2]),std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{}); DecodedAudio pcm;
        auto error=decodeAudio(bytes.data(),bytes.size(),"mp3",pcm);
        if (!error.empty()) { std::printf("MP3 failed: %s\n",error.c_str()); return 1; }
        std::printf("[PASS] User MP3 decoded: %zu samples, %d Hz, %d channels\n",pcm.samples.size(),pcm.spec.freq,pcm.spec.channels);
        SdlAudio audio(project,directory); if (!audio.playBgm("test_bgm",35,true).empty()) return 1;
        runFor(audio,200); if (!audio.bgmPlaying()) return 1;
        std::printf("[PASS] User MP3 playback stream\n");
    }
    return result;
}
