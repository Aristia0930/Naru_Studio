#include "naru/runtime/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace naru {

namespace {
// Session과 같은 이유로, 긴 정지 뒤에 한 번에 처리할 최대 틱 수.
constexpr int kMaxTicksPerUpdate = 8;

struct Rect {
    double x, y, w, h;
};

Rect rectOf(const SceneObject& o) {
    return {o.x, o.y, o.width, o.height};
}

// 가장자리가 맞닿은 것은 겹친 것으로 보지 않는다.
bool overlaps(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

double overlapArea(const Rect& a, const Rect& b) {
    double w = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
    double h = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
    return w > 0 && h > 0 ? w * h : 0.0;
}

bool runsOnContact(const SceneObject& o) {
    return !o.event.commands.empty() &&
           (o.event.trigger == EventTrigger::Touch || o.event.trigger == EventTrigger::EventTouch);
}
} // namespace

const char* toKey(Facing facing) {
    switch (facing) {
    case Facing::Down: return "down";
    case Facing::Up: return "up";
    case Facing::Left: return "left";
    case Facing::Right: return "right";
    }
    return "down";
}

std::optional<Facing> facingFromKey(const std::string& key) {
    for (Facing f : {Facing::Down, Facing::Up, Facing::Left, Facing::Right}) {
        if (key == toKey(f)) {
            return f;
        }
    }
    return std::nullopt;
}

// 실행 중인 객체 이벤트 하나. 로그는 객체 이름을 붙여 게임 로그로 전달한다.
struct GameWorld::RunningEvent {
    std::string objectId;
    std::string objectName;
    Logger logger;
    CommandContext ctx;
    EventRunner runner;

