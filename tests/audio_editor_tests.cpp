#include "TestMain.h"
#include "AssetBrowser.h"
#include "CommandDialog.h"
#include "ProjectDocument.h"
#include "naru/runtime/Audio.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

namespace {
QByteArray readFile(const QString& path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{}; }
}
TEST_CASE("소리 명령 실제 입력창·적용/취소·소리 에셋 참조와 ID Undo") {
    QTemporaryDir temporary;
    QString error;
    auto doc=ProjectDocument::create(temporary.path()+"/project","audio",&error);
    CHECK(doc != nullptr); if (!doc) return;
    const QString source=QString::fromUtf8(NARU_AUDIO_EXAMPLE_DIR)+"/assets/effect.wav";
    AssetBrowser browser(doc.get());
    CHECK(browser.importFiles({source},{"sound"}).isEmpty());
    naru::EventCommand c; c.type=naru::EventCommandType::Audio;
    CommandDialog dialog(c,{},&doc->project());
    dialog.findChild<QDoubleSpinBox*>("audioVolume")->setValue(36);
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    CHECK(dialog.result()==QDialog::Accepted);
    c=dialog.command(); CHECK(c.audioAsset=="sound" && c.volume==36 && c.loop);
    CommandDialog cancel(c,{},&doc->project());
    cancel.findChild<QDoubleSpinBox*>("audioVolume")->setValue(20); cancel.reject(); CHECK(cancel.command()==c);
    CommandDialog stop(c,{},&doc->project());
    auto* action=stop.findChild<QComboBox*>("audioAction"); action->setCurrentIndex(action->findData("stop_bgm"));
    CHECK(!stop.findChild<QComboBox*>("audioAsset")->isEnabled());
    stop.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click(); CHECK(stop.command().audioAction=="stop_bgm");
    auto object=doc->project().scenes[0].objects[0];
    naru::EventCommand choices; choices.type=naru::EventCommandType::ShowChoices; choices.choices={{"재생",{c}}};
    object.event.commands={choices}; doc->setObject(doc->project().scenes[0].id,object);
    auto common=doc->project().commonEvents;
    common.push_back({"audio_common","공통 소리",{}}); common.back().event.commands={c}; doc->setCommonEvents(common);
    const auto before=doc->project();
    CHECK(naru::audioUses(before,"sound").size()==2);
    CHECK(!browser.unregisterAsset("sound"));
    CHECK(browser.renameAssetId("sound","renamed",&error));
    CHECK(naru::audioUses(doc->project(),"renamed").size()==2); CHECK(naru::audioUses(doc->project(),"sound").empty());
    doc->undoStack()->undo(); CHECK(doc->project()==before);
    doc->undoStack()->redo(); CHECK(naru::audioUses(doc->project(),"renamed").size()==2);
    CHECK(doc->save(&error));
    auto loaded=ProjectDocument::open(doc->directory(),&error); CHECK(loaded != nullptr);
    if (loaded) CHECK(naru::audioUses(loaded->project(),"renamed").size()==2);
    QFile broken(temporary.path()+"/broken.mp3"); CHECK(broken.open(QIODevice::WriteOnly)); broken.write("broken mp3"); broken.close();
    const auto valid=doc->project(); CHECK(!browser.importFiles({broken.fileName()}).isEmpty()); CHECK(doc->project()==valid);
    const QString mp3=qEnvironmentVariable("NARU_TEST_MP3_FILE");
    if (!mp3.isEmpty()) {
        const auto original=readFile(mp3); CHECK(!original.isEmpty());
        CHECK(browser.importFiles({mp3},{"user_music"}).isEmpty());
        const auto& a=doc->project().assets.back();
        CHECK(a.kind=="audio" && a.id=="user_music");
        CHECK(readFile(doc->directory()+"/"+QString::fromStdString(a.path))==original);
        CHECK(readFile(mp3)==original);
    }
}
