// Naru Player: 프로젝트를 읽어 게임 실행 화면을 띄운다.
// 그리기·입력은 SDL3, 대사 글자는 SDL3_ttf. 게임 진행은 엔진의 GameWorld가 맡는다.
//
//   NaruPlayer <프로젝트 폴더> [--scene <id>] [--font <ttf>]
//   자동 검사·설명서 촬영: [--frames <n>] [--auto-confirm] [--screenshot <png>]
//                          [--hold <동작>:<시작 프레임>-<끝 프레임>]... (동작 이름: up down left right confirm cancel,
//                          또는 전역 스크립트가 만든 동작)
//
// 로그는 표준 출력에 한 줄씩 "NARU\t수준\t명령ID\t파일\t행\t메시지\t부가정보" 형식으로 쓴다 (제작툴이 읽는다).
// 종료 코드: 0 정상, 1 시작 실패, 3 이벤트 오류가 있었음.

#include "naru/project/ProjectIO.h"
#include "naru/runtime/GameWorld.h"

#if NARU_PLAYER_HAS_PYTHON
#include "naru/python/PythonGameScripts.h"
#include "naru/python/PythonRuntime.h"
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace naru;

namespace {

// 자동 검사용 입력: from~to 프레임 동안 동작을 누르고 있는다.
struct ScriptedInput {
    std::string action;
    int from;
    int to;
};

struct Options {
    fs::path projectDir;
    std::string sceneId;
    std::string fontPath;
    int frames = 0; // 0이면 창을 닫을 때까지
    bool autoConfirm = false;
    std::vector<ScriptedInput> scripted;
    std::string screenshot;
};

fs::path pathFromUtf8(const std::string& text) {
    return fs::path(std::u8string(text.begin(), text.end()));
}

std::string utf8(const fs::path& path) {
    std::u8string s = path.u8string();
    return std::string(s.begin(), s.end());
}

std::string escapeField(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\t') {
            out += "\\t";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c != '\r') {
            out += c;
        }
    }
    return out;
}

void writeLog(const LogEntry& e) {
    std::string line = "NARU\t" + std::string(toString(e.level)) + "\t" + escapeField(e.source.commandId) + "\t" +
                       escapeField(e.source.file) + "\t" + std::to_string(e.source.line) + "\t" +
                       escapeField(e.message) + "\t" + escapeField(e.detail) + "\n";
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fflush(stdout);
}

void writeStartupError(const std::string& message) {
    LogEntry e;
    e.level = LogLevel::Error;
    e.message = message;
    writeLog(e);
}

std::optional<ScriptedInput> parseHold(const std::string& text) {
    std::size_t colon = text.find(':');
    std::size_t dash = text.find('-', colon == std::string::npos ? 0 : colon);
    if (colon == std::string::npos || dash == std::string::npos) {
        return std::nullopt;
    }
    std::string name = text.substr(0, colon);
    int from = std::atoi(text.substr(colon + 1, dash - colon - 1).c_str());
    int to = std::atoi(text.substr(dash + 1).c_str());
    if (!InputMap::isValidActionName(name) || from < 0 || to < from) {
        return std::nullopt;
    }
    return ScriptedInput{name, from, to};
}
std::optional<Options> parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::optional<std::string> {
            if (i + 1 >= argc) {
                writeStartupError(arg + " 다음에 값이 필요합니다");
                return std::nullopt;
            }
            return std::string(argv[++i]);
        };
        if (arg == "--hold") {
            auto value = next();
            if (!value) {
                return std::nullopt;
            }
            std::optional<ScriptedInput> held = parseHold(*value);
            if (!held) {
                writeStartupError("--hold 형식: <동작>:<시작>-<끝> (예: right:0-60): " + *value);
                return std::nullopt;
            }
            o.scripted.push_back(*held);
        } else if (arg == "--scene" || arg == "--font" || arg == "--frames" || arg == "--screenshot") {
            auto value = next();
            if (!value) {
                return std::nullopt;
            }
            if (arg == "--scene") {
                o.sceneId = *value;
            } else if (arg == "--font") {
                o.fontPath = *value;
            } else if (arg == "--frames") {
                o.frames = std::atoi(value->c_str());
            } else {
                o.screenshot = *value;
            }
        } else if (arg == "--auto-confirm") {
            o.autoConfirm = true;
        } else if (!arg.starts_with("--") && o.projectDir.empty()) {
            o.projectDir = pathFromUtf8(arg);
        } else {
            writeStartupError("알 수 없는 옵션: " + arg);
            return std::nullopt;
        }
    }
    if (o.projectDir.empty()) {
        writeStartupError("사용법: NaruPlayer <프로젝트 폴더> [--scene <id>]");
        return std::nullopt;
    }
    return o;
}

