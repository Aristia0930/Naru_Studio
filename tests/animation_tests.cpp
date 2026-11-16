#include "TestMain.h"
#include "naru/runtime/GameWorld.h"
#include "naru/project/AnimationVisual.h"

TEST_CASE("공용 이펙트는 두 개를 동시에 재생하고 끝나거나 중지하면 정리된다") {
    naru::Project p;naru::Effect effect;effect.id="fire";effect.name="불꽃";
    naru::EffectFrame frame;frame.image="sheet";frame.seconds=.05;effect.frames={frame,frame};p.effects={effect};
    naru::Scene scene;scene.id="stage";scene.playerObjectId="hero";naru::SceneObject hero;hero.id="hero";scene.objects={hero};p.scenes={scene};
    naru::GameWorld world(p,naru::ScriptRunner{});CHECK(world.startGame("stage"));
    auto first=world.playEffect("fire");auto second=world.playEffect("fire","@screen");CHECK(first!=second);CHECK(world.activeEffects().size()==2);
    world.update(.05,naru::InputState{});CHECK(world.effectPlaying(first));CHECK(world.activeEffects()[0].frame==1);
    world.stopEffect(first);CHECK(!world.effectPlaying(first));CHECK(world.effectPlaying(second));
    world.update(.05,naru::InputState{});CHECK(!world.effectPlaying(second));CHECK(world.activeEffects().empty());
    CHECK(world.playEffect("fire")>0);world.enterScene("stage");CHECK(world.activeEffects().empty());
}

TEST_CASE("이펙트 표시 이벤트는 실제 재생을 기다리고 F6에서 그림을 재생하지 않는다") {
    naru::Project p;naru::Effect effect;effect.id="spark";effect.name="불꽃";naru::EffectFrame frame;frame.image="sheet";frame.seconds=.05;effect.frames={frame};p.effects={effect};
    naru::Scene scene;scene.id="stage";naru::SceneObject actor;actor.id="actor";scene.objects={actor};p.scenes={scene};
    naru::GameWorld world(p,naru::ScriptRunner{});CHECK(world.startGame("stage"));
    naru::EventCommand command;command.type=naru::EventCommandType::ShowEffect;command.effect="spark";command.target="actor";command.waitForEnd=true;
    naru::CommandContext ctx{world.variables(),world.logger()};ctx.game=&world;
    naru::EventRunner runner(naru::compileEvent({command},{}));runner.start(ctx);CHECK(runner.status()==naru::CommandStatus::Waiting);CHECK(world.activeEffects().size()==1);
    world.update(.05,naru::InputState{});runner.update(ctx,.05);CHECK(runner.status()==naru::CommandStatus::Completed);CHECK(world.activeEffects().empty());
    ctx.game=nullptr;naru::EventRunner preview(naru::compileEvent({command},{}));preview.start(ctx);CHECK(preview.status()==naru::CommandStatus::Completed);CHECK(world.activeEffects().empty());
}

