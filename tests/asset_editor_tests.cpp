#include "TestMain.h"
#include "AssetBrowser.h"
#include "ProjectDocument.h"
#include "naru/project/ProjectIO.h"

#include <SDL3/SDL.h>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTableWidget>

namespace {
bool write(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray png(QRgb color) {
    QImage image(40, 32, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
QByteArray wav() {
    // 0.1 seconds of mono PCM silence, 8 kHz / 16 bit.
    QByteArray data = QByteArray::fromHex("524946466406000057415645666d74201000000001000100401f0000803e0000020010006461746140060000");
    data.append(QByteArray(1600, '\0'));
    return data;
}
}

TEST_CASE("에셋 가져오기·충돌·오류·재가져오기·Undo·이동·복구") {
    QTemporaryDir temp;
    CHECK(temp.isValid());
    auto root = temp.path();
    QDir().mkpath(root + "/a"); QDir().mkpath(root + "/b");
    const auto first = root + QStringLiteral("/a/그림.png");
    const auto second = root + QStringLiteral("/b/그림.png");
    const auto sound = root + "/tone.wav";
    CHECK(write(first, png(qRgba(20, 90, 190, 120))));
    CHECK(write(second, png(qRgba(220, 90, 20, 255))));
    CHECK(write(sound, wav()));
    QString error;
    auto doc = ProjectDocument::create(root + QStringLiteral("/프로젝트"), "assets", &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    AssetBrowser browser(doc.get());
    bool separateRegistration = false;
    QTimer::singleShot(0, &browser, [&] {
        auto* dialog = browser.findChild<QDialog*>(QStringLiteral("assetImportDialog"));
        separateRegistration = dialog && dialog->isVisible();
        if (dialog) dialog->reject();
    });
    browser.showImportDialog();
    CHECK(separateRegistration);
    CHECK(doc->project().assets.empty());
    CHECK(!doc->isModified());
    CHECK(browser.importFiles({}).isEmpty()); // cancelled picker
    CHECK(!doc->isModified());
    const auto importErrors = browser.importFiles({first, second, sound});
    for (const auto& e : importErrors) std::printf("Import error: %s\n", e.toUtf8().constData());
    CHECK(importErrors.isEmpty());
    CHECK(doc->project().assets.size() == 3);
    if (doc->project().assets.size() != 3) return;
    CHECK(doc->isModified());
    const auto original = doc->project().assets;
    CHECK(original[0].name == original[1].name);
    CHECK(original[0].id != original[1].id && original[0].path != original[1].path);
    CHECK(original[0].sha256 != original[1].sha256);
    CHECK(original[2].kind == "audio");
    CHECK(original[0].sourceName == "그림.png");
    const auto copied = QDir(doc->directory()).filePath(QString::fromStdString(original[0].path));
    CHECK(QImage(copied).pixelColor(0, 0).alpha() == 120);
    CHECK(doc->save(&error));
    CHECK(!doc->isModified());
    const auto exported = root + QStringLiteral("/내보낸 그림.png");
    CHECK(browser.exportFile(original[0].id, exported, &error));
    QFile exportedFile(exported);
    CHECK(exportedFile.open(QIODevice::ReadOnly));
    CHECK(exportedFile.readAll() == png(qRgba(20, 90, 190, 120)));
    exportedFile.close();
    CHECK(browser.exportFile(original[2].id, root + "/export.wav", &error));
    CHECK(!browser.exportFile(original[0].id, exported, &error));
    CHECK(browser.exportFile(original[1].id, exported, &error, true));
    CHECK(QImage(exported).pixelColor(0, 0) == QColor(220, 90, 20, 255));
    CHECK(!browser.exportFile(original[0].id, copied, &error, true));
    CHECK(!browser.exportFile(original[0].id, doc->directory() + "/project.json", &error, true));
    CHECK(!browser.exportFile(original[0].id, doc->directory() + "/new.png", &error));
    CHECK(!browser.exportFile(original[0].id, root + "/missing/export.png", &error));
    CHECK(!browser.exportFile(original[0].id, {}, &error));
    CHECK(doc->project().assets == original && !doc->isModified());
    doc->undoStack()->undo();
    CHECK(doc->project().assets.empty());
    doc->undoStack()->redo();
    CHECK(doc->project().assets == original && !doc->isModified());
    const auto broken = root + "/broken.png";
    const auto fakeSound = root + "/broken.wav";
    const auto fakeFont = root + "/broken.ttf";
    CHECK(write(broken, "bad image")); CHECK(write(fakeSound, "bad wav")); CHECK(write(fakeFont, "bad font"));
    CHECK(browser.importFiles({broken, fakeSound, fakeFont, root + "/missing.png"}).size() == 4);
    CHECK(doc->project().assets == original && !doc->isModified());
    CHECK(!browser.reimport(original[0].id, sound, &error));
    CHECK(doc->project().assets == original);
    CHECK(browser.reimport(original[0].id, second, &error));
    CHECK(doc->project().assets[0].id == original[0].id);
    CHECK(doc->project().assets[0].path != original[0].path);
    CHECK(QImage(copied).pixelColor(0, 0).alpha() == 120); // old copy untouched
    auto fromDisk = naru::ProjectIO::load(std::filesystem::path(doc->directory().toStdWString()));
    CHECK(fromDisk.ok() && fromDisk.project->assets == original); // unsaved reimport leaves saved assets intact
    doc->undoStack()->undo();
    CHECK(doc->project().assets == original);
    CHECK(browser.renameAsset(original[0].id, QStringLiteral("주인공 그림")));
    CHECK(doc->project().assets[0].id == original[0].id);
    CHECK(!browser.renameAsset(original[0].id, "  "));
    CHECK(browser.unregisterAsset(original[1].id));
    CHECK(doc->project().assets.size() == 2);
    doc->undoStack()->undo();
    CHECK(doc->project().assets.size() == 3);
    auto* search = browser.findChild<QLineEdit*>(QStringLiteral("assetSearch"));
    auto* list = browser.findChild<QListWidget*>(QStringLiteral("assetList"));
    search->setText(QStringLiteral("주인공")); CHECK(list->count() == 1);
    search->clear(); CHECK(list->count() == 3);
    // Exercise the actual audio stream with SDL's dummy device.
    list->setCurrentRow(2);
    auto* play = browser.findChild<QPushButton*>(QStringLiteral("assetPlay"));
    CHECK(play && play->isEnabled());
    if (play) play->click();
    CHECK(browser.findChild<QLabel*>(QStringLiteral("assetPlayback"))->text().contains(QStringLiteral("재생 중")));
    browser.stopAudio();
    CHECK(write(copied, "changed"));
    CHECK(!browser.exportFile(original[0].id, root + "/changed.png", &error));
    CHECK(!QFile::exists(root + "/changed.png"));
    CHECK(browser.assetState(doc->project().assets[0]) == QStringLiteral("파일 변경됨"));
    CHECK(QFile::remove(copied));
    CHECK(!browser.exportFile(original[0].id, exported, &error, true));
    CHECK(QImage(exported).pixelColor(0, 0) == QColor(220, 90, 20, 255));
    CHECK(browser.assetState(doc->project().assets[0]) == QStringLiteral("파일 없음"));
    CHECK(doc->save(&error));
    CHECK(naru::ProjectIO::load(std::filesystem::path(doc->directory().toStdWString())).ok());
    CHECK(browser.reimport(original[0].id, first, &error));
    CHECK(browser.assetState(doc->project().assets[0]) == QStringLiteral("정상"));
    CHECK(doc->save(&error));
    // Remove import sources and move the entire project: no external dependency remains.
    CHECK(QFile::remove(first)); CHECK(QFile::remove(second)); CHECK(QFile::remove(sound));
    CHECK(QDir().rename(doc->directory(), root + QStringLiteral("/옮긴 프로젝트")));
    auto moved = ProjectDocument::open(root + QStringLiteral("/옮긴 프로젝트"), &error);
    CHECK(moved && moved->project().assets == doc->project().assets);
    if (moved) {
        AssetBrowser movedBrowser(moved.get());
        for (const auto& a : moved->project().assets) CHECK(movedBrowser.assetState(a) == QStringLiteral("정상"));
    }
}

TEST_CASE("에셋 그룹의 추가·이름 변경·이동·삭제는 파일과 ID를 보존하고 Undo·저장된다") {
    QTemporaryDir temp;
    QString error;
    auto doc = ProjectDocument::create(temp.path() + "/project", "groups", &error);
    CHECK(doc != nullptr);
    if (!doc) return;
    AssetBrowser browser(doc.get());
    const auto source = temp.path() + "/sample.png";
    CHECK(write(source, png(qRgba(20, 90, 190, 120))));
    CHECK(browser.addGroup(QStringLiteral("캐릭터")));
    CHECK(!browser.addGroup(QStringLiteral(" 캐릭터 ")));
    CHECK(!browser.addGroup("  "));
    CHECK(browser.importFiles({source, source}).isEmpty());
    CHECK(doc->project().assets.size() == 2);
    if (doc->project().assets.size() != 2) return;
    const auto original = doc->project().assets;
    CHECK(original[0].group == "캐릭터");
    CHECK(browser.addGroup(QStringLiteral("배경")));
    auto* groups = browser.findChild<QComboBox*>(QStringLiteral("assetGroupFilter"));
    auto* list = browser.findChild<QListWidget*>(QStringLiteral("assetList"));
    CHECK(list->count() == 0); // newly added empty group
    CHECK(browser.moveToGroup({QString::fromStdString(original[0].id), QString::fromStdString(original[1].id)}, QStringLiteral("배경")));
    CHECK(list->count() == 2);
    CHECK(!browser.moveToGroup({QString::fromStdString(original[0].id)}, "missing"));
    CHECK(!browser.moveToGroup({"missing"}, QStringLiteral("배경")));
    CHECK(!browser.renameGroup(QStringLiteral("배경"), QStringLiteral("캐릭터")));
    CHECK(browser.renameGroup(QStringLiteral("배경"), QStringLiteral("장면 배경")));
    CHECK(doc->project().assets[0].group == "장면 배경");
    CHECK(doc->save(&error));
    const auto grouped = ProjectDocument::open(doc->directory(), &error);
    CHECK(grouped && grouped->project().assets == doc->project().assets);
    CHECK(browser.removeGroup(QStringLiteral("장면 배경")));
    CHECK(doc->project().assets[0].group.empty());
    CHECK(QFile::exists(QDir(doc->directory()).filePath(QString::fromStdString(original[0].path))));
    CHECK(doc->project().assets[0].id == original[0].id && doc->project().assets[0].path == original[0].path);
    doc->undoStack()->undo();
    CHECK(doc->project().assets[0].group == "장면 배경");
    doc->undoStack()->redo();
    CHECK(doc->project().assets[0].group.empty());
    groups->setCurrentIndex(1); // ungrouped
    CHECK(list->count() == 2);
    CHECK(browser.renameGroup(QStringLiteral("캐릭터"), QStringLiteral("빈 그룹")));
    CHECK(doc->save(&error));
    auto loaded = ProjectDocument::open(doc->directory(), &error);
    CHECK(loaded != nullptr);
    if (loaded) {
        CHECK(loaded->project().assetGroups == doc->project().assetGroups);
        CHECK(loaded->project().assets == doc->project().assets);
    }
}

TEST_CASE("에셋 ID 직접 입력·중복 사전 차단·ID 변경 참조/Undo·저장·스크립트 보존") {
    QTemporaryDir temp;
    QString error;
    auto doc = ProjectDocument::create(temp.path() + "/project", "ids", &error);
    CHECK(doc != nullptr); if (!doc) return;
    const auto source = temp.path() + "/source.png";
    CHECK(write(source, png(qRgba(20, 90, 190, 120))));
    AssetBrowser browser(doc.get());
    QTimer::singleShot(0, &browser, [&] {
        auto* dialog = browser.findChild<QDialog*>("assetImportDialog");
        auto* table = dialog->findChild<QTableWidget*>("assetImportFiles");
        auto* submit = dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
        CHECK(table->rowCount() == 2);
        table->item(0, 1)->setText("day"); table->item(1, 1)->setText("day");
        CHECK(!submit->isEnabled());
        table->item(1, 1)->setText(QStringLiteral("잘못된 ID")); CHECK(!submit->isEnabled());
        table->item(1, 1)->setText("night"); CHECK(submit->isEnabled()); submit->click();
    });
    browser.showImportDialog({source, source});
    CHECK(doc->project().assets.size() == 2); if (doc->project().assets.size() != 2) return;
    CHECK(doc->project().assets[0].id == "day" && doc->project().assets[1].id == "night");
    CHECK(doc->save(&error));
    const auto original = doc->project();
    const auto copies = QDir(doc->directory() + "/assets").entryList(QDir::Files);
    for (const auto& id : {QString("day"), QString("has space"), QString("../bad"), QStringLiteral("한글"), QString(65, 'x')}) {
        CHECK(!browser.importFiles({source}, {id}).isEmpty());
        CHECK(doc->project() == original && !doc->isModified());
    }
    CHECK(!browser.importFiles({source, source}, {"fresh", "fresh"}).isEmpty());
    CHECK(!browser.importFiles({source, source}, {"fresh"}).isEmpty());
    CHECK(QDir(doc->directory() + "/assets").entryList(QDir::Files) == copies);
    CHECK(browser.importFiles({source, source}, {"hero_idle", ""}).isEmpty());
    CHECK(doc->project().assets[2].id == "hero_idle" && doc->project().assets[3].id.starts_with("asset_"));
    auto scene = doc->project().scenes[0];
    scene.backgroundImage.asset = "day"; doc->setSceneInfo(scene);
    auto object = scene.objects[0]; object.image.asset = "day"; doc->setObject(scene.id, object);
    auto missing = scene.objects[1]; missing.image.asset = "day_new"; doc->setObject(scene.id, missing);
    auto second = *doc->project().findScene(scene.id); second.id = "second";
    for (auto& o : second.objects) o.id += "_second";
    if (!second.playerObjectId.empty()) second.playerObjectId += "_second";
    doc->insertScene(1, second);
    const auto script = doc->directory() + "/scripts/id_literal.py";
    const QByteArray literal = "def run(ctx):\n    ctx.scene.background_image = 'day'\n";
    CHECK(write(script, literal));
    CHECK(doc->save(&error));
    const auto beforeRename = doc->project();
    CHECK(!browser.renameAssetId("day", "night", &error));
    CHECK(!browser.renameAssetId("day", " ", &error));
    CHECK(!browser.renameAssetId("missing", "fresh", &error));
    CHECK(doc->project() == beforeRename && !doc->isModified());
    CHECK(browser.renameAssetId("day", "day_new", &error));
    for (const auto& s : doc->project().scenes) {
        CHECK(s.backgroundImage.asset == "day_new" && s.objects[0].image.asset == "day_new");
    }
    CHECK(doc->project().assets[0].path == beforeRename.assets[0].path);
    doc->undoStack()->undo(); CHECK(doc->project() == beforeRename && !doc->isModified());
    CHECK(doc->project().scenes[0].objects[1].image.asset == "day_new"); // preexisting missing reference stays untouched
    doc->undoStack()->redo();
    CHECK(doc->save(&error));
    const auto loaded = ProjectDocument::open(doc->directory(), &error);
    CHECK(loaded && loaded->project() == doc->project());
    QFile scriptFile(script); CHECK(scriptFile.open(QIODevice::ReadOnly)); CHECK(scriptFile.readAll() == literal);
    // The actual ID-change popup remains cancellable and accepts a new ID.
    auto* list = browser.findChild<QListWidget*>("assetList"); list->setCurrentRow(0);
    const auto beforeCancel = doc->project();
    QTimer::singleShot(0, &browser, [&] { browser.findChild<QInputDialog*>()->reject(); });
    browser.findChild<QPushButton*>("assetRenameId")->click();
    CHECK(doc->project() == beforeCancel);
    QTimer::singleShot(0, &browser, [&] { auto* dialog = browser.findChild<QInputDialog*>(); dialog->setTextValue("sky_day"); dialog->accept(); });
    browser.findChild<QPushButton*>("assetRenameId")->click();
    CHECK(doc->project().assets[0].id == "sky_day" && doc->project().scenes[0].backgroundImage.asset == "sky_day");
}

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    QApplication app(argc, argv);
    return naru::test::runAll();
}
