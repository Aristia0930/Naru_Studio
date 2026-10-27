#pragma once

#include "naru/core/Log.h"
#include "naru/core/Value.h"
#include "naru/events/Command.h"
#include "naru/events/EventData.h"
#include "naru/project/Project.h"

#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

class ProjectDocument;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace naru {
class Session;
namespace python {
class PythonRuntime;
}
} // namespace naru

// RPG Maker식 이벤트 편집기: 선택한 객체의 실행 조건과 명령 목록(조건 분기 중첩)을 편집하고 테스트 실행한다.
// 대사는 아래 미리보기 창에 표시하고 "다음"으로 넘긴다.
class EventEditor : public QWidget, public naru::DialogueHost {
    Q_OBJECT

public:
    EventEditor(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent = nullptr);
    ~EventEditor() override;

    void setTarget(const QString& sceneId, const QString& objectId);
    void setLogSink(std::function<void(const naru::LogEntry&)> sink) { logSink_ = std::move(sink); }
    void setVariablesSink(std::function<void(const naru::Variables&)> sink) { variablesSink_ = std::move(sink); }

    // 편집 조작. listPrefix는 명령 목록 위치("" 최상위, "<분기 ID>/t", "<분기 ID>/e").
    bool insertCommand(const std::string& listPrefix, int index, const naru::EventCommand& command);
    bool replaceCommand(const std::string& listPrefix, int index, const naru::EventCommand& command);
    bool removeCommand(const std::string& listPrefix, int index);
    bool moveCommand(const std::string& listPrefix, int index, int delta);

    // 테스트 실행
    void startTest();
    void stopTest();
    bool isRunning() const { return session_ != nullptr; }
    void advanceDialogue();

    // DialogueHost
    void showMessage(const std::string& speaker, const std::string& text) override;
    bool messageClosed() const override { return !messageOpen_; }
    void closeMessage() override;

private:
    void rebuild();
    void addCommandAt(const std::string& listPrefix, int index);
    void editSelected();
    void addAtSelection();
    void removeSelected();
    void moveSelected(int delta);
    void tick();
    void finishTest();
    void highlightCommand(const std::string& id);
    QTreeWidgetItem* findCommandItem(const std::string& id) const;
    bool selectedPosition(std::string& listPrefix, int& index) const;
    bool editEvent(const std::function<bool(naru::ObjectEvent&)>& change);
    const naru::SceneObject* target() const;
    QStringList scriptNames() const;

    ProjectDocument* doc_;
    naru::python::PythonRuntime* python_;
    QString sceneId_;
    QString objectId_;

    QLabel* title_;
    QComboBox* trigger_;
    QTreeWidget* tree_;
    QPushButton* runButton_;
    QPushButton* stopButton_;
    QFrame* dialogueBox_;
    QLabel* dialogueSpeaker_;
    QLabel* dialogueText_;
    bool loading_ = false;

    naru::Database runDatabase_; // 테스트 실행 중 세션이 읽는 사본 (session_보다 먼저 선언해야 나중에 파괴된다)
    std::unique_ptr<naru::Session> session_;
    QTimer timer_;
    QElapsedTimer clock_;
    bool messageOpen_ = false;
    std::string highlighted_;
    std::function<void(const naru::LogEntry&)> logSink_;
    std::function<void(const naru::Variables&)> variablesSink_;
};
