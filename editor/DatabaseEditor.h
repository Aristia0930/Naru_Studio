#pragma once

#include "naru/data/Database.h"

#include <QString>
#include <QWidget>

#include <functional>

class ProjectDocument;
class QLabel;
class QLineEdit;
class QListWidget;
class QScrollArea;
class QTableWidget;
class QTabWidget;

// 데이터베이스 편집: 데이터 종류 → 항목 → (값 편집 | 속성 정의) 3단 화면.
// 모든 변경은 SetDatabaseCommand로 기록되어 Ctrl+Z로 되돌릴 수 있다.
class DatabaseEditor : public QWidget {
    Q_OBJECT

public:
    explicit DatabaseEditor(ProjectDocument* doc, QWidget* parent = nullptr);

    // 편집 조작 (화면 버튼과 자동 검사가 같은 경로를 쓴다)
    bool addType(const naru::DataType& type);
    bool addRecord(const QString& typeKey, const naru::DataRecord& record);
    bool setRecordValue(const QString& typeKey, const QString& recordId, const QString& fieldKey,
                        const naru::Value& value);
    bool setField(const QString& typeKey, int index, const naru::FieldDef& field, bool askBeforeReset);
    void select(const QString& typeKey, const QString& recordId = {});
    void showFieldsTab();

    QString currentTypeKey() const { return typeKey_; }
    QString currentRecordId() const { return recordId_; }

private:
    void rebuildAll();
    void rebuildTypes();
    void rebuildRecords();
    void rebuildForm();
    void rebuildFields();
    void updateIssues();

    void addTypeFromTemplate(naru::DataTemplate kind);
    void renameType();
    void removeType();
    void addRecordInteractive();
    void duplicateRecord();
    void removeRecord();
    void renameRecord(const QString& name);
    void addFieldInteractive();
    void editFieldInteractive();
    void removeField();
    void moveField(int delta);

    bool apply(const std::function<bool(naru::Database&)>& change, const QString& text, const QString& mergeKey = {});
    const naru::DataType* currentType() const;
    const naru::DataRecord* currentRecord() const;

    ProjectDocument* doc_;
    QString typeKey_;
    QString recordId_;
    bool committing_ = false;
    bool rebuilding_ = false;

    QListWidget* types_;
    QLineEdit* filter_;
    QListWidget* records_;
    QTabWidget* detailTabs_;
    QScrollArea* formArea_;
    QTableWidget* fields_;
    QLabel* issues_;
};
