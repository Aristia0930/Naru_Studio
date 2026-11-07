#include "TestMain.h"
#include "AssetBrowser.h"
#include "ImageSettingsDialog.h"
#include "Inspector.h"
#include "ProjectDocument.h"
#include "SceneView.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

TEST_CASE("이미지 인스펙터 팝업 적용·취소·Undo와 실제 캔버스·사용처 보호") {
    QTemporaryDir temp;
    QString error;
    auto doc = ProjectDocument::create(temp.path() + "/project", "images", &error);
    CHECK(doc != nullptr); if (!doc) return;
    QImage source(4, 2, QImage::Format_ARGB32); source.fill(Qt::red);
    const auto path = temp.path() + "/sprite.png"; CHECK(source.save(path));
    AssetBrowser browser(doc.get()); CHECK(browser.importFiles({path}).isEmpty());
    const auto asset = doc->project().assets[0].id;
    auto scene = doc->project().scenes[0];
    scene.width = 320; scene.height = 240; doc->setSceneInfo(scene);
    auto object = scene.objects[0]; object.x = 100; object.y = 100; object.width = 20; object.height = 10;
    doc->setObject(scene.id, object);
    Inspector inspector(doc.get()); inspector.setTarget(QString::fromStdString(scene.id), QString::fromStdString(object.id));
    QTimer::singleShot(0, &inspector, [&] {
        auto* dialog = inspector.findChild<ImageSettingsDialog*>();
        CHECK(dialog != nullptr); if (!dialog) return;
        auto* images = dialog->findChild<QComboBox*>("imageAsset");
        images->setCurrentIndex(images->findData(QString::fromStdString(asset)));
        dialog->findChild<QDoubleSpinBox*>("imageWidth")->setValue(80);
        dialog->findChild<QDoubleSpinBox*>("imageHeight")->setValue(80);
        dialog->accept();
    });
    inspector.findChild<QPushButton*>("objectImageSettings")->click();
    const auto* changed = doc->project().findScene(scene.id)->findObject(object.id);
    CHECK(changed->image.asset == asset && changed->image.width == 80);
    CHECK(changed->width == 20 && changed->height == 10);
    CHECK(!browser.unregisterAsset(asset));
    SceneView view(doc.get()); view.setSceneId(QString::fromStdString(scene.id));
    QImage rendered(320, 240, QImage::Format_ARGB32); rendered.fill(Qt::black);
    { QPainter painter(&rendered); view.findChild<QGraphicsScene*>()->render(&painter, QRectF(0,0,320,240), QRectF(0,0,320,240)); }
    CHECK(rendered.pixelColor(110, 165) == QColor(Qt::red)); // image extends below the independent collision rectangle
    doc->undoStack()->undo(); CHECK(doc->project().findScene(scene.id)->findObject(object.id)->image.asset.empty());
    doc->undoStack()->redo();
    QTimer::singleShot(0, &inspector, [&] {
        auto* dialog = inspector.findChild<ImageSettingsDialog*>(); if (!dialog) return;
        dialog->findChild<QComboBox*>("imageAsset")->setCurrentIndex(0); dialog->reject();
    });
    inspector.findChild<QPushButton*>("objectImageSettings")->click();
    CHECK(doc->project().findScene(scene.id)->findObject(object.id)->image.asset == asset);
    inspector.setTarget(QString::fromStdString(scene.id), {});
    QTimer::singleShot(0, &inspector, [&] {
        auto* dialog = inspector.findChild<ImageSettingsDialog*>(); if (!dialog) return;
        auto* images = dialog->findChild<QComboBox*>("imageAsset"); images->setCurrentIndex(images->findData(QString::fromStdString(asset)));
        auto* mode = dialog->findChild<QComboBox*>("backgroundMode"); mode->setCurrentIndex(mode->findData("repeat")); dialog->accept();
    });
    inspector.findChild<QPushButton*>("backgroundImageSettings")->click();
    CHECK(doc->project().findScene(scene.id)->backgroundImage.mode == "repeat");
    CHECK(doc->save(&error));
    const auto loaded = ProjectDocument::open(doc->directory(), &error);
    CHECK(loaded && loaded->project() == doc->project());
    // JPEG/BMP go through the native player decoder, not Qt's canvas decoder.
#if defined(NARU_TEST_PLAYER_EXE) && defined(_WIN32)
    for (const auto& suffix : {QString("jpg"), QString("bmp")}) {
        const auto filename = temp.path() + "/sprite." + suffix;
        CHECK(source.save(filename)); CHECK(browser.importFiles({filename}).isEmpty());
        auto info = *doc->project().findScene(scene.id); info.backgroundImage.asset.clear(); doc->setSceneInfo(info);
        auto sprite = *doc->project().findScene(scene.id)->findObject(object.id);
        sprite.image.asset = doc->project().assets.back().id; doc->setObject(scene.id, sprite); CHECK(doc->save(&error));
        const auto shot = temp.path() + "/" + suffix + ".png";
        QProcess player;
        player.start(QString::fromUtf8(NARU_TEST_PLAYER_EXE), {doc->directory(), "--frames", "2", "--screenshot", shot});
        CHECK(player.waitForFinished(15000)); CHECK(player.exitCode() == 0);
        QImage renderedPlayer(shot); CHECK(!renderedPlayer.isNull());
        if (!renderedPlayer.isNull()) {
            const auto red = renderedPlayer.pixelColor(110, 165);
            CHECK(red.red() > 245 && red.green() < 8 && red.blue() < 8);
        }
    }
#endif
}
