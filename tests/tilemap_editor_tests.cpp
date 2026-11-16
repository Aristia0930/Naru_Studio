#include "TestMain.h"
#include "TileMapEditor.h"
#include "AssetBrowser.h"
#include "Inspector.h"
#include "naru/project/ImageLayout.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTimer>

TEST_CASE("레이어 타일맵 칠하기·잠금·Undo·에셋 ID·저장 재열기") {
    QTemporaryDir temp;QString error;
    auto doc=ProjectDocument::create(temp.path()+"/project","타일맵",&error);CHECK(doc!=nullptr);if(!doc)return;
    QImage atlas(64,32,QImage::Format_ARGB32);atlas.fill(Qt::green);QPainter painter(&atlas);painter.fillRect(32,0,32,32,Qt::red);painter.end();CHECK(atlas.save(temp.path()+"/tiles.png"));
    AssetBrowser browser(doc.get());CHECK(browser.importFiles({temp.path()+"/tiles.png"},{"tiles"}).isEmpty());
    auto scene=doc->project().scenes.front();scene.width=1024;scene.height=768;scene.cameraWidth=640;scene.cameraHeight=360;
    naru::TileLayer floor;floor.name="바닥";floor.asset="tiles";
    naru::TileLayer roof=floor;roof.name="지붕";roof.above=true;roof.cells={{0,1}};scene.tileLayers={floor,roof};
    doc->setSceneInfo(scene);
    TileMapEditor dialog(doc.get(),QString::fromStdString(scene.id));dialog.show();QApplication::processEvents();
    auto* canvas=static_cast<TileMapCanvas*>(dialog.findChild<QGraphicsView*>("tileCanvas"));CHECK(canvas!=nullptr);
    auto paint=[&] {
        QPoint point=canvas->mapFromScene(QPointF(48,48));
        QMouseEvent press(QEvent::MouseButtonPress,QPointF(point),QPointF(canvas->viewport()->mapToGlobal(point)),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(),&press);
        QMouseEvent release(QEvent::MouseButtonRelease,QPointF(point),QPointF(canvas->viewport()->mapToGlobal(point)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(),&release);
    };
    canvas->centerOn(100,100);paint();
    CHECK(doc->project().scenes.front().tileLayers[0].cells.contains(33));
    doc->undoStack()->undo();CHECK(doc->project().scenes.front().tileLayers[0].cells.empty());doc->undoStack()->redo();
    auto changed=doc->project().scenes.front();changed.tileLayers[0].locked=true;doc->setSceneInfo(changed);
    canvas->tile=1;paint();CHECK(doc->project().scenes.front().tileLayers[0].cells.at(33)==0);
    CHECK(!browser.unregisterAsset("tiles"));CHECK(browser.renameAssetId("tiles","renamed",&error));
    CHECK(doc->project().scenes.front().tileLayers[0].asset=="renamed");doc->undoStack()->undo();CHECK(doc->project().scenes.front().tileLayers[0].asset=="tiles");
    CHECK(doc->save(&error));auto loaded=ProjectDocument::open(doc->directory(),&error);CHECK(loaded && loaded->project()==doc->project());
    auto click=[&](const QString& text){for(auto* button:dialog.findChildren<QPushButton*>())if(button->text()==text){button->click();return;}CHECK(false);};
    const auto originalLayers=doc->project().scenes.front().tileLayers;
    click(QStringLiteral("추가"));CHECK(doc->project().scenes.front().tileLayers.size()==3);
    dialog.findChild<QLineEdit*>()->setText(QStringLiteral("장식"));click(QStringLiteral("레이어 설정 적용"));
    CHECK(doc->project().scenes.front().tileLayers.back().name=="장식");
    click(QStringLiteral("앞으로"));CHECK(doc->project().scenes.front().tileLayers[1].name=="장식");
    QTimer::singleShot(0,[]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});
    click(QStringLiteral("삭제"));CHECK(doc->project().scenes.front().tileLayers==originalLayers);
    doc->undoStack()->undo();CHECK(doc->project().scenes.front().tileLayers.size()==3);
    doc->undoStack()->redo();CHECK(doc->project().scenes.front().tileLayers==originalLayers);
    const auto originalScene=doc->project().scenes.front();
    auto answer=[&](QMessageBox::StandardButton response){QTimer::singleShot(0,[response]{auto* box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());CHECK(box!=nullptr);if(box){const auto shot=qEnvironmentVariable("NARU_TILEMAP_RESIZE_SCREENSHOT");if(!shot.isEmpty())CHECK(box->grab().save(shot));box->button(response)->click();}});};
    auto* mapWidth=dialog.findChild<QSpinBox*>("tileMapWidth");auto* mapHeight=dialog.findChild<QSpinBox*>("tileMapHeight");
    mapWidth->setValue(2048);answer(QMessageBox::No);click(QStringLiteral("맵/카메라 설정 적용"));CHECK(doc->project().scenes.front()==originalScene);
    answer(QMessageBox::Yes);click(QStringLiteral("맵/카메라 설정 적용"));
    CHECK(doc->project().scenes.front().width==2048);CHECK(doc->project().scenes.front().tileLayers[0].cells.contains(65));
    CHECK(!doc->project().scenes.front().tileLayers[0].cells.contains(33));
    mapWidth->setValue(32);mapHeight->setValue(32);answer(QMessageBox::Yes);click(QStringLiteral("맵/카메라 설정 적용"));
    CHECK(doc->project().scenes.front().tileLayers[0].cells.empty());
    CHECK(doc->project().scenes.front().tileLayers[1].cells.at(0)==1);
    doc->undoStack()->undo();CHECK(doc->project().scenes.front().width==2048);CHECK(doc->project().scenes.front().tileLayers[0].cells.at(65)==0);
    doc->undoStack()->undo();CHECK(doc->project().scenes.front()==originalScene);
    doc->undoStack()->redo();doc->undoStack()->redo();CHECK(doc->save(&error));loaded=ProjectDocument::open(doc->directory(),&error);CHECK(loaded && loaded->project()==doc->project());
    doc->undoStack()->undo();doc->undoStack()->undo();
    dialog.hide();
    Inspector inspector(doc.get());inspector.setTarget(QString::fromStdString(scene.id),{});inspector.show();QApplication::processEvents();
    auto* inspectorWidth=inspector.findChild<QSpinBox*>("sceneWidth");auto* inspectorHeight=inspector.findChild<QSpinBox*>("sceneHeight");
    CHECK(inspectorWidth->isEnabled());CHECK(inspectorHeight->isEnabled());
    answer(QMessageBox::No);inspectorWidth->setValue(2048);CHECK(doc->project().scenes.front()==originalScene);CHECK(inspectorWidth->value()==1024);
    answer(QMessageBox::Yes);inspectorWidth->setValue(2048);CHECK(doc->project().scenes.front().tileLayers[0].cells.at(65)==0);
    CHECK(inspectorHeight->isEnabled());
    answer(QMessageBox::Yes);inspectorHeight->setValue(32);CHECK(doc->project().scenes.front().tileLayers[0].cells.empty());
    doc->undoStack()->undo();CHECK(doc->project().scenes.front().height==768);CHECK(doc->project().scenes.front().tileLayers[0].cells.at(65)==0);
    doc->undoStack()->undo();CHECK(doc->project().scenes.front()==originalScene);
    doc->undoStack()->redo();CHECK(doc->save(&error));loaded=ProjectDocument::open(doc->directory(),&error);CHECK(loaded && loaded->project()==doc->project());
    const auto inspectorShot=qEnvironmentVariable("NARU_MAP_INSPECTOR_SCREENSHOT");if(!inspectorShot.isEmpty())CHECK(inspector.grab().save(inspectorShot));
    const auto screenshot=qEnvironmentVariable("NARU_TILEMAP_SCREENSHOT");if(!screenshot.isEmpty())CHECK(dialog.grab().save(screenshot));
}
