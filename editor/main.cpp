#include "CommandDialog.h"
#include "FieldDialog.h"
#include "GlobalScriptsDialog.h"
#include "MainWindow.h"
#include "ProjectDocument.h"
#include "StartDialog.h"
#include "EventConditionsDialog.h"
#include "VariablesDialog.h"

#include "naru/python/PythonRuntime.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>

#include <cstdio>
#include <memory>

namespace {

struct LaunchRequest {
    QString directory;
    QString newName; // 비어 있지 않으면 새 프로젝트를 만든다
};

std::unique_ptr<ProjectDocument> openOrCreate(const LaunchRequest& request, QString* error) {
    if (!request.newName.isEmpty()) {
        return ProjectDocument::create(request.directory, request.newName, error);
    }
    return ProjectDocument::open(request.directory, error);
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Naru"));
    QApplication::setApplicationName(QStringLiteral("Naru Studio"));
    QApplication::setApplicationVersion(QStringLiteral(NARU_STUDIO_VERSION));
    // Windows 11 스타일 메뉴는 페이드인 동안 반투명이라 글자가 흐리게 보인다. 바로 불투명하게 띄운다.
    QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Naru Studio 제작툴"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption newOption(QStringLiteral("new-project"),
                                 QStringLiteral("<dir>에 새 프로젝트를 만들어 연다. 이미 프로젝트가 있으면 연다."),
                                 QStringLiteral("dir"));
    QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                   QStringLiteral("창을 띄운 뒤 <ms> 후 종료한다 (자동 검사용)."), QStringLiteral("ms"));
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                        QStringLiteral("smoke-test 종료 직전 창을 <path>에 PNG로 저장한다."),
                                        QStringLiteral("path"));
    QCommandLineOption selfTestOption(QStringLiteral("self-test"),
                                      QStringLiteral("편집 흐름 자동 검사를 실행하고 결과를 종료 코드로 돌려준다."));
    QCommandLineOption eventOption(QStringLiteral("event"),
                                   QStringLiteral("시작 장면의 <objectId> 객체를 선택하고 이벤트 탭을 연다."),
                                   QStringLiteral("objectId"));
    // 사용 설명서(docs/guide) 스크린샷 촬영용 옵션
    QCommandLineOption runEventOption(QStringLiteral("run-event"),
                                      QStringLiteral("--event로 연 이벤트를 바로 테스트 실행한다."));
    QCommandLineOption startScreenOption(QStringLiteral("start-screen"),
                                         QStringLiteral("시작 화면을 띄워 --screenshot으로 저장하고 종료한다."));
    QCommandLineOption commandDialogOption(QStringLiteral("command-dialog"),
                                           QStringLiteral("<type> 명령 입력 창을 예시 값으로 띄워 --screenshot으로 저장하고 종료한다."),
                                           QStringLiteral("type"));
    QCommandLineOption databaseOption(QStringLiteral("database"),
                                      QStringLiteral("데이터베이스 탭을 열고 <type>[/<record>]를 선택한다."),
                                      QStringLiteral("type/record"));
    QCommandLineOption fieldsTabOption(QStringLiteral("fields-tab"),
                                       QStringLiteral("--database와 함께: '속성 정의' 탭을 연다."));
    QCommandLineOption globalScriptsOption(QStringLiteral("global-scripts-dialog"),
                                           QStringLiteral("프로젝트의 전역 스크립트 창을 띄워 --screenshot으로 저장하고 종료한다."));
    QCommandLineOption fieldDialogOption(QStringLiteral("field-dialog"),
                                         QStringLiteral("속성 정의 창을 예시 값으로 띄워 --screenshot으로 저장하고 종료한다."));
    QCommandLineOption variablesDialogOption(QStringLiteral("variables-dialog"),
                                             QStringLiteral("변수·스위치 창을 예시 값으로 띄워 --screenshot으로 저장하고 종료한다."));
    QCommandLineOption conditionsDialogOption(QStringLiteral("conditions-dialog"),
                                              QStringLiteral("이벤트 실행 조건 창을 예시 값으로 띄워 --screenshot으로 저장하고 종료한다."));
    QCommandLineOption lastOption(QStringLiteral("last"),
                                  QStringLiteral("마지막으로 연 프로젝트를 바로 연다 (없으면 시작 화면). 개발 중 빌드 후 실행용."));
    parser.addOptions({newOption, smokeOption, screenshotOption, selfTestOption, eventOption, runEventOption,
                       startScreenOption, commandDialogOption, databaseOption, fieldsTabOption, fieldDialogOption,
                       globalScriptsOption, variablesDialogOption, conditionsDialogOption, lastOption});
    parser.addPositionalArgument(QStringLiteral("project"), QStringLiteral("열 프로젝트 폴더"));
    parser.process(app);

    // 창 하나를 띄우고 잠시 뒤 PNG로 저장한 다음 종료한다.
    auto captureDialog = [&](QDialog& dialog) {
        QString path = parser.value(screenshotOption);
        QTimer::singleShot(800, &dialog, [&dialog, path] {
            if (!path.isEmpty()) {
                dialog.grab().save(path);
            }
            dialog.reject();
        });
        dialog.exec();
        return 0;
    };
    if (parser.isSet(startScreenOption)) {
        // 설명서 이미지에 실제 사용자의 최근 프로젝트 경로가 찍히지 않도록 빈 설정을 쓴다.
        QApplication::setOrganizationName(QStringLiteral("NaruGuideCapture"));
        StartDialog start;
        return captureDialog(start);
    }
    if (parser.isSet(commandDialogOption)) {
        auto type = naru::commandTypeFromKey(parser.value(commandDialogOption).toStdString());
        if (!type) {
            return 3;
        }
        naru::EventCommand sample;
        sample.type = *type;
        sample.speaker = "촌장";
        sample.text = "어서 오게, 여행자여.\n이 마을은 처음인가?";
        sample.variable = "visits";
        sample.value = std::int64_t{3};
        sample.compare = naru::CompareOp::GreaterEqual;
        sample.op = naru::VariableOp::Add;
        sample.seconds = 1.0;
        sample.script = "main.py";
        naru::Database db;
        db.types.push_back(naru::makeTemplate(naru::DataTemplate::Item, db));
        naru::DataType monsters = naru::makeTemplate(naru::DataTemplate::Monster, db);
        monsters.records = {{"slime", "슬라임", {}}};
        db.types.push_back(monsters);
        sample.dataType = "monster";
        sample.record = "slime";
        sample.field = "hp";
        if (*type == naru::EventCommandType::GetData) {
            sample.variable = "enemy_hp";
        } else if (*type == naru::EventCommandType::CallScript) {
            sample.script = "game.py";
            sample.function = "add_combo";
            sample.variable = "combo";
        }
        // 대상 객체 목록용 예시 장면
        naru::Scene scene;
        for (auto [id, name] : {std::pair{"obj_001", "주인공"}, std::pair{"obj_002", "문지기"},
                                std::pair{"obj_003", "비밀 문"}}) {
            naru::SceneObject o;
            o.id = id;
            o.name = name;
            scene.objects.push_back(o);
        }
        if (*type == naru::EventCommandType::MoveObject) {
            sample.target = "obj_002";
            sample.direction = "right";
            sample.distance = 96;
            sample.speed = 120;
        } else if (*type == naru::EventCommandType::SetVisible) {
            sample.target = "obj_003";
            sample.show = false;
        } else if (*type == naru::EventCommandType::ChangeScene) {
            sample.scene = "scene_002";
            sample.x = 600;
            sample.y = 640;
            sample.direction = "up";
        }
        naru::Project project;
        project.database = db;
        project.variables = {{"visits", std::int64_t{0}, "마을에 온 횟수"},
                             {"enemy_hp", std::int64_t{0}, "싸우는 적의 체력"},
                             {"combo", std::int64_t{0}, ""},
                             {"gate_open", false, "성문이 열렸는가"}};
        scene.id = "scene_001";
        scene.name = "마을";
        project.scenes.push_back(scene);
        naru::Scene inside;
        inside.id = "scene_002";
        inside.name = "여관 안";
        project.scenes.push_back(inside);
        CommandDialog dialog(sample, {QStringLiteral("game.py"), QStringLiteral("main.py")}, &project,
                             &project.scenes[0]);
        return captureDialog(dialog);
    }
    if (parser.isSet(variablesDialogOption)) {
        VariablesDialog dialog({{"box_opened", std::int64_t{0}, "상자를 연 횟수 (상자 이벤트에서 1씩 더함)"},
                                {"gold", std::int64_t{100}, "가진 돈"},
                                {"hero_name", std::string("아리"), "주인공 이름"},
                                {"inn_door_open", false, "여관 문이 열렸는가"}});
        return captureDialog(dialog);
    }
    if (parser.isSet(conditionsDialogOption)) {
        naru::Project project;
        project.variables = {{"inn_door_open", false, "여관 문이 열렸는가"}, {"gold", std::int64_t{100}, "가진 돈"}};
        EventConditionsDialog dialog({{"inn_door_open", naru::CompareOp::Equal, true},
                                      {"gold", naru::CompareOp::GreaterEqual, std::int64_t{50}}},
                                     &project);
        return captureDialog(dialog);
    }
    if (parser.isSet(fieldDialogOption)) {
        naru::Database db;
        db.types.push_back(naru::makeTemplate(naru::DataTemplate::Item, db));
        naru::DataType monsters = naru::makeTemplate(naru::DataTemplate::Monster, db);
        FieldDialog dialog(*monsters.findField("element"), monsters, db, false);
        return captureDialog(dialog);
    }

    std::unique_ptr<naru::python::PythonRuntime> python;
    try {
        python = std::make_unique<naru::python::PythonRuntime>();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, QStringLiteral("Naru Studio"),
                              QStringLiteral("Python 런타임을 시작하지 못했습니다.\n%1").arg(QString::fromUtf8(e.what())));
        return 2;
    }
    if (parser.isSet(globalScriptsOption)) {
        QString error;
        auto doc = parser.positionalArguments().isEmpty()
                       ? nullptr
                       : ProjectDocument::open(parser.positionalArguments().first(), &error);
        if (!doc) {
            return 3;
        }
        GlobalScriptsDialog dialog(doc->scriptFiles(), doc->project().globalScripts, doc->scriptsDirectory(),
                                   python.get());
        return captureDialog(dialog);
    }
    LaunchRequest request;
    if (parser.isSet(newOption)) {
        request.directory = parser.value(newOption);
        if (!QFileInfo(QDir(request.directory).filePath(QStringLiteral("project.json"))).isFile()) {
            request.newName = QDir(request.directory).dirName();
        }
    } else if (!parser.positionalArguments().isEmpty()) {
        request.directory = parser.positionalArguments().first();
    } else if (parser.isSet(lastOption)) {
        for (const QString& dir : StartDialog::recentProjects()) {
            if (QFileInfo(QDir(dir).filePath(QStringLiteral("project.json"))).isFile()) {
                request.directory = dir;
                break;
            }
        }
    }
    const bool automated = parser.isSet(smokeOption) || parser.isSet(selfTestOption);

    // 프로젝트 닫기를 고르면 시작 화면으로 돌아간다.
    while (true) {
        if (request.directory.isEmpty()) {
            if (automated) {
                return 3;
            }
            StartDialog start;
            if (start.exec() != QDialog::Accepted) {
                return 0;
            }
            request.directory = start.projectDirectory();
            request.newName = start.createRequested() ? start.newProjectName() : QString();
        }

        QString error;
        auto doc = openOrCreate(request, &error);
        if (!doc) {
            QString message = QStringLiteral("프로젝트를 열지 못했습니다.\n%1\n\n%2")
                                  .arg(QDir::toNativeSeparators(request.directory), error);
            if (automated) {
                std::fprintf(stderr, "%s\n", message.toUtf8().constData());
                return 3;
            }
            QMessageBox::warning(nullptr, QStringLiteral("Naru Studio"), message);
            request = {};
            continue;
        }
        if (!automated) {
            StartDialog::addRecentProject(doc->directory());
        }

        MainWindow window(doc.get(), python.get());
        window.show();
        if (parser.isSet(eventOption)) {
            window.showObjectEvent(parser.value(eventOption));
            if (parser.isSet(runEventOption)) {
                window.startEventTest();
            }
        }
        if (parser.isSet(databaseOption)) {
            QStringList parts = parser.value(databaseOption).split('/');
            window.showDatabase(parts.value(0), parts.value(1), parser.isSet(fieldsTabOption));
        }

        if (parser.isSet(selfTestOption)) {
            return window.runSelfTest() ? 0 : 1;
        }
        if (parser.isSet(smokeOption)) {
            int ms = parser.value(smokeOption).toInt();
            QString screenshot = parser.value(screenshotOption);
            if (!screenshot.isEmpty()) {
                // 스크린샷에는 개인 폴더 경로 대신 프로젝트 폴더 이름만 보인다.
                window.statusBar()->showMessage(QDir(doc->directory()).dirName());
            }
            QTimer::singleShot(ms > 0 ? ms : 1000, &app, [&window, screenshot] {
                if (!screenshot.isEmpty()) {
                    window.grab().save(screenshot);
                }
                QApplication::exit(0);
            });
            return app.exec();
        }

        app.exec();
        if (!window.returnToStart()) {
            return 0;
        }
        request = {};
    }
}
