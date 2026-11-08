#include "naru/network/Network.h"
#include "naru/runtime/GameWorld.h"
#include "naru/python/PythonRuntime.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <tuple>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
naru::HttpResult wait(naru::Network& network, std::uint64_t id) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < end) {
        auto result = network.result(id);
        if (result.status != "pending") return network.result(id, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("request did not finish");
}
struct StartRequest : naru::GameScripts {
    std::string url;
    std::uint64_t id = 0;
    int load(naru::CommandContext&) override { return 0; }
    naru::HookStatus call(const std::string& hook, naru::CommandContext& ctx) override {
        if (hook != naru::kHookGameStart) return naru::HookStatus::Missing;
        id = ctx.game->network().request({url});
        return naru::HookStatus::Ok;
    }
};
}
int main(int argc, char** argv) {
    try {
        check(argc == 3, "expected server URL and Python fixture");
        const std::string base = argv[1];
        naru::Network network;
        for (const auto& [path, status, code, error] : {
            std::tuple{"/unicode", "completed", 200, ""},
            std::tuple{"/empty", "completed", 204, ""},
            std::tuple{"/invalid", "failed", 200, "invalid_json"},
            std::tuple{"/unauthorized", "failed", 401, "http_error"},
            std::tuple{"/redirect", "failed", 302, "http_error"},
            std::tuple{"/large", "failed", 200, "response_too_large"}}) {
            const auto result = wait(network, network.request({base + path}));
            check(result.status == status && result.statusCode == code && result.error == error, path);
        }
        naru::HttpRequest post{base + "/echo"}; post.method = "POST"; post.body = R"({"message":"한국어"})";
        post.headers["Content-Type"] = "application/json";
        check(wait(network, network.request(post)).text == post.body, "JSON POST body changed");
        for (const std::string method : {"PUT", "PATCH", "DELETE"}) {
            post.method = method;
            check(wait(network, network.request(post)).text == post.body, "HTTP method or body changed");
        }
        naru::HttpRequest timeout{base + "/slow"}; timeout.timeoutMs = 100;
        check(wait(network, network.request(timeout)).error == "timeout", "timeout failed");
        const auto id = network.request({base + "/slow"});
        network.cancel(id);
        check(network.result(id).status == "cancelled", "cancel not immediate");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        check(network.result(id, true).status == "cancelled", "late response resurrected request");
        check(wait(network, network.request({base + "/disconnect"})).error == "transport_error", "disconnect failed");
        check(wait(network, network.request({"http://example.com/"})).error == "https_required", "remote cleartext allowed");
        post.headers["X-Test"] = "bad\r\nInjected: value";
        bool rejected = false;
        try { network.request(post); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "header injection allowed");
        post.headers.clear(); post.headers["Content-Length"] = "1";
        rejected = false;
        try { network.request(post); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "reserved transport header allowed");
        naru::Project project;
        naru::Scene scene; scene.id = "first"; project.scenes.push_back(scene); scene.id = "second"; project.scenes.push_back(scene);
        StartRequest hooks; hooks.url = base + "/slow";
        naru::GameWorld world(project, {});
        world.setGameScripts(&hooks);
        check(world.startGame("first"), "game start failed");
        check(world.network().result(hooks.id).status == "pending", "first scene cancelled game-start request");
        const auto pending = world.network().request({base + "/slow"});
        for (int tick = 0; tick < 10; ++tick) world.update(1.0 / 60.0, {});
        check(world.tickCount() >= 10, "network blocked game ticks");
        world.enterScene("second");
        check(world.network().result(hooks.id, true).status == "cancelled", "scene did not cancel game-start request");
        check(world.network().result(pending, true).status == "cancelled", "scene did not cancel request");
        rejected = false;
        try { world.network().result(pending); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "consumed cancelled request remained");
        const auto start = std::chrono::steady_clock::now();
        { naru::Network temporary; temporary.request({base + "/slow"}); }
        check(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100), "destructor blocked");
        const auto old = network.request({base + "/slow"});
        network.cancel(old);
        auto newer = wait(network, network.request({base + "/unicode"}));
        check(newer.status == "completed", "old cancellation broke newer request");
        std::this_thread::sleep_for(std::chrono::milliseconds(450));
        check(network.result(old, true).status == "cancelled", "old completion overwrote cancellation");
        naru::python::PythonRuntime python;
        naru::Variables variables; variables.set("base_url", base);
        naru::Logger logger;
        naru::CommandContext context{variables, logger};
        check(python.runFile(argv[2], context, "outside_game").ok, "F5/F6 should expose network=None");
        context.game = &world;
        auto result = python.runFile(argv[2], context);
        if (!result.ok) throw std::runtime_error(result.summary());
        std::cout << "native HTTP, cancellation, game ticks and Python integration passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