SDL_Color parseColor(const std::string& text, SDL_Color fallback) {
    if (text.size() != 7 || text[0] != '#') {
        return fallback;
    }
    char* end = nullptr;
    unsigned long v = std::strtoul(text.c_str() + 1, &end, 16);
    if (end != text.c_str() + 7) {
        return fallback;
    }
    return SDL_Color{static_cast<Uint8>((v >> 16) & 0xff), static_cast<Uint8>((v >> 8) & 0xff),
                     static_cast<Uint8>(v & 0xff), 255};
}

void setColor(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

// 글자를 텍스처로 만들어 두고, 내용이 바뀔 때만 다시 만든다.
class TextCache {
public:
    ~TextCache() { reset(); }
    void reset() {
        if (texture_) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        text_.clear();
    }
    // 그린 높이를 반환한다.
    float draw(SDL_Renderer* r, TTF_Font* font, const std::string& text, SDL_Color color, float x, float y,
               int wrapWidth) {
        if (text.empty()) {
            return 0.0f;
        }
        if (!font) {
            // 글꼴이 없으면 SDL 내장 디버그 글꼴(영문·숫자만)로 대신한다.
            setColor(r, color);
            SDL_RenderDebugText(r, x, y, text.c_str());
            return static_cast<float>(SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE) + 4.0f;
        }
        if (!texture_ || text != text_ || wrapWidth != wrap_) {
            reset();
            SDL_Surface* surface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), 0, color, wrapWidth);
            if (!surface) {
                return 0.0f;
            }
            texture_ = SDL_CreateTextureFromSurface(r, surface);
            w_ = static_cast<float>(surface->w);
            h_ = static_cast<float>(surface->h);
            SDL_DestroySurface(surface);
            text_ = text;
            wrap_ = wrapWidth;
        }
        SDL_FRect dst{x, y, w_, h_};
        SDL_RenderTexture(r, texture_, nullptr, &dst);
        return h_;
    }

private:
    SDL_Texture* texture_ = nullptr;
    std::string text_;
    int wrap_ = 0;
    float w_ = 0.0f;
    float h_ = 0.0f;
};

void drawScene(SDL_Renderer* r, const Scene& scene, const SceneObject* player, Facing facing) {
    setColor(r, parseColor(scene.background, SDL_Color{32, 40, 48, 255}));
    SDL_RenderClear(r);
    for (const SceneObject& o : scene.objects) {
        if (!o.visible) {
            continue;
        }
        SDL_FRect rect{static_cast<float>(o.x), static_cast<float>(o.y), static_cast<float>(o.width),
                       static_cast<float>(o.height)};
        setColor(r, parseColor(o.color, SDL_Color{74, 144, 217, 255}));
        SDL_RenderFillRect(r, &rect);
    }
    // 플레이어가 바라보는 쪽 가장자리에 작은 표시 (이미지가 생기기 전 임시 표현)
    if (player && player->visible) {
        const float s = 8.0f;
        float x = static_cast<float>(player->x);
        float y = static_cast<float>(player->y);
        float w = static_cast<float>(player->width);
        float h = static_cast<float>(player->height);
        SDL_FRect mark{x + (w - s) / 2, y + (h - s) / 2, s, s};
        switch (facing) {
        case Facing::Down: mark.y = y + h - s - 2; break;
        case Facing::Up: mark.y = y + 2; break;
        case Facing::Left: mark.x = x + 2; break;
        case Facing::Right: mark.x = x + w - s - 2; break;
        }
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderFillRect(r, &mark);
    }
}

void drawMessage(SDL_Renderer* r, TTF_Font* font, const GameWorld::Message& message, const Scene& scene,
                 TextCache& speakerText, TextCache& bodyText, Uint64 frame) {
    const float margin = 24.0f;
    const float boxH = 170.0f;
    const float w = static_cast<float>(scene.width);
    const float h = static_cast<float>(scene.height);
    SDL_FRect box{margin, h - margin - boxH, w - margin * 2, boxH};

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 12, 16, 28, 215);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 220, 228, 240, 255);
    SDL_RenderRect(r, &box);

    float x = box.x + 24.0f;
    float y = box.y + 16.0f;
    int wrap = static_cast<int>(box.w - 48.0f);
    if (!message.speaker.empty()) {
        y += speakerText.draw(r, font, message.speaker, SDL_Color{255, 214, 102, 255}, x, y, wrap) + 6.0f;
    }
    bodyText.draw(r, font, message.text, SDL_Color{240, 244, 250, 255}, x, y, wrap);

    // 넘길 수 있음을 알리는 깜빡이는 표시.
    if ((frame / 30) % 2 == 0) {
        SDL_FRect mark{box.x + box.w - 34.0f, box.y + box.h - 30.0f, 12.0f, 12.0f};
        SDL_SetRenderDrawColor(r, 255, 214, 102, 255);
        SDL_RenderFillRect(r, &mark);
    }
}

