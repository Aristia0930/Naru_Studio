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
    return o.visible && !o.event.commands.empty() &&
           (o.event.trigger == EventTrigger::Touch || o.event.trigger == EventTrigger::EventTouch);
}

bool runsAutomatically(const SceneObject& o) {
    return !o.event.commands.empty() &&
           (o.event.trigger == EventTrigger::Autorun || o.event.trigger == EventTrigger::Parallel);
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
        ctx.selfObjectId = object.id;
        logger.addSink([&world, name = object.name](const LogEntry& entry) {
            LogEntry copy = entry;
            copy.message = name + " ▸ " + copy.message;
            world.logger_.log(std::move(copy));
        });
    }
};

GameWorld::GameWorld(Project project, ScriptRunner scripts, double fixedStep)
    : project_(std::move(project)), scripts_(std::move(scripts)), fixedStep_(fixedStep),
      hookCtx_{variables_, logger_, this, std::nullopt, &project_.database, this} {
    variables_ = project_.initialVariables();
}

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

bool GameWorld::enterScene(const std::string& sceneId, const Arrival& arrival) {
    const Scene* source = project_.findScene(sceneId);
    if (!source) {
        logger_.error("장면이 없습니다: " + sceneId);
        return false;
    }
    if (scene_) {
        callHook(kHookSceneExit);
    }
    stopAll();
    closeMessage();
    contacts_.clear();
    tickContacts_.clear();
    autoStarted_.clear();
    objectBumps_.clear();
    tickObjectBumps_.clear();
    playerMovedThisTick_ = false;
    facing_ = arrival.facing.value_or(Facing::Down);
    scene_ = *source;
    logger_.info("장면 시작: " + scene_->name);
    if (!scene_->playerObjectId.empty() && !playerObject()) {
        logger_.warning("플레이어 객체가 장면에 없습니다: " + scene_->playerObjectId);
    }
    if (arrival.x || arrival.y) {
        if (SceneObject* player = playerObject()) {
            player->x = arrival.x.value_or(player->x);
            player->y = arrival.y.value_or(player->y);
        } else {
            logger_.warning("플레이어 객체가 없는 장면이라 도착 위치를 쓰지 않습니다: " + scene_->name);
        }
    }
    callHook(kHookSceneEnter);
    startAutoEvents();
    return true;
}

bool GameWorld::conditionsMet(const SceneObject& object) const {
    return naru::conditionsMet(object.event.conditions, variables_);
}

void GameWorld::startAutoEvents() {
    if (!scene_) {
        return;
    }
    for (const SceneObject& object : scene_->objects) {
        if (!runsAutomatically(object) || (!object.event.repeat && autoStarted_.count(object.id)) ||
            !conditionsMet(object)) {
            continue;
        }
        if (object.event.trigger == EventTrigger::Autorun) {
            if (!queueBlocking(object)) {
                continue; // 아직 실행 중이거나 기다리는 중
            }
        } else {
            if (std::any_of(parallel_.begin(), parallel_.end(),
                            [&object](const auto& e) { return e->objectId == object.id; })) {
                continue;
            }
            parallel_.push_back(makeEvent(object));
        }
        autoStarted_.insert(object.id);
    }
}