TEST_CASE("클립 방향·한 번 재생·신호·잠금·자동 복귀·장면/F8 정리") {
    naru::Project p;naru::Scene scene;scene.id="stage";scene.playerObjectId="hero";
    naru::SceneObject hero;hero.id="hero";hero.x=100;hero.y=100;hero.width=16;hero.height=16;
    naru::AnimationFrame f;f.asset="sheet";f.width=32;f.height=48;f.seconds=.05;
    naru::AnimationClip idle;idle.name="idle";idle.frames={f};
    naru::AnimationClip walk=idle;walk.name="walk_right";walk.frames[0].x=32;
    naru::AnimationClip attack=idle;attack.name="attack_right";attack.loop=false;attack.frames[0].signal="start";f.x=64;f.signal="hit";attack.frames.push_back(f);
    hero.animations={idle,walk,attack};scene.objects={hero};p.scenes={scene};
    naru::GameWorld world(p,naru::ScriptRunner{});CHECK(world.startGame("stage"));world.setFacing(naru::Facing::Right);
    auto token=world.playObjectAnimation("hero","attack",false,true);CHECK(token>0);CHECK(!world.movePlayer(50,0));CHECK(world.player()->x==100);
    auto signals=world.takeAnimationSignals();CHECK(signals.size()==1&&signals[0].signal=="start");
    world.update(.05,naru::InputState{});signals=world.takeAnimationSignals();CHECK(signals.size()==1&&signals[0].signal=="hit"&&signals[0].frame==1);
    CHECK(world.player()->animationState.frame==1);world.update(.05,naru::InputState{});CHECK(!world.objectAnimating("hero"));CHECK(world.player()->animationState.clip=="idle");
    CHECK(world.movePlayer(10,0));world.update(1./60,naru::InputState{});CHECK(world.player()->animationState.clip=="walk_right");
    world.update(1./60,naru::InputState{});CHECK(world.player()->animationState.clip=="idle");
    token=world.playObjectAnimation("hero","attack",true,true);CHECK(world.startObjectMove("hero",48,0,240));world.update(.2,naru::InputState{});CHECK(world.player()->x==110);CHECK(world.objectMoving("hero"));
    auto newer=world.playObjectAnimation("hero","attack",false,false);CHECK(newer!=token);CHECK(!world.objectAnimating("hero",token));world.update(1./60,naru::InputState{});CHECK(world.player()->x>110);
    world.stopEvents();CHECK(!world.objectAnimating("hero"));CHECK(world.takeAnimationSignals().empty());CHECK(world.movePlayer(1,0));
    CHECK(world.enterScene("stage"));CHECK(world.player()->x==100);CHECK(world.project()==p);
    bool failed=false;try{world.playObjectAnimation("hero","missing");}catch(const std::invalid_argument&){failed=true;}CHECK(failed);
    auto visual=naru::animationVisual(*world.player());CHECK(visual.width==16&&visual.height==16);CHECK(visual.image.width==32&&visual.image.height==48);CHECK(visual.image.offsetX==8&&visual.image.offsetY==16);
}

TEST_CASE("객체 애니메이션 이벤트는 대기·F6·취소·잘못된 반복 조합을 구분한다") {
    naru::Project p;naru::Scene scene;scene.id="stage";
    naru::SceneObject actor;actor.id="actor";naru::AnimationFrame frame;frame.asset="sheet";frame.seconds=.05;
    naru::AnimationClip clip;clip.name="attack";clip.frames={frame};actor.animations={clip};scene.objects={actor};p.scenes={scene};
    naru::GameWorld world(p,naru::ScriptRunner{});CHECK(world.startGame("stage"));
    naru::EventCommand command;command.type=naru::EventCommandType::ObjectAnimation;command.target="actor";command.animation="attack";command.waitForEnd=true;
    naru::CommandContext ctx{world.variables(),world.logger()};ctx.game=&world;
    naru::EventRunner runner(naru::compileEvent({command},{}));runner.start(ctx);CHECK(runner.status()==naru::CommandStatus::Waiting);
    world.update(.05,naru::InputState{});runner.update(ctx,.05);CHECK(runner.status()==naru::CommandStatus::Completed);
    naru::EventRunner cancelled(naru::compileEvent({command},{}));cancelled.start(ctx);cancelled.cancel(ctx);CHECK(!world.objectAnimating("actor"));
    command.animationLoop=true;naru::EventRunner invalid(naru::compileEvent({command},{}));invalid.start(ctx);CHECK(invalid.status()==naru::CommandStatus::Failed);
    command.animationLoop=false;ctx.game=nullptr;naru::EventRunner preview(naru::compileEvent({command},{}));preview.start(ctx);CHECK(preview.status()==naru::CommandStatus::Completed);CHECK(!world.objectAnimating("actor"));
}