#if NARU_PLAYER_HAS_PYTHON
// 이벤트의 "스크립트 호출". 전역 스크립트로 읽어 둔 파일이면 실행 중인 모듈의 함수를 부르고(모듈 전역 변수 공유),
// 아니면 파일을 새로 읽어 실행한다.
ScriptRunner makeScriptRunner(python::PythonRuntime* python, std::string initError, fs::path scriptsDir,
                              python::PythonGameScripts* globals) {
    return [python, initError, scriptsDir, globals](const std::string& script, const std::string& function,
                                                   CommandContext& ctx) -> ScriptCallResult {
        if (!python) {
            return {"Python을 시작하지 못했습니다: " + initError, {}};
        }
        if (script.find_first_of("/\\") != std::string::npos || script.find("..") != std::string::npos) {
            return {"scripts 폴더 밖의 파일은 호출할 수 없습니다: " + script, {}};
        }
        ScriptCallResult call;
        if (globals) {
            if (std::optional<std::string> loaded = globals->loadedPath(script)) {
                python::ScriptResult result;
                switch (python->callModuleFunction(*loaded, function, ctx, &result, &call.returned)) {
                case python::CallStatus::Ok: return call;
                case python::CallStatus::Missing:
                    return {"전역 스크립트 " + script + "에 호출할 함수 '" + function + "'가 없습니다", {}};
                case python::CallStatus::Failed: return {"Python 스크립트 오류 (" + result.error.type + ")", {}};
                }
            }
        }
        fs::path path = scriptsDir / pathFromUtf8(script);
        if (!fs::exists(path)) {
            return {"스크립트 파일이 없습니다: scripts/" + script, {}};
        }
        auto result = python->runFile(utf8(fs::absolute(path)), ctx, function, &call.returned);
        if (!result.ok) {
            call.error = "Python 스크립트 오류 (" + result.error.type + ")";
        }
        return call;
    };
}
#endif
TTF_Font* openFont(const std::string& requested) {
    std::vector<std::string> candidates;
    if (!requested.empty()) {
        candidates.push_back(requested);
    }
    if (const char* env = SDL_getenv("NARU_PLAYER_FONT")) {
        candidates.emplace_back(env);
    }
#ifdef _WIN32
    // 개발 중에는 Windows 기본 한글 글꼴을 쓴다. 게임 내보내기에서는 글꼴을 함께 묶는다.
    candidates.emplace_back("C:/Windows/Fonts/malgun.ttf");
#endif
    for (const std::string& path : candidates) {
        if (TTF_Font* font = TTF_OpenFont(path.c_str(), 26.0f)) {
            return font;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char** argv) {
    std::optional<Options> options = parseArgs(argc, argv);
    if (!options) {
        return 1;
    }

    ProjectLoadResult loaded = ProjectIO::load(options->projectDir);
    if (!loaded.ok()) {
        writeStartupError("프로젝트를 열 수 없습니다: " + loaded.errorText());
        return 1;
    }
    Project project = std::move(*loaded.project);
    std::string sceneId = options->sceneId;
    if (sceneId.empty()) {
        sceneId = !project.startSceneId.empty() ? project.startSceneId
                                                : (project.scenes.empty() ? std::string() : project.scenes.front().id);
    }
    std::string title = project.name + " — Naru Player";
    fs::path scriptsDir = ProjectIO::scriptsDirectory(options->projectDir);

    // 소멸 순서: world → 전역 스크립트(모듈 정리) → Python
#if NARU_PLAYER_HAS_PYTHON
    std::unique_ptr<python::PythonRuntime> python;
    std::string pythonError;
    try {
        python = std::make_unique<python::PythonRuntime>();
    } catch (const std::exception& ex) {
        pythonError = ex.what();
    }
    std::unique_ptr<python::PythonGameScripts> globals;
    if (python && !project.globalScripts.empty()) {
        std::vector<std::string> paths;
        for (const std::string& name : project.globalScripts) {
            paths.push_back(utf8(fs::absolute(scriptsDir / pathFromUtf8(name))));
        }
        globals = std::make_unique<python::PythonGameScripts>(*python, std::move(paths));
    }
    bool wantsGlobals = !project.globalScripts.empty();
    GameWorld world(std::move(project), makeScriptRunner(python.get(), pythonError, scriptsDir, globals.get()));
    world.logger().addSink(writeLog);
    world.setGameScripts(globals.get());
    if (!python && wantsGlobals) {
        world.logger().error("Python을 시작하지 못해 전역 스크립트를 실행하지 않습니다: " + pythonError);
    }
#else
    GameWorld world(std::move(project), [](const std::string&, const std::string&, CommandContext&) {
        return ScriptCallResult{"이 player는 Python 없이 빌드되었습니다", {}};
    });
    world.logger().addSink(writeLog);
#endif
    if (!world.startGame(sceneId)) {
        return 1;
    }
    const Scene& scene = *world.scene();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        writeStartupError(std::string("SDL을 시작하지 못했습니다: ") + SDL_GetError());
        return 1;
    }
    // 창은 장면 크기로 열되, 큰 장면은 화면에 들어가도록 줄인다. 그리기 좌표는 항상 장면 좌표다.
    int winW = scene.width;
    int winH = scene.height;
    while (winW > 1600 || winH > 900) {
        winW = winW * 3 / 4;
        winH = winH * 3 / 4;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(title.c_str(), winW, winH, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        writeStartupError(std::string("창을 만들지 못했습니다: ") + SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, scene.width, scene.height, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(renderer, 1);

    TTF_Font* font = nullptr;
    if (TTF_Init()) {
        font = openFont(options->fontPath);
    }
    if (!font) {
        world.logger().warning("한글 글꼴을 열지 못해 대사를 영문 디버그 글꼴로 표시합니다 (--font로 지정)");
    }

    InputState input;
    // 누르고 있는 실제 키 이름 (SDL 키 이름). 매 프레임 키 배치(전역 스크립트가 바꿀 수 있음)로 동작을 계산한다.
    std::set<std::string> heldKeys;
    TextCache speakerText;
    TextCache bodyText;
    bool running = true;
    Uint64 frame = 0;
    Uint64 messageSince = 0;
    std::string shownMessage;
    Uint64 last = SDL_GetTicksNS();
    const bool fixedFrames = options->frames > 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                heldKeys.insert(SDL_GetScancodeName(event.key.scancode));
            } else if (event.type == SDL_EVENT_KEY_UP) {
                heldKeys.erase(SDL_GetScancodeName(event.key.scancode));
            }
        }
        std::set<std::string> scriptedHeld;
        for (const ScriptedInput& s : options->scripted) {
            if (static_cast<int>(frame) >= s.from && static_cast<int>(frame) <= s.to) {
                scriptedHeld.insert(s.action);
            }
        }
        // 자동 검사: 대사가 뜬 지 0.5초가 지나면 결정 키를 한 프레임 누른 것으로 친다.
        if (options->autoConfirm && world.message() && frame - messageSince >= 30 && frame % 2 == 0) {
            scriptedHeld.insert(toKey(InputAction::Confirm));
        }
        std::set<std::string> actions(scriptedHeld.begin(), scriptedHeld.end());
        for (const std::string& a : world.inputMap().actions()) {
            actions.insert(a);
        }
        for (const std::string& action : actions) {
            bool held = scriptedHeld.count(action) > 0;
            for (const std::string& key : world.inputMap().keys(action)) {
                for (const std::string& pressed : heldKeys) {
                    held = held || InputMap::keyMatches(key, pressed);
                }
            }
            input.set(action, held);
        }
        Uint64 now = SDL_GetTicksNS();
        double dt = fixedFrames ? 1.0 / 60.0 : static_cast<double>(now - last) / 1e9;
        last = now;
        world.update(dt, input);
        input.endFrame();
        std::string messageKey = world.message() ? world.message()->speaker + "\n" + world.message()->text : "";
        if (messageKey != shownMessage) {
            shownMessage = messageKey;
            messageSince = frame;
        }

        drawScene(renderer, *world.scene(), world.player(), world.facing());
        if (world.message()) {
            drawMessage(renderer, font, *world.message(), *world.scene(), speakerText, bodyText, frame);
        }

        ++frame;
        bool lastFrame = fixedFrames && frame >= static_cast<Uint64>(options->frames);
        if (lastFrame && !options->screenshot.empty()) {
            if (SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr)) {
                if (!SDL_SavePNG(shot, options->screenshot.c_str())) {
                    world.logger().error(std::string("화면을 저장하지 못했습니다: ") + SDL_GetError());
                }
                SDL_DestroySurface(shot);
            }
        }
        SDL_RenderPresent(renderer);
        if (lastFrame) {
            running = false;
        }
    }

    speakerText.reset();
    bodyText.reset();
    if (font) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    world.logger().info("게임 종료");
    return world.failedEventCount() > 0 ? 3 : 0;
}