void GameWorld::update(double realSeconds, const InputState& input) {
    if (!scene_) {
        return;
    }
    tickInput_ = input;
    // 대사는 틱보다 먼저 닫는다. 이번 프레임에 새로 뜬 대사가 같은 키 입력으로 바로 넘어가지 않게 하고,
    // 대사를 넘긴 결정 키가 곧바로 앞의 객체를 다시 조사하지 않게 한다 (스크립트에도 "새로 누름"으로 보이지 않는다).
    bool actionPressed = input.wasPressed(InputAction::Confirm);
    const bool dialogueInput = message_.has_value();
    if (choices()) {
        if (input.wasPressed(InputAction::Up)) {
            selectedChoice_ = (selectedChoice_ + choices()->options.size() - 1) % choices()->options.size();
        } else if (input.wasPressed(InputAction::Down)) {
            selectedChoice_ = (selectedChoice_ + 1) % choices()->options.size();
        }
        if (actionPressed) {
            selectChoice(selectedChoice_);
        } else if (input.wasPressed(InputAction::Cancel) && choices()->cancelChoice >= 0) {
            selectChoice(static_cast<std::size_t>(choices()->cancelChoice));
        }
        for (auto action : {InputAction::Up, InputAction::Down, InputAction::Confirm, InputAction::Cancel}) {
            tickInput_.consume(toKey(action));
        }
        actionPressed = false;
    } else if (message_ && actionPressed) {
        closeMessage();
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
        // 끝난 반복 이벤트는 다음 틱에 다시 시작한다 (같은 틱 안에서 끝없이 돌지 않게).
        startAutoEvents();
        runBlocking(fixedStep_);
        if (!dialogueInput && !busy() && defaultControl_) {
            controlPlayer(actionPressed, fixedStep_);
        }
        advanceRoutes(fixedStep_);
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
            enterScene(next, pendingArrival_);
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

bool GameWorld::showChoices(const std::shared_ptr<ChoiceRequest>& request) {
    if (!DialogueHost::showChoices(request)) return false;
    selectedChoice_ = static_cast<std::size_t>(request->defaultChoice);
    return true;
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
    for (const std::string& id : moveAxes(*player, dx, dy)) {
        tickContacts_.insert(id);
    }
    playerMovedThisTick_ = true;
    return true;
}

bool GameWorld::setPlayerPosition(double x, double y) {
    const SceneObject* player = playerObject();
    return player && setObjectPosition(player->id, x, y);
}

const SceneObject* GameWorld::object(const std::string& id) const {
    return scene_ ? scene_->findObject(id) : nullptr;
}

bool GameWorld::moveObject(const std::string& id, double dx, double dy) {
    SceneObject* o = scene_ ? scene_->findObject(id) : nullptr;
    if (!o) {
        return false;
    }
    if (o->id == scene_->playerObjectId) {
        return movePlayer(dx, dy);
    }
    // 막는 객체가 움직이다 플레이어에 부딪히면 "이벤트 접촉" (틱 끝에 판정).
    for (const std::string& blocker : moveAxes(*o, dx, dy)) {
        if (blocker == scene_->playerObjectId) {
            tickObjectBumps_.insert(o->id);
        }
    }
    return true;
}

bool GameWorld::setObjectPosition(const std::string& id, double x, double y) {
    SceneObject* o = scene_ ? scene_->findObject(id) : nullptr;
    if (!o) {
        return false;
    }
    o->x = x;
    o->y = y;
    if (o->id == scene_->playerObjectId) {
        playerMovedThisTick_ = true;
    }
    return true;
}

bool GameWorld::setObjectPassable(const std::string& id, bool passable) {
    SceneObject* o = scene_ ? scene_->findObject(id) : nullptr;
    if (!o) {
        return false;
    }
    o->passable = passable;
    return true;
}

bool GameWorld::setObjectVisible(const std::string& id, bool visible) {
    SceneObject* o = scene_ ? scene_->findObject(id) : nullptr;
    if (!o) {
        return false;
    }
    o->visible = visible;
    return true;
}

std::string GameWorld::resolveTarget(const std::string& target, const std::string& selfId) const {
    if (!scene_) {
        return {};
    }
    std::string id = target == kTargetSelf ? selfId : target == kTargetPlayer ? scene_->playerObjectId : target;
    return !id.empty() && scene_->findObject(id) ? id : std::string();
}

bool GameWorld::startObjectMove(const std::string& id, double dx, double dy, double speed) {
    if (!object(id)) {
        return false;
    }
    stopObjectMove(id);
    double length = std::hypot(dx, dy);
    if (length == 0.0) {
        return true;
    }
    if (speed <= 0.0) {
        return moveObject(id, dx, dy);
    }
    routes_.push_back({id, dx / length, dy / length, length, speed});
    return true;
}

bool GameWorld::objectMoving(const std::string& id) const {
    return std::any_of(routes_.begin(), routes_.end(), [&id](const Route& r) { return r.objectId == id; });
}

void GameWorld::stopObjectMove(const std::string& id) {
    std::erase_if(routes_, [&id](const Route& r) { return r.objectId == id; });
}

void GameWorld::advanceRoutes(double dt) {
    // moveObject가 접촉 이벤트를 예약할 수는 있어도 routes_를 바꾸지는 않는다.
    for (std::size_t i = 0; i < routes_.size();) {
        Route& route = routes_[i];
        const SceneObject* o = object(route.objectId);
        bool done = o == nullptr;
        if (o) {
            double step = std::min(route.speed * dt, route.remaining);
            double x = o->x;
            double y = o->y;
            moveObject(route.objectId, route.dirX * step, route.dirY * step);
            double moved = std::hypot(o->x - x, o->y - y);
            route.remaining -= step;
            // 막혀서 덜 움직였으면 그 자리에서 끝낸다.
            done = route.remaining <= 1e-9 || moved + 1e-9 < step;
        }
        if (done) {
            routes_.erase(routes_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

bool GameWorld::setObjectColor(const std::string& id, const std::string& color) {
    SceneObject* o = scene_ ? scene_->findObject(id) : nullptr;
    if (!o) {
        return false;
    }
    o->color = color;
    return true;
}

std::vector<std::string> GameWorld::moveAxes(SceneObject& mover, double dx, double dy) {
    std::vector<std::string> blockers;
    if (dx != 0) {
        moveAxis(mover, dx, 0.0, blockers);
    }
    if (dy != 0) {
        moveAxis(mover, 0.0, dy, blockers);
    }
    return blockers;
}

void GameWorld::moveAxis(SceneObject& mover, double dx, double dy, std::vector<std::string>& blockers) {
    Rect before = rectOf(mover);
    Rect after{before.x + dx, before.y + dy, before.w, before.h};
    // 장면 가장자리
    after.x = std::clamp(after.x, 0.0, std::max(0.0, scene_->width - after.w));
    after.y = std::clamp(after.y, 0.0, std::max(0.0, scene_->height - after.h));

    // 통과 가능한 객체는 아무것에도 막히지 않고, 아무것도 막지 않는다.
    // 숨긴 객체도 마찬가지다.
    auto blocks = [&mover](const SceneObject& o) {
        return o.id != mover.id && !mover.passable && !o.passable && mover.visible && o.visible;
    };
    // 지나간 경로 전체(이전 위치 ~ 새 위치)로 검사한다. 한 번에 크게 움직여도 벽을 건너뛰지 않게.
    Rect swept{std::min(before.x, after.x), std::min(before.y, after.y), before.w + std::abs(after.x - before.x),
               before.h + std::abs(after.y - before.y)};
    for (const SceneObject& o : scene_->objects) {
        if (!blocks(o)) {
            continue;
        }
        Rect wall = rectOf(o);
        // 처음부터 겹쳐 있던 객체에는 막히지 않는다 (배치 실수로 갇히지 않게).
        if (!overlaps(swept, wall) || overlaps(before, wall)) {
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
        blockers.push_back(o.id);
    }
    // 이미 벽에 붙어 있는 상태에서 그쪽으로 밀면 움직이지 않지만 "부딪힘"으로 기록해야 한다.
    if (after.x == before.x && after.y == before.y) {
        Rect probe{before.x + (dx > 0 ? 1 : dx < 0 ? -1 : 0), before.y + (dy > 0 ? 1 : dy < 0 ? -1 : 0), before.w,
                   before.h};
        for (const SceneObject& o : scene_->objects) {
            if (blocks(o) && overlaps(probe, rectOf(o)) && !overlaps(before, rectOf(o))) {
                blockers.push_back(o.id);
            }
        }
    }
    mover.x = after.x;
    mover.y = after.y;
}

void GameWorld::finishContacts() {
    const SceneObject* player = playerObject();
    // 객체가 움직여 플레이어에게 부딪힌 경우: 새로 부딪힌 "이벤트 접촉" 객체만 (계속 밀고 있으면 반복 안 함)
    if (player && !busy()) {
        for (const std::string& id : tickObjectBumps_) {
            const SceneObject* o = scene_->findObject(id);
            if (o && o->visible && !objectBumps_.count(id) && o->event.trigger == EventTrigger::EventTouch &&
                !o->event.commands.empty() && conditionsMet(*o)) {
                queueBlocking(*o);
            }
        }
    }
    objectBumps_ = std::move(tickObjectBumps_);
    tickObjectBumps_.clear();

    // 이벤트·대사 중에 플레이어가 움직이지 않았다면 접촉 상태를 그대로 둔다 (끝난 뒤 다시 실행되지 않게).
    if (!player || !player->visible || (busy() && !playerMovedThisTick_)) {
        tickContacts_.clear();
        playerMovedThisTick_ = false;
        return;
    }
    // 겹쳐 있는 동안 닿아 있는 것으로 본다 (둘 중 하나가 통과 가능해서 겹칠 수 있을 때).
    // 누가 움직였든 같다 — 통과 가능한 "이벤트 접촉" 객체가 플레이어 위로 와도 실행된다.
    Rect self = rectOf(*player);
    for (const SceneObject& o : scene_->objects) {
        if (o.id != player->id && o.visible && (o.passable || player->passable) && overlaps(self, rectOf(o))) {
            tickContacts_.insert(o.id);
        }
    }
    if (!busy()) {
        for (const SceneObject& o : scene_->objects) {
            if (tickContacts_.count(o.id) && !contacts_.count(o.id) && runsOnContact(o) && conditionsMet(o)) {
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
    if (!player || !player->visible || busy()) {
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
        if (o.id == player->id || !o.visible || o.event.trigger != EventTrigger::Action || o.event.commands.empty() ||
            !conditionsMet(o)) {
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

bool GameWorld::queueBlocking(const SceneObject& object) {
    // 같은 객체 이벤트가 이미 실행 중이거나 기다리고 있으면 또 넣지 않는다.
    if ((blocking_ && blocking_->objectId == object.id) ||
        std::find(blockingQueue_.begin(), blockingQueue_.end(), object.id) != blockingQueue_.end()) {
        return false;
    }
    blockingQueue_.push_back(object.id);
    return true;
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
    routes_.clear();
}

} // namespace naru
