#include "TestMain.h"
#include "AnimationEditor.h"
#include "AssetBrowser.h"
#include "Inspector.h"
#include "UndoCommands.h"
#include "CommandDialog.h"
#include <QTimer>
#include <QTemporaryDir>
#include <QApplication>
#include <QMouseEvent>

TEST_CASE("애니메이션 편집·적용/취소·에셋 참조·Undo·저장 재열기") {
    QTemporaryDir temp;QString error;auto doc=ProjectDocument::create(temp.path()+"/project","동작",&error);CHECK(doc!=nullptr);if(!doc)return;
    QImage sheet(64,128,QImage::Format_ARGB32);sheet.fill(Qt::red);CHECK(sheet.save(temp.path()+"/sheet.png"));
    AssetBrowser browser(doc.get());CHECK(browser.importFiles({temp.path()+"/sheet.png"},{"sheet"}).isEmpty());
    auto scene=doc->project().scenes.front();auto actor=scene.objects.front();naru::AnimationClip clip;clip.name="idle";naru::AnimationFrame f;f.asset="sheet";clip.frames={f};actor.animations={clip};doc->setObject(scene.id,actor);
    AnimationEditor dialog(doc.get(),actor);dialog.show();QApplication::processEvents();auto* table=dialog.findChild<QTableWidget*>("animationFrames");CHECK(table&&table->rowCount()==1);table->item(0,1)->setText("32");table->item(0,8)->setText("ready");
    auto* sheetView=dialog.findChild<QWidget*>("animationSheetView");CHECK(sheetView!=nullptr);table->setCurrentCell(0,0);
    dialog.findChild<QSpinBox*>("animationCellWidth")->setValue(10);dialog.findChild<QSpinBox*>("animationCellHeight")->setValue(10);
    QMouseEvent choose(QEvent::MouseButtonPress,QPointF(45,25),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(sheetView,&choose);
    CHECK(table->item(0,1)->text()=="20");CHECK(table->item(0,2)->text()=="10");CHECK(table->item(0,3)->text()=="10");CHECK(table->item(0,4)->text()=="10");
    for(auto* box:dialog.findChildren<QCheckBox*>())if(box->text().contains(QStringLiteral("클릭할 때마다 프레임 추가")))box->setChecked(true);
    QApplication::sendEvent(sheetView,&choose);CHECK(table->rowCount()==2);
    table->removeRow(1);table->item(0,1)->setText("32");
    dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();CHECK(dialog.result()==QDialog::Accepted);CHECK(dialog.object().animations[0].frames[0].x==32);CHECK(doc->project().scenes.front().objects.front()==actor);
    doc->undoStack()->push(new SetObjectCommand(doc.get(),scene.id,actor,dialog.object()));CHECK(!browser.unregisterAsset("sheet"));
    CHECK(browser.renameAssetId("sheet","renamed",&error));CHECK(doc->project().scenes.front().objects.front().animations[0].frames[0].asset=="renamed");doc->undoStack()->undo();CHECK(doc->project().scenes.front().objects.front().animations[0].frames[0].asset=="sheet");
    doc->undoStack()->undo();CHECK(doc->project().scenes.front().objects.front()==actor);doc->undoStack()->redo();CHECK(doc->save(&error));auto loaded=ProjectDocument::open(doc->directory(),&error);CHECK(loaded&&loaded->project()==doc->project());
    AnimationEditor cancelled(doc.get(),actor);cancelled.findChild<QTableWidget*>("animationFrames")->item(0,1)->setText("0");cancelled.reject();CHECK(doc->project().scenes.front().objects.front().animations[0].frames[0].x==32);
    AnimationEditor shot(doc.get(),doc->project().scenes.front().objects.front());shot.show();QApplication::processEvents();
    for(auto* button:shot.findChildren<QPushButton*>())if(button->text()==QStringLiteral("4방향 idle/walk 틀 생성"))button->click();
    CHECK(shot.findChild<QListWidget*>("animationClips")->count()==9);shot.findChild<QListWidget*>("animationClips")->setCurrentRow(6);
    for(auto* button:shot.findChildren<QPushButton*>())if(button->text()==QStringLiteral("미리보기 재생"))button->click();
    QApplication::processEvents();shot.findChild<QTableWidget*>("animationFrames")->setCurrentCell(0,0);const auto path=qEnvironmentVariable("NARU_ANIMATION_SCREENSHOT");if(!path.isEmpty())CHECK(shot.grab().save(path));
    auto* frames=shot.findChild<QTableWidget*>("animationFrames");CHECK(frames->rowCount()==2);frames->item(0,5)->setText("0");
    QTimer::singleShot(0,[]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());CHECK(box!=nullptr);if(box)box->accept();});
    shot.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();CHECK(shot.result()!=QDialog::Accepted);
    naru::EventCommand command;command.type=naru::EventCommandType::ObjectAnimation;command.target=actor.id;command.animation="idle";command.animationLock=true;
    CommandDialog commandDialog(command,{},&doc->project(),&scene);commandDialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();CHECK(commandDialog.result()==QDialog::Accepted);CHECK(commandDialog.command()==command);
}
