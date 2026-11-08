#include "TestMain.h"
#include "Inspector.h"
#include "ProjectDocument.h"
#include "SceneView.h"
#include <QFile>
#include <QLineEdit>
#include <QTemporaryDir>

TEST_CASE("객체 ID 편집·전역 충돌·중첩 참조·선택 유지·Undo·저장 재열기") {
    QTemporaryDir temp;
    QString error;
    auto doc = ProjectDocument::create(temp.path() + "/project", "object ids", &error);
    CHECK(doc != nullptr); if (!doc) return;
    auto scene = doc->project().scenes.front();
    auto object = scene.objects.front();
    const auto old = object.id;
    const std::string next = "hero_main";
    scene.playerObjectId = old; doc->setSceneInfo(scene);
    naru::EventCommand move; move.type = naru::EventCommandType::MoveObject; move.target = old;
    naru::EventCommand visible = move; visible.type = naru::EventCommandType::SetVisible;
    naru::EventCommand branch; branch.type = naru::EventCommandType::IfVariable;
    branch.thenCommands = {move}; branch.elseCommands = {visible};
    naru::EventCommand choices; choices.type = naru::EventCommandType::ShowChoices;
    choices.choices.push_back({"yes", {branch}});
    naru::EventCommand self = move; self.target = "@self";
    naru::EventCommand player = move; player.target = "@player";
    naru::EventCommand dangling = move; dangling.target = next;
    naru::EventCommand text; text.type = naru::EventCommandType::Log; text.text = old;
    object.event.commands = {choices, self, player, dangling, text}; doc->setObject(scene.id, object);
    naru::Scene other; other.id = "other"; other.name = "Other";
    naru::SceneObject observer; observer.id = "observer"; observer.name = "Observer";
    observer.event.commands = {visible}; other.objects.push_back(observer);
    doc->insertScene(1, other);
    naru::CommonEvent common; common.id = "common"; common.name = "Common"; common.event.commands = {choices};
    doc->setCommonEvents({common});
    const auto beforeScenes = doc->project().scenes;
    const auto beforeCommon = doc->project().commonEvents;
    QFile script(doc->scriptsDirectory() + "/identity.py");
    const QByteArray scriptSource = "def run(ctx):\n    return ctx.scene.object('" + QByteArray::fromStdString(old) + "')\n";
    CHECK(script.open(QIODevice::WriteOnly)); CHECK(script.write(scriptSource) == scriptSource.size()); script.close();
    Inspector inspector(doc.get()); inspector.setTarget(QString::fromStdString(scene.id), QString::fromStdString(old));
    SceneView view(doc.get()); view.setSceneId(QString::fromStdString(scene.id)); view.selectObject(QString::fromStdString(old));
    for (const auto& bad : {QString(), QString("bad id"), QString("../id"), QString("@player"), QString("id\n"), QString(65, 'a'), QString("observer")}) {
        CHECK(!doc->renameObjectId(QString::fromStdString(scene.id), QString::fromStdString(old), bad, &error));
        CHECK(!error.isEmpty());
        CHECK(doc->undoStack()->count() == 0);
    }
    auto* edit = inspector.findChild<QLineEdit*>("objectId"); CHECK(edit != nullptr); if (!edit) return;
    edit->setText(QString::fromStdString(next)); QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
    CHECK(doc->undoStack()->count() == 1);
    CHECK(view.selectedObjectId() == QString::fromStdString(next));
    const auto* changedScene = doc->project().findScene(scene.id);
    const auto* changed = changedScene->findObject(next); CHECK(changed != nullptr); if (!changed) return;
    CHECK(changedScene->playerObjectId == next);
    const auto& commands = changed->event.commands;
    CHECK(commands[0].choices[0].commands[0].thenCommands[0].target == next);
    CHECK(commands[0].choices[0].commands[0].elseCommands[0].target == next);
    CHECK(commands[1].target == "@self" && commands[2].target == "@player");
    CHECK(commands[4].text == old);
    CHECK(doc->project().findScene(other.id)->objects[0].event.commands[0].target == next);
    CHECK(doc->project().commonEvents[0].event.commands[0].choices[0].commands[0].thenCommands[0].target == next);
    const auto afterScenes = doc->project().scenes;
    const auto afterCommon = doc->project().commonEvents;
    doc->undoStack()->undo();
    CHECK(doc->project().scenes == beforeScenes && doc->project().commonEvents == beforeCommon);
    CHECK(edit->text() == QString::fromStdString(old));
    CHECK(view.selectedObjectId() == QString::fromStdString(old));
    doc->undoStack()->redo();
    CHECK(doc->project().scenes == afterScenes && doc->project().commonEvents == afterCommon);
    CHECK(edit->text() == QString::fromStdString(next));
    CHECK(doc->save(&error));
    auto reopened = ProjectDocument::open(doc->directory(), &error); CHECK(reopened != nullptr); if (!reopened) return;
    CHECK(reopened->project().scenes == afterScenes && reopened->project().commonEvents == afterCommon);
    CHECK(script.open(QIODevice::ReadOnly)); CHECK(script.readAll() == scriptSource); script.close();
    CHECK(doc->renameObjectId(QString::fromStdString(scene.id), QString::fromStdString(next), QString::fromStdString(next), &error));
    CHECK(doc->undoStack()->count() == 1);
}
