#pragma once
#include <QWidget>
#include <QString>
class EventEditor;
class ProjectDocument;
class QListWidget;
namespace naru::python { class PythonRuntime; }

class CommonEventsEditor : public QWidget {
    Q_OBJECT
public:
    CommonEventsEditor(ProjectDocument* doc, naru::python::PythonRuntime* python, QWidget* parent = nullptr);
    EventEditor* editor() const { return editor_; }
    void selectEvent(const QString& id);
    QString addEvent(const QString& name);
    bool renameEvent(const QString& name);
    bool removeEvent();
    bool moveEvent(int delta);
private:
    void rebuild();
    QString selectedId() const;
    ProjectDocument* doc_;
    QListWidget* list_;
    EventEditor* editor_;
};
