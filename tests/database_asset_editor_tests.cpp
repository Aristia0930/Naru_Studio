#include "TestMain.h"
#include "AssetBrowser.h"
#include "DatabaseEditor.h"
#include "FieldDialog.h"
#include "ProjectDocument.h"
#include "ValueEditor.h"
#include "naru/project/ImageLayout.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QImage>
#include <QPushButton>
#include <QTemporaryDir>

TEST_CASE("DB 에셋 선택·기본값·잘못된 종류·ID 변경·Undo·저장 재열기") {
    QTemporaryDir temporary;
    QString error;
    auto doc = ProjectDocument::create(temporary.path()+"/project", "DB assets", &error);
    CHECK(doc != nullptr); if (!doc) return;
    QImage image(16,16,QImage::Format_ARGB32); image.fill(Qt::red);
    CHECK(image.save(temporary.path()+"/image.png"));
    AssetBrowser browser(doc.get());
    CHECK(browser.importFiles({temporary.path()+"/image.png"}, {"picture"}).isEmpty());
    CHECK(browser.importFiles({QString::fromUtf8(NARU_AUDIO_EXAMPLE_DIR)+"/assets/effect.wav"}, {"sound"}).isEmpty());
    naru::FieldDef picture; picture.key="picture"; picture.name="그림"; picture.type=naru::FieldType::Image; picture.defaultValue=std::string("picture");
    naru::FieldDef sound; sound.key="sound"; sound.name="소리"; sound.type=naru::FieldType::Audio;
    naru::DataType type; type.key="media"; type.name="표현 자료"; type.fields={picture,sound};
    type.records={{"demo","예제",{{"picture",std::string("picture")}}}};
    DatabaseEditor editor(doc.get()); CHECK(editor.addType(type)); editor.select("media","demo");
    CHECK(!editor.setRecordValue("media","demo","picture",std::string("sound")));
    CHECK(!editor.setRecordValue("media","demo","sound",std::string("missing")));
    CHECK(editor.setRecordValue("media","demo","sound",std::string("sound")));
    ValueEditor picker(picture, picture.defaultValue, &doc->project().database, nullptr, doc.get());
    auto* combo=picker.findChild<QComboBox*>("dbAssetValue"); CHECK(combo != nullptr);
    CHECK(combo->findData("sound")==-1 && combo->findData("picture")>=0);
    CHECK(!combo->itemIcon(combo->findData("picture")).isNull());
    combo->setCurrentIndex(0); CHECK(picker.value()==naru::Value{std::monostate{}});
    CHECK(!picker.findChild<QPushButton*>("dbAssetPreview")->isEnabled());
    combo->setCurrentIndex(combo->findData("picture"));
    CHECK(picker.findChild<QPushButton*>("dbAssetPreview")->isEnabled());
    ValueEditor missing(picture,std::string("missing"),&doc->project().database,nullptr,doc.get());
    CHECK(missing.value()==naru::Value{std::string("missing")});
    CHECK(!missing.findChild<QPushButton*>("dbAssetPreview")->isEnabled());
    FieldDialog defaults(picture,doc->project().database.types.back(),doc->project().database,false,nullptr,doc.get());
    CHECK(defaults.findChild<QComboBox*>("dbAssetValue")->currentData().toString()=="picture");
    defaults.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    CHECK(defaults.result()==QDialog::Accepted && defaults.field()==picture);
    CHECK(naru::imageUses(doc->project(),"picture").size()==2);
    CHECK(!browser.unregisterAsset("picture") && !browser.unregisterAsset("sound"));
    const auto before=doc->project();
    CHECK(browser.renameAssetId("picture","renamed",&error));
    CHECK(naru::databaseAssetUses(doc->project().database,"renamed").size()==2);
    CHECK(naru::databaseAssetUses(doc->project().database,"picture").empty());
    doc->undoStack()->undo(); CHECK(doc->project()==before);
    doc->undoStack()->redo(); CHECK(doc->save(&error));
    auto loaded=ProjectDocument::open(doc->directory(),&error);
    CHECK(loaded && loaded->project()==doc->project());
    CHECK(editor.setRecordValue("media","demo","picture",std::monostate{}));
    CHECK(naru::databaseAssetUses(doc->project().database,"renamed").size()==1);
    auto changed=picture; changed.defaultValue=std::monostate{};
    CHECK(editor.setField("media",0,changed,false));
    CHECK(browser.unregisterAsset("renamed"));
}
