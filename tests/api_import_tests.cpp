#include "TestMain.h"
#include "DatabaseEditor.h"
#include "ProjectDocument.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>

TEST_CASE("API 가져오기 실제 조회·충돌·미리보기·적용·Undo·저장 재열기") {
    QProcess server;
    server.start(QString::fromUtf8(NARU_TEST_PYTHON), {QString::fromUtf8(NARU_IMPORT_SERVER), "0"});
    CHECK(server.waitForStarted()); CHECK(server.waitForReadyRead(5000));
    const auto port = QString::fromUtf8(server.readLine()).trimmed();
    CHECK(!port.isEmpty());
    QTemporaryDir temporary; QString error;
    auto doc = ProjectDocument::create(temporary.path()+"/project", "API import", &error);
    CHECK(doc != nullptr); if (!doc) return;
    doc->setNetworkConnections({{"test", "http://127.0.0.1:"+port.toStdString(), {}, 3000}});
    naru::DataType type; type.key="goods"; type.name="상품";
    naru::FieldDef price; price.key="price"; price.name="가격"; price.type=naru::FieldType::Integer;
    price.defaultValue=std::int64_t{0}; type.fields={price};
    type.records={{"potion","기존 약",{{"price",std::int64_t{50}}}}};
    DatabaseEditor editor(doc.get()); CHECK(editor.addType(type)); editor.select("goods");
    const auto original=doc->project().database;
    int stage=0, ticks=0;
    QTimer timer;
    QObject::connect(&timer,&QTimer::timeout,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        if (++ticks>150) { CHECK(false); dialog->reject(); return; }
        auto* fetch=dialog->findChild<QPushButton*>("apiImportFetch");
        auto* preview=dialog->findChild<QPushButton*>("apiImportPreview");
        auto* apply=dialog->findChild<QPushButton*>("apiImportApply");
        auto* report=dialog->findChild<QPlainTextEdit*>("apiImportReport");
        if (stage==0) { fetch->click(); stage=1; return; }
        if (stage==1 && report->toPlainText().contains(QStringLiteral("조회 완료"))) {
            preview->click(); CHECK(!apply->isEnabled());
            CHECK(report->toPlainText().contains(QStringLiteral("기존 ID와 충돌")));
            CHECK(doc->project().database==original);
            dialog->findChild<QCheckBox*>("apiImportUpdate")->setChecked(true);
            preview->click(); CHECK(apply->isEnabled());
            CHECK(report->toPlainText().contains("120"));
            const auto screenshot=qEnvironmentVariable("NARU_IMPORT_SCREENSHOT");
            if (!screenshot.isEmpty()) CHECK(dialog->grab().save(screenshot));
            apply->click(); stage=2;
        }
    });
    timer.start(50); editor.importApiData(); timer.stop(); CHECK(stage==2);
    const auto imported=doc->project().database;
    CHECK(imported.findType("goods")->records.size()==2);
    CHECK(imported.findType("goods")->findRecord("potion")->name=="회복약");
    doc->undoStack()->undo(); CHECK(doc->project().database==original);
    doc->undoStack()->redo(); CHECK(doc->project().database==imported);
    CHECK(doc->save(&error)); auto loaded=ProjectDocument::open(doc->directory(),&error);
    CHECK(loaded && loaded->project().database==imported);
    for (const QString route : {QStringLiteral("/bad"), QStringLiteral("/missing-field"), QStringLiteral("/duplicate"), QStringLiteral("/missing"), QStringLiteral("/slow")}) {
        int attempt=0, count=0; bool checked=false;
        QTimer failureTimer;
        QObject::connect(&failureTimer,&QTimer::timeout,[&] {
            auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()); if (!dialog) return;
            if (++count>150) { CHECK(false); dialog->reject(); return; }
            auto* report=dialog->findChild<QPlainTextEdit*>("apiImportReport");
            if (attempt==0) {
                dialog->findChild<QCheckBox*>("apiImportUpdate")->setChecked(true);
                dialog->findChild<QLineEdit*>("apiImportPath")->setText(route);
                dialog->findChild<QPushButton*>("apiImportFetch")->click(); attempt=1;
                if (route=="/slow") { checked=true; dialog->reject(); }
                return;
            }
            if (report->toPlainText().contains(QStringLiteral("조회 완료"))) {
                dialog->findChild<QPushButton*>("apiImportPreview")->click();
                CHECK(!dialog->findChild<QPushButton*>("apiImportApply")->isEnabled());
                CHECK(report->toPlainText().contains(QStringLiteral("항목 2 [ether]")) || route=="/duplicate");
                if(route=="/bad") CHECK(report->toPlainText().contains(QStringLiteral("DB 'price'")));
                if(route=="/missing-field") CHECK(report->toPlainText().contains(QStringLiteral("누락")));
                if(route=="/bad") {const auto shot=qEnvironmentVariable("NARU_IMPORT_ERROR_SCREENSHOT");if(!shot.isEmpty())CHECK(dialog->grab().save(shot));}
                if(route=="/duplicate") CHECK(report->toPlainText().contains(QStringLiteral("중복 ID")));
                checked=true; dialog->reject();
            } else if (report->toPlainText().contains(QStringLiteral("조회 실패"))) {
                CHECK(route=="/missing"); checked=true; dialog->reject();
            }
        });
        failureTimer.start(50); editor.importApiData(); failureTimer.stop();
        CHECK(checked); CHECK(doc->project().database==imported);
    }
    server.kill(); server.waitForFinished();
}
