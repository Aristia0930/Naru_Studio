#include "MainWindow.h"

#include "naru/python/PythonRuntime.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QTimer>

#include <memory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Naru Studio"));
    QApplication::setApplicationVersion(QStringLiteral(NARU_STUDIO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Naru Studio 제작툴"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                   QStringLiteral("창을 띄운 뒤 <ms> 후 종료한다 (자동 검사용)."),
                                   QStringLiteral("ms"));
    QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                        QStringLiteral("smoke-test 종료 직전 창을 <path>에 PNG로 저장한다."),
                                        QStringLiteral("path"));
    QCommandLineOption runOption(QStringLiteral("run"), QStringLiteral("열린 스크립트를 시작하자마자 실행한다."));
    parser.addOption(smokeOption);
    parser.addOption(screenshotOption);
    parser.addOption(runOption);
    parser.addPositionalArgument(QStringLiteral("script"), QStringLiteral("열 Python 스크립트"));
    parser.process(app);

    std::unique_ptr<naru::python::PythonRuntime> python;
    try {
        python = std::make_unique<naru::python::PythonRuntime>();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, QStringLiteral("Naru Studio"),
                              QStringLiteral("Python 런타임을 시작하지 못했습니다.\n%1").arg(QString::fromUtf8(e.what())));
        return 2;
    }

    MainWindow window(python.get());
    if (!parser.positionalArguments().isEmpty()) {
        window.openScript(parser.positionalArguments().first());
        if (parser.isSet(runOption)) {
            QTimer::singleShot(0, &window, [&window] {
                QMetaObject::invokeMethod(&window, "runScript");
            });
        }
    }
    window.show();

    if (parser.isSet(smokeOption)) {
        int ms = parser.value(smokeOption).toInt();
        QString screenshot = parser.value(screenshotOption);
        QTimer::singleShot(ms > 0 ? ms : 1000, &app, [&window, screenshot] {
            if (!screenshot.isEmpty()) {
                window.grab().save(screenshot);
            }
            QApplication::exit(0);
        });
    }

    int code = app.exec();
    return code;
}