    RunningEvent(GameWorld& world, const SceneObject& object)
        : objectId(object.id), objectName(object.name),
          ctx{world.variables_, logger, &world, std::nullopt, &world.project_.database, &world},
          runner(compileEvent(object.event.commands, world.scripts_)) {
        logger.addSink([&world, name = object.name](const LogEntry& entry) {
            LogEntry copy = entry;
            copy.message = name + " ▸ " + copy.message;
            world.logger_.log(std::move(copy));
        });
    }
};

GameWorld::GameWorld(Project project, ScriptRunner scripts, double fixedStep)
    : project_(std::move(project)), scripts_(std::move(scripts)), fixedStep_(fixedStep),
      hookCtx_{variables_, logger_, this, std::nullopt, &project_.database, this} {}

GameWorld::~GameWorld() {
    stopAll();
}

bool GameWorld::startGame(const std::string& sceneId) {
    if (!project_.findScene(sceneId)) {
        logger_.error("장면이 없습니다: " + sceneId);
        return false;
    }
    if (gameScripts_) {
        failedEvents_ += gameScripts_->load(hookCtx_);
    }
    callHook(kHookGameStart);
    return enterScene(sceneId);
}

bool GameWorld::enterScene(const std::string& sceneId) {
    const Scene* source = project_.findScene(sceneId);
    if (!source) {
        logger_.error("장면이 없습니다: " + sceneId);
        return false;
    }
    if (scene_) {
        callHook(kHookSceneExit);
    }
    stopAll();
    message_.reset();
    contacts_.clear();
    tickContacts_.clear();
    playerMovedThisTick_ = false;
    facing_ = Facing::Down;
    scene_ = *source;
    logger_.info("장면 시작: " + scene_->name);
    if (!scene_->playerObjectId.empty() && !playerObject()) {
        logger_.warning("플레이어 객체가 장면에 없습니다: " + scene_->playerObjectId);
    }

    for (const SceneObject& object : scene_->objects) {
        if (object.event.commands.empty()) {
            continue;
        }
        if (object.event.trigger == EventTrigger::Autorun) {
            blockingQueue_.push_back(object.id);
        } else if (object.event.trigger == EventTrigger::Parallel) {
            parallel_.push_back(makeEvent(object));
        }
    }
    callHook(kHookSceneEnter);
    return true;
}

void GameWorld::update(double realSeconds, const InputState& input) {
    if (!scene_) {
        return;
    }
    tickInput_ = input;
    // 대사는 틱보다 먼저 닫는다. 이번 프레임에 새로 뜬 대사가 같은 키 입력으로 바로 넘어가지 않게 하고,
    // 대사를 넘긴 결정 키가 곧바로 앞의 객체를 다시 조사하지 않게 한다 (스크립트에도 "새로 누름"으로 보이지 않는다).
    bool actionPressed = input.wasPressed(InputAction::Confirm);
    if (message_ && actionPressed) {
        message_.reset();
        actionPressed = false;
        tickInput_.consume(toKey(InputAction::Confirm));
    }

    accumulator_ += realSeconds;
    int ran = 0;
    while (accumulator_ >= fixedStep_ && ran < kMaxTicksPerUpdate) {
        accumulator_ -= fixedStep_;
        ++ticks_;
        ++ran;

        callHook(kHookUpdate);
        runBlocking(fixedStep_);
        if (!busy() && defaultControl_) {
            controlPlayer(actionPressed, fixedStep_);
        }
        finishContacts();
        // 조사·접촉으로 시작된 이벤트는 같은 틱에 첫 명령까지 실행한다 (대사가 바로 뜨도록).
        runBlocking(fixedStep_);

        for (std::size_t i = 0; i < parallel_.size();) {
            if (tickEvent(*parallel_[i], fixedStep_)) {
                parallel_.erase(parallel_.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        // "새로 누름"은 프레임당 한 번이므로 첫 틱에서만 쓴다 (이벤트 중에 누른 키로 조사하지 않는다).
        actionPressed = false;
        tickInput_.endFrame();

        if (pendingScene_) {
            std::string next = std::move(*pendingScene_);
            pendingScene_.reset();
            enterScene(next);
        }
    }
    if (ran == kMaxTicksPerUpdate) {
        accumulator_ = 0.0;
    }
}

const SceneObject* GameWorld::player() const {
    if (!scene_ || scene_->playerObjectId.empty()) {
        return nullptr;
    }
    return scene_->findObject(scene_->playerObjectId);
}

SceneObject* GameWorld::playerObject() {
    if (!scene_ || scene_->playerObjectId.empty()) {
        return nullptr;
    }
    return scene_->findObject(scene_->playerObjectId);
}

bool GameWorld::blockingEventActive() const {
    return blocking_ != nullptr || !blockingQueue_.empty();
}

std::size_t GameWorld::runningEventCount() const {
    return (blocking_ ? 1u : 0u) + parallel_.size();
}

void GameWorld::showMessage(const std::string& speaker, const std::string& text) {
    // 대사 창은 하나다. 병렬 이벤트가 동시에 대사를 띄우면 나중 것이 앞의 대사를 덮는다.
    message_ = Message{speaker, text};
}

void GameWorld::callHook(const char* hook) {
    if (!gameScripts_ || disabledHooks_.count(hook)) {
        return;
    }
    switch (gameScripts_->call(hook, hookCtx_)) {
    case HookStatus::Ok: break;
    case HookStatus::Missing: disabledHooks_.insert(hook); break;
    case HookStatus::Failed:
        // 오류가 난 스크립트의 그 훅은 GameScripts가 끈다 (on_update 오류가 매 틱 반복되지 않게). 다른 스크립트는 계속 불린다.
        ++failedEvents_;
        logger_.error(std::string("전역 스크립트 ") + hook + " 오류 — 이번 실행에서는 그 스크립트의 " + hook +
                      "을(를) 다시 부르지 않습니다");
        break;
    }
}

void GameWorld::controlPlayer(bool actionPressed, double dt) {
    if (!playerObject()) {
        return;
    }
    if (actionPressed && interact()) {
        return;
    }
    const InputState& in = tickInput_;
    int dx = (in.isHeld(InputAction::Right) ? 1 : 0) - (in.isHeld(InputAction::Left) ? 1 : 0);
    int dy = (in.isHeld(InputAction::Down) ? 1 : 0) - (in.isHeld(InputAction::Up) ? 1 : 0);
    if (dx == 0 && dy == 0) {
        return;
    }
    double step = playerSpeed_ * dt;
    if (dx != 0 && dy != 0) {
        step /= std::sqrt(2.0);
    }
    movePlayer(dx * step, dy * step);
}

bool GameWorld::movePlayer(double dx, double dy) {
    SceneObject* player = playerObject();
    if (!player) {
        return false;
    }
    // 바라보는 방향: 한 방향만 움직이면 그쪽, 대각선이면 지금 방향이 그중 하나일 때 유지하고 아니면 가로.
    if (dx != 0 || dy != 0) {
        Facing horizontal = dx > 0 ? Facing::Right : Facing::Left;
        Facing vertical = dy > 0 ? Facing::Down : Facing::Up;
        if (dy == 0) {
            facing_ = horizontal;
        } else if (dx == 0) {
            facing_ = vertical;
        } else if (facing_ != horizontal && facing_ != vertical) {
            facing_ = horizontal;
        }
    }
    if (dx != 0) {
        moveAxis(*player, dx, 0.0);
    }
    if (dy != 0) {
        moveAxis(*player, 0.0, dy);
    }
    playerMovedThisTick_ = true;
    return true;
}

bool GameWorld::setPlayerPosition(double x, double y) {
    SceneObject* player = playerObject();
    if (!player) {
        return false;
    }
    player->x = x;
    player->y = y;
    playerMovedThisTick_ = true;
    return true;
}

void GameWorld::moveAxis(SceneObject& player, double dx, double dy) {
    Rect before = rectOf(player);
    Rect after{before.x + dx, before.y + dy, before.w, before.h};
    // 장면 가장자리
    after.x = std::clamp(after.x, 0.0, std::max(0.0, scene_->width - after.w));
    after.y = std::clamp(after.y, 0.0, std::max(0.0, scene_->height - after.h));

    for (const SceneObject& o : scene_->objects) {
        if (o.id == player.id || o.passable) {
            continue;
        }
        Rect wall = rectOf(o);
        // 처음부터 겹쳐 있던 객체에는 막히지 않는다 (배치 실수로 갇히지 않게).
        if (!overlaps(after, wall) || overlaps(before, wall)) {
            continue;
        }
        if (dx > 0) {
            after.x = std::min(after.x, wall.x - after.w);
        } else if (dx < 0) {
            after.x = std::max(after.x, wall.x + wall.w);
        } else if (dy > 0) {
            after.y = std::min(after.y, wall.y - after.h);
        } else if (dy < 0) {
            after.y = std::max(after.y, wall.y + wall.h);
        }
        tickContacts_.insert(o.id);
    }
    // 이미 벽에 붙어 있는 상태에서 그쪽으로 밀면 움직이지 않지만 "부딪힘"으로 기록해야 한다.
    if (after.x == before.x && after.y == before.y) {
        Rect probe{before.x + (dx > 0 ? 1 : dx < 0 ? -1 : 0), before.y + (dy > 0 ? 1 : dy < 0 ? -1 : 0), before.w,
                   before.h};
        for (const SceneObject& o : scene_->objects) {
            if (o.id != player.id && !o.passable && overlaps(probe, rectOf(o)) && !overlaps(before, rectOf(o))) {
                tickContacts_.insert(o.id);
            }
        }
    }
    player.x = after.x;
    player.y = after.y;
}

void GameWorld::finishContacts() {
    const SceneObject* player = playerObject();
    // 이벤트·대사 중에 플레이어가 움직이지 않았다면 접촉 상태를 그대로 둔다 (끝난 뒤 다시 실행되지 않게).
    if (!player || (busy() && !playerMovedThisTick_)) {
        tickContacts_.clear();
        playerMovedThisTick_ = false;
        return;
    }
    // 통과 가능한 객체는 위에 서 있는 동안 닿아 있는 것으로 본다.
    Rect self = rectOf(*player);
    for (const SceneObject& o : scene_->objects) {
        if (o.id != player->id && o.passable && overlaps(self, rectOf(o))) {
            tickContacts_.insert(o.id);
        }
    }
    if (!busy()) {
        for (const SceneObject& o : scene_->objects) {
            if (tickContacts_.count(o.id) && !contacts_.count(o.id) && runsOnContact(o)) {
                queueBlocking(o);
            }
        }
    }
    contacts_ = std::move(tickContacts_);
    tickContacts_.clear();
    playerMovedThisTick_ = false;
}

bool GameWorld::interact() {
    const SceneObject* player = playerObject();
    if (!player || busy()) {
        return false;
    }
    Rect self = rectOf(*player);
    Rect probe = self;
    switch (facing_) {
    case Facing::Down: probe = {self.x, self.y + self.h, self.w, kActionReach}; break;
    case Facing::Up: probe = {self.x, self.y - kActionReach, self.w, kActionReach}; break;
    case Facing::Left: probe = {self.x - kActionReach, self.y, kActionReach, self.h}; break;
    case Facing::Right: probe = {self.x + self.w, self.y, kActionReach, self.h}; break;
    }
    // 앞에 있는 객체 중 가장 많이 겹친 것. 없으면 밟고 있는 통과 가능한 객체.
    const SceneObject* best = nullptr;
    double bestArea = 0.0;
    for (const SceneObject& o : scene_->objects) {
        if (o.id == player->id || o.event.trigger != EventTrigger::Action || o.event.commands.empty()) {
            continue;
        }
        double area = overlapArea(probe, rectOf(o));
        if (area == 0.0 && o.passable) {
            area = overlapArea(self, rectOf(o)) * 1e-6; // 앞의 객체보다 낮은 우선순위
        }
        if (area > bestArea) {
            best = &o;
            bestArea = area;
        }
    }
    if (!best) {
        return false;
    }
    queueBlocking(*best);
    return true;
}

void GameWorld::queueBlocking(const SceneObject& object) {
    blockingQueue_.push_back(object.id);
}

void GameWorld::runBlocking(double dt) {
    while (true) {
        if (!blocking_) {
            if (blockingQueue_.empty()) {
                return;
            }
            std::string id = blockingQueue_.front();
            blockingQueue_.pop_front();
            if (const SceneObject* object = scene_->findObject(id)) {
                blocking_ = makeEvent(*object);
            }
            continue;
        }
        if (!tickEvent(*blocking_, dt)) {
            return;
        }
        blocking_.reset();
    }
}

std::unique_ptr<GameWorld::RunningEvent> GameWorld::makeEvent(const SceneObject& object) {
    return std::make_unique<RunningEvent>(*this, object);
}

bool GameWorld::tickEvent(RunningEvent& event, double dt) {
    if (event.runner.status() == CommandStatus::Pending) {
        event.runner.start(event.ctx);
    } else {
        event.runner.update(event.ctx, dt);
    }
    switch (event.runner.status()) {
    case CommandStatus::Completed:
    case CommandStatus::Cancelled:
        return true;
    case CommandStatus::Failed:
        ++failedEvents_;
        logger_.error("이벤트 실패: " + event.objectName + " (" + event.objectId + ")",
                      {"", 0, event.runner.failedCommandId()});
        return true;
    default:
        return false;
    }
}

void GameWorld::stopAll() {
    if (blocking_) {
        blocking_->runner.cancel(blocking_->ctx);
        blocking_.reset();
    }
    for (auto& event : parallel_) {
        event->runner.cancel(event->ctx);
    }
    parallel_.clear();
    blockingQueue_.clear();
}

} // namespace naru
