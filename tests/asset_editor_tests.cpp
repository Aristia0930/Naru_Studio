#include "TestMain.h"
#include "AssetBrowser.h"
#include "ProjectDocument.h"
#include "naru/project/ProjectIO.h"

#include <SDL3/SDL.h>
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

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
    CHECK(browser.assetState(doc->project().assets[0]) == QStringLiteral("파일 변경됨"));
    CHECK(QFile::remove(copied));
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

int main(int argc, char** argv) {
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    QApplication app(argc, argv);
    return naru::test::runAll();
}
