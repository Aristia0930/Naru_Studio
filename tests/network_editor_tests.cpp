#include "TestMain.h"
#include "NetworkSettingsDialog.h"
#include "NetworkFields.h"
#include "CommandDialog.h"
#include "ProjectDocument.h"
#include "UndoCommands.h"
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TEST_CASE("네트워크 접속 설정·명령 입력·취소·Undo·저장 재열기") {
    QTemporaryDir temp; QString error;
    auto doc = ProjectDocument::create(temp.path()+"/network", "network", &error);
    CHECK(doc != nullptr); if (!doc) return;
    const auto original = doc->project();
    NetworkSettingsDialog settings(doc->project());
    settings.findChild<QPushButton*>("networkAdd")->click();
    settings.findChild<QLineEdit*>("networkConnectionId")->setText("local");
    settings.findChild<QLineEdit*>("networkBaseUrl")->setText("http://127.0.0.1:8765");
    settings.findChild<QDoubleSpinBox*>("networkTimeout")->setValue(2);
    settings.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    CHECK(settings.result() == QDialog::Accepted);
    doc->undoStack()->push(new SetNetworkConnectionsCommand(doc.get(), settings.connections()));
    CHECK(doc->isModified()); doc->undoStack()->undo(); CHECK(doc->project() == original);
    doc->undoStack()->redo(); CHECK(doc->project().networkConnections[0].timeoutMs == 2000);
    naru::EventCommand c; c.type = naru::EventCommandType::NetworkRequest;
    CommandDialog command(c, {}, &doc->project());
    command.findChild<QLineEdit*>("networkPath")->setText("/hello");
    static_cast<NetworkFields*>(command.findChild<QWidget*>("networkMappings"))->append("/message", "notice");
    auto* mappings = static_cast<NetworkFields*>(command.findChild<QWidget*>("networkMappings"));
    const auto beforeHelp = mappings->values();
    bool helpShown = false;
    QTimer::singleShot(0, [&] {
        auto* help = QApplication::activeModalWidget();
        auto* example = help ? help->findChild<QPlainTextEdit*>("networkMappingExample") : nullptr;
        helpShown = help && help->objectName() == "networkMappingHelp" && example && example->isReadOnly() &&
                    example->toPlainText().contains("/message") && example->toPlainText().contains("공지내용");
        if (auto* dialog = qobject_cast<QDialog*>(help)) dialog->reject();
    });
    command.findChild<QPushButton*>("networkMappingHelpButton")->click();
    CHECK(helpShown && mappings->values() == beforeHelp && command.findChild<QLineEdit*>("networkPath")->text() == "/hello");
    command.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    CHECK(command.result() == QDialog::Accepted);
    c = command.command(); CHECK(c.network.connection == "local" && c.network.mappings.at("/message") == "notice");
    CommandDialog cancel(c, {}, &doc->project());
    cancel.findChild<QLineEdit*>("networkPath")->setText("/other"); cancel.reject(); CHECK(cancel.command() == c);
    auto object = doc->project().scenes[0].objects[0]; object.event.commands = {c};
    doc->setObject(doc->project().scenes[0].id, object);
    CHECK(doc->save(&error));
    const auto loaded = ProjectDocument::open(doc->directory(), &error);
    CHECK(loaded != nullptr); if (loaded) CHECK(loaded->project() == doc->project());
    NetworkSettingsDialog protectedSettings(doc->project());
    protectedSettings.findChild<QLineEdit*>("networkConnectionId")->setText("renamed");
    QTimer::singleShot(0, [] { if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) message->accept(); });
    protectedSettings.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    CHECK(protectedSettings.result() != QDialog::Accepted);
    QFile manifest(doc->directory()+"/project.json");
    CHECK(manifest.open(QIODevice::ReadOnly));
    auto json = QJsonDocument::fromJson(manifest.readAll()).object(); manifest.close();
    auto connections = json["networkConnections"].toArray(); auto first = connections[0].toObject();
    first["timeoutMs"] = 4294977296.0; connections[0] = first; json["networkConnections"] = connections;
    CHECK(manifest.open(QIODevice::WriteOnly)); manifest.write(QJsonDocument(json).toJson()); manifest.close();
    CHECK(ProjectDocument::open(doc->directory(), &error) == nullptr);
}
