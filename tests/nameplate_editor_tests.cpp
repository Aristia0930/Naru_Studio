#include "TestMain.h"
#include "Inspector.h"
#include "ProjectDocument.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

TEST_CASE("게임 이름표 인스펙터·개별 설정·이름 변경·Undo·저장 호환") {
    QTemporaryDir temp; QString error;
    auto doc=ProjectDocument::create(temp.path()+"/project","names",&error);
    CHECK(doc != nullptr); if (!doc) return;
    const auto scene=doc->project().scenes[0];
    const auto object=scene.objects[0];
    Inspector inspector(doc.get()); inspector.setTarget(QString::fromStdString(scene.id),QString::fromStdString(object.id));
    auto* show=inspector.findChild<QCheckBox*>("objectShowName");
    auto* text=inspector.findChild<QLineEdit*>("objectNameText");
    CHECK(show && text); if (!show || !text) return;
    CHECK(!show->isChecked() && text->text().isEmpty());
    show->click();
    CHECK(doc->project().scenes[0].objects[0].showName);
    text->setText(QStringLiteral("여관 주인")); QMetaObject::invokeMethod(text,"editingFinished");
    CHECK(doc->project().scenes[0].objects[0].nameText=="여관 주인");
    CHECK(doc->project().scenes[0].objects[0].name==object.name);
    doc->undoStack()->undo(); CHECK(doc->project().scenes[0].objects[0].nameText.empty());
    doc->undoStack()->undo(); CHECK(!doc->project().scenes[0].objects[0].showName);
    doc->undoStack()->redo(); doc->undoStack()->redo();
    CHECK(doc->save(&error)); const auto saved=doc->project();
    auto reopened=ProjectDocument::open(doc->directory(),&error); CHECK(reopened != nullptr);
    if (reopened) CHECK(reopened->project()==saved);
}

TEST_CASE("이름표 배경 투명·색 선택 취소/적용·위아래·Undo·저장") {
    QTemporaryDir temp; QString error;
    auto doc=ProjectDocument::create(temp.path()+"/project","style",&error);
    CHECK(doc != nullptr); if (!doc) return;
    const auto scene=doc->project().scenes[0]; const auto id=scene.objects[0].id;
    Inspector inspector(doc.get()); inspector.setTarget(QString::fromStdString(scene.id),QString::fromStdString(id));
    auto* transparent=inspector.findChild<QCheckBox*>("objectNameTransparent");
    auto* color=inspector.findChild<QPushButton*>("objectNameBackgroundColor");
    auto* position=inspector.findChild<QComboBox*>("objectNamePosition");
    CHECK(transparent && color && position); if (!transparent || !color || !position) return;
    CHECK(!transparent->isChecked() && color->isEnabled() && position->currentData()=="above");
    const auto before=doc->project();
    QTimer::singleShot(0,&inspector,[&] { auto* dialog=inspector.findChild<QColorDialog*>("nameBackgroundColorDialog"); CHECK(dialog); if (dialog) dialog->reject(); });
    color->click(); CHECK(doc->project()==before);
    QTimer::singleShot(0,&inspector,[&] { auto* dialog=inspector.findChild<QColorDialog*>("nameBackgroundColorDialog"); CHECK(dialog); if (dialog) { dialog->setCurrentColor(QColor("#2468ac")); dialog->accept(); } });
    color->click(); CHECK(doc->project().scenes[0].objects[0].nameBackgroundColor=="#2468ac");
    transparent->click(); CHECK(!color->isEnabled());
    CHECK(doc->project().scenes[0].objects[0].nameBackgroundTransparent);
    CHECK(doc->project().scenes[0].objects[0].nameBackgroundColor=="#2468ac");
    position->setCurrentIndex(position->findData("below")); CHECK(doc->project().scenes[0].objects[0].namePosition=="below");
    doc->undoStack()->undo(); CHECK(position->currentData()=="above");
    doc->undoStack()->undo(); CHECK(!transparent->isChecked() && color->isEnabled());
    doc->undoStack()->undo(); CHECK(doc->project()==before);
    doc->undoStack()->redo(); doc->undoStack()->redo(); doc->undoStack()->redo();
    CHECK(doc->save(&error)); auto loaded=ProjectDocument::open(doc->directory(),&error); CHECK(loaded != nullptr);
    if (loaded) CHECK(loaded->project()==doc->project());
    transparent->click(); CHECK(doc->project().scenes[0].objects[0].nameBackgroundColor=="#2468ac");
}
