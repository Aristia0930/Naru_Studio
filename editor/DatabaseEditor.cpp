#include "DatabaseEditor.h"

#include "FieldDialog.h"
#include "ProjectDocument.h"
#include "UndoCommands.h"
#include "ValueEditor.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using naru::DataRecord;
using naru::DataType;
using naru::FieldDef;
using naru::FieldType;

namespace {

QString q(const std::string& s) {
    return QString::fromStdString(s);
}

QString displayValue(const naru::Value& v, const FieldDef& f, const naru::Database& db) {
    if (f.type == FieldType::Boolean) {
        if (const auto* b = std::get_if<bool>(&v)) {
            return *b ? QStringLiteral("참") : QStringLiteral("거짓");
        }
    }
    if (f.type == FieldType::Reference) {
        const auto* id = std::get_if<std::string>(&v);
        if (!id) {
            return QStringLiteral("(없음)");
        }
        const DataType* target = db.findType(f.refType);
        const DataRecord* r = target ? target->findRecord(*id) : nullptr;
        return r ? QStringLiteral("%1 (%2)").arg(q(r->name), q(*id)) : q(*id);
    }
    return q(naru::toDisplayString(v));
}

QString fieldSettings(const FieldDef& f, const naru::Database& db) {
    auto num = [](double v) { return v == std::floor(v) ? QString::number(static_cast<long long>(v)) : QString::number(v); };
    switch (f.type) {
    case FieldType::Integer:
    case FieldType::Number:
        if (f.min && f.max) return QStringLiteral("%1 ~ %2").arg(num(*f.min), num(*f.max));
        if (f.min) return QStringLiteral("%1 이상").arg(num(*f.min));
        if (f.max) return QStringLiteral("%1 이하").arg(num(*f.max));
        return {};
    case FieldType::Choice: {
        QStringList list;
        for (const auto& c : f.choices) list << q(c);
        return list.join(QStringLiteral(" / "));
    }
    case FieldType::Reference: {
        const DataType* t = db.findType(f.refType);
        return QStringLiteral("→ %1").arg(t ? q(t->name) : QStringLiteral("⚠ 없음 (%1)").arg(q(f.refType)));
    }
    default: return {};
    }
}

// 표시 이름과 영문 키(ID)를 함께 입력받는 작은 대화상자.
bool askNameAndKey(QWidget* parent, const QString& title, const QString& keyLabel, QString& name, QString& key,
                   const std::function<QString(const QString&)>& validateKey) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* nameEdit = new QLineEdit(name, &dialog);
    auto* keyEdit = new QLineEdit(key, &dialog);
    keyEdit->setToolTip(QStringLiteral("스크립트와 이벤트에서 찾을 때 쓰는 이름입니다. 영문·숫자·_·- 로 씁니다. 만든 뒤에는 바꿀 수 없습니다."));
    auto* hint = new QLabel(QStringLiteral("이름은 언제든 바꿀 수 있지만 ID는 바꿀 수 없습니다."), &dialog);
    hint->setStyleSheet(QStringLiteral("color: gray"));
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("이름"), nameEdit);
    form->addRow(keyLabel, keyEdit);
    form->addRow(hint);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, title, QStringLiteral("이름을 입력하세요."));
            return;
        }
        QString error = validateKey(keyEdit->text().trimmed());
        if (!error.isEmpty()) {
            QMessageBox::warning(&dialog, title, error);
            return;
        }
        dialog.accept();
    });
    dialog.resize(420, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    name = nameEdit->text().trimmed();
    key = keyEdit->text().trimmed();
    return true;
}

QPushButton* smallButton(const QString& text, QWidget* parent) {
    auto* b = new QPushButton(text, parent);
    b->setAutoDefault(false);
    return b;
}

} // namespace

DatabaseEditor::DatabaseEditor(ProjectDocument* doc, QWidget* parent) : QWidget(parent), doc_(doc) {
    // 1단: 데이터 종류
    types_ = new QListWidget(this);
    auto* addType = new QToolButton(this);
    addType->setText(QStringLiteral("종류 추가 ▼"));
    addType->setPopupMode(QToolButton::InstantPopup);
    auto* templates = new QMenu(addType);
    templates->addAction(QStringLiteral("빈 데이터 종류"), this, [this] { addTypeFromTemplate(naru::DataTemplate::Empty); });
    templates->addSeparator();
    templates->addAction(QStringLiteral("템플릿: 캐릭터"), this, [this] { addTypeFromTemplate(naru::DataTemplate::Character); });
    templates->addAction(QStringLiteral("템플릿: 아이템"), this, [this] { addTypeFromTemplate(naru::DataTemplate::Item); });
    templates->addAction(QStringLiteral("템플릿: 몬스터"), this, [this] { addTypeFromTemplate(naru::DataTemplate::Monster); });
    addType->setMenu(templates);
    auto* renameTypeButton = smallButton(QStringLiteral("이름 변경"), this);
    auto* removeTypeButton = smallButton(QStringLiteral("삭제"), this);
    auto* typePanel = new QWidget(this);
    auto* typeLayout = new QVBoxLayout(typePanel);
    typeLayout->addWidget(new QLabel(QStringLiteral("<b>데이터 종류</b>"), this));
    typeLayout->addWidget(types_, 1);
    auto* typeButtons = new QHBoxLayout;
    typeButtons->addWidget(addType);
    typeButtons->addWidget(renameTypeButton);
    typeButtons->addWidget(removeTypeButton);
    typeLayout->addLayout(typeButtons);

    // 2단: 항목
    filter_ = new QLineEdit(this);
    filter_->setPlaceholderText(QStringLiteral("검색 (이름·ID)"));
    filter_->setClearButtonEnabled(true);
    records_ = new QListWidget(this);
    auto* addRecordButton = smallButton(QStringLiteral("추가"), this);
    auto* duplicateButton = smallButton(QStringLiteral("복제"), this);
    auto* removeRecordButton = smallButton(QStringLiteral("삭제"), this);
    auto* recordPanel = new QWidget(this);
    auto* recordLayout = new QVBoxLayout(recordPanel);
    recordLayout->addWidget(new QLabel(QStringLiteral("<b>항목</b>"), this));
    recordLayout->addWidget(filter_);
    recordLayout->addWidget(records_, 1);
    auto* recordButtons = new QHBoxLayout;
    recordButtons->addWidget(addRecordButton);
    recordButtons->addWidget(duplicateButton);
    recordButtons->addWidget(removeRecordButton);
    recordLayout->addLayout(recordButtons);

    // 3단: 값 편집 / 속성 정의
    formArea_ = new QScrollArea(this);
    formArea_->setWidgetResizable(true);
    fields_ = new QTableWidget(0, 5, this);
    fields_->setHorizontalHeaderLabels({QStringLiteral("표시 이름"), QStringLiteral("키"), QStringLiteral("형식"),
                                        QStringLiteral("기본값"), QStringLiteral("설정")});
    fields_->horizontalHeader()->setStretchLastSection(true);
    fields_->verticalHeader()->hide();
    fields_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fields_->setSelectionMode(QAbstractItemView::SingleSelection);
    fields_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto* addFieldButton = smallButton(QStringLiteral("속성 추가"), this);
    auto* editFieldButton = smallButton(QStringLiteral("편집"), this);
    auto* removeFieldButton = smallButton(QStringLiteral("삭제"), this);
    auto* upButton = smallButton(QStringLiteral("위로"), this);
    auto* downButton = smallButton(QStringLiteral("아래로"), this);
    auto* fieldPage = new QWidget(this);
    auto* fieldLayout = new QVBoxLayout(fieldPage);
    auto* fieldHint = new QLabel(QStringLiteral("이 데이터 종류의 모든 항목이 가지는 속성입니다. 형식을 바꾸면 기존 값은 가능한 만큼 변환됩니다."), this);
    fieldHint->setWordWrap(true);
    fieldHint->setStyleSheet(QStringLiteral("color: gray"));
    fieldLayout->addWidget(fieldHint);
    fieldLayout->addWidget(fields_, 1);
    auto* fieldButtons = new QHBoxLayout;
    for (QPushButton* b : {addFieldButton, editFieldButton, removeFieldButton, upButton, downButton}) {
        fieldButtons->addWidget(b);
    }
    fieldButtons->addStretch();
    fieldLayout->addLayout(fieldButtons);

    detailTabs_ = new QTabWidget(this);
    detailTabs_->addTab(formArea_, QStringLiteral("값 편집"));
    detailTabs_->addTab(fieldPage, QStringLiteral("속성 정의"));

    auto* splitter = new QSplitter(this);
    splitter->addWidget(typePanel);
    splitter->addWidget(recordPanel);
    splitter->addWidget(detailTabs_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 3);
    splitter->setSizes({200, 220, 600});

    issues_ = new QLabel(this);
    issues_->setStyleSheet(QStringLiteral("color: #b06000"));
    issues_->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(splitter, 1);
    layout->addWidget(issues_);

    connect(types_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (rebuilding_ || !item) {
            return;
        }
        typeKey_ = item->data(Qt::UserRole).toString();
        recordId_.clear();
        rebuildRecords();
        rebuildForm();
        rebuildFields();
    });
    connect(records_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (rebuilding_ || !item) {
            return;
        }
        recordId_ = item->data(Qt::UserRole).toString();
        rebuildForm();
    });
    connect(filter_, &QLineEdit::textChanged, this, [this] { rebuildRecords(); });
    connect(fields_, &QTableWidget::cellDoubleClicked, this, [this] { editFieldInteractive(); });
    connect(renameTypeButton, &QPushButton::clicked, this, &DatabaseEditor::renameType);
    connect(removeTypeButton, &QPushButton::clicked, this, &DatabaseEditor::removeType);
    connect(addRecordButton, &QPushButton::clicked, this, &DatabaseEditor::addRecordInteractive);
    connect(duplicateButton, &QPushButton::clicked, this, &DatabaseEditor::duplicateRecord);
    connect(removeRecordButton, &QPushButton::clicked, this, &DatabaseEditor::removeRecord);
    connect(addFieldButton, &QPushButton::clicked, this, &DatabaseEditor::addFieldInteractive);
    connect(editFieldButton, &QPushButton::clicked, this, &DatabaseEditor::editFieldInteractive);
    connect(removeFieldButton, &QPushButton::clicked, this, &DatabaseEditor::removeField);
    connect(upButton, &QPushButton::clicked, this, [this] { moveField(-1); });
    connect(downButton, &QPushButton::clicked, this, [this] { moveField(1); });

    connect(doc_, &ProjectDocument::databaseChanged, this, [this] {
        if (committing_) {
            // 값 입력 중에는 입력 칸을 다시 만들지 않는다 (포커스·커서 유지). 목록과 표만 갱신한다.
            rebuildTypes();
            rebuildRecords();
            rebuildFields();
            updateIssues();
        } else {
            rebuildAll(); // 실행 취소 등 바깥에서 바뀐 경우
        }
    });
    rebuildAll();
}

// ---------------------------------------------------------------- 공통

bool DatabaseEditor::apply(const std::function<bool(naru::Database&)>& change, const QString& text,
                           const QString& mergeKey) {
    const naru::Database& before = doc_->project().database;
    naru::Database after = before;
    if (!change(after) || after == before) {
        return false;
    }
    committing_ = true;
    doc_->undoStack()->push(new SetDatabaseCommand(doc_, before, after, text, mergeKey));
    committing_ = false;
    return true;
}

const DataType* DatabaseEditor::currentType() const {
    return doc_->project().database.findType(typeKey_.toStdString());
}

const DataRecord* DatabaseEditor::currentRecord() const {
    const DataType* t = currentType();
    return t ? t->findRecord(recordId_.toStdString()) : nullptr;
}

void DatabaseEditor::select(const QString& typeKey, const QString& recordId) {
    typeKey_ = typeKey;
    recordId_ = recordId;
    rebuildAll();
}

void DatabaseEditor::showFieldsTab() {
    detailTabs_->setCurrentIndex(1);
}

// ---------------------------------------------------------------- 화면 갱신

void DatabaseEditor::rebuildAll() {
    rebuildTypes();
    rebuildRecords();
    rebuildForm();
    rebuildFields();
    updateIssues();
}

void DatabaseEditor::rebuildTypes() {
    rebuilding_ = true;
    const auto& db = doc_->project().database;
    if (!db.findType(typeKey_.toStdString())) {
        typeKey_ = db.types.empty() ? QString() : q(db.types.front().key);
        recordId_.clear();
    }
    types_->clear();
    for (const auto& t : db.types) {
        auto* item = new QListWidgetItem(QStringLiteral("%1  (%2) · %3개").arg(q(t.name), q(t.key)).arg(t.records.size()),
                                         types_);
        item->setData(Qt::UserRole, q(t.key));
        if (q(t.key) == typeKey_) {
            types_->setCurrentItem(item);
        }
    }
    rebuilding_ = false;
}

void DatabaseEditor::rebuildRecords() {
    rebuilding_ = true;
    records_->clear();
    const DataType* t = currentType();
    if (t && !t->findRecord(recordId_.toStdString())) {
        recordId_ = t->records.empty() ? QString() : q(t->records.front().id);
    }
    if (t) {
        QString filter = filter_->text().trimmed();
        for (const auto& r : t->records) {
            QString label = QStringLiteral("%1  (%2)").arg(q(r.name), q(r.id));
            if (!filter.isEmpty() && !label.contains(filter, Qt::CaseInsensitive)) {
                continue;
            }
            auto* item = new QListWidgetItem(label, records_);
            item->setData(Qt::UserRole, q(r.id));
            if (q(r.id) == recordId_) {
                records_->setCurrentItem(item);
            }
        }
    }
    rebuilding_ = false;
}

void DatabaseEditor::rebuildForm() {
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);
    const DataType* t = currentType();
    const DataRecord* r = currentRecord();
    const auto& db = doc_->project().database;
    if (!t) {
        form->addRow(new QLabel(QStringLiteral("왼쪽 '종류 추가'로 데이터 종류를 만드세요.\n템플릿(캐릭터·아이템·몬스터)으로 시작해서 고쳐 써도 됩니다.")));
    } else if (!r) {
        form->addRow(new QLabel(QStringLiteral("'%1'에 항목이 없습니다. 가운데 '추가'를 누르세요.").arg(q(t->name))));
    } else {
        auto* id = new QLabel(q(r->id));
        id->setTextInteractionFlags(Qt::TextSelectableByMouse);
        id->setToolTip(QStringLiteral("스크립트: ctx.db.get(\"%1\", \"%2\")").arg(q(t->key), q(r->id)));
        form->addRow(QStringLiteral("ID"), id);
        auto* name = new QLineEdit(q(r->name));
        connect(name, &QLineEdit::editingFinished, this, [this, name] {
            if (name->isModified()) {
                name->setModified(false);
                renameRecord(name->text().trimmed());
            }
        });
        form->addRow(QStringLiteral("이름"), name);
        if (t->fields.empty()) {
            form->addRow(new QLabel(QStringLiteral("속성이 없습니다. '속성 정의' 탭에서 추가하세요.")));
        }
        QString typeKey = q(t->key);
        QString recordId = q(r->id);
        for (const auto& f : t->fields) {
            naru::Value value = t->valueOf(*r, f);
            auto* editor = new ValueEditor(f, value, &db);
            auto* label = new QLabel(q(f.name));
            QString tip = QStringLiteral("키: %1 · %2").arg(q(f.key), ValueEditor::typeLabel(f.type));
            if (auto problem = naru::checkValue(value, f)) {
                label->setStyleSheet(QStringLiteral("color: #c03030"));
                tip += QStringLiteral("\n⚠ ") + q(*problem);
            }
            label->setToolTip(tip);
            QString fieldKey = q(f.key);
            connect(editor, &ValueEditor::edited, this, [this, editor, typeKey, recordId, fieldKey] {
                setRecordValue(typeKey, recordId, fieldKey, editor->value());
            });
            form->addRow(label, editor);
        }
    }
    delete formArea_->takeWidget();
    formArea_->setWidget(page);
}

void DatabaseEditor::rebuildFields() {
    int selected = fields_->currentRow();
    fields_->setRowCount(0);
    const DataType* t = currentType();
    if (!t) {
        return;
    }
    const auto& db = doc_->project().database;
    for (const auto& f : t->fields) {
        int row = fields_->rowCount();
        fields_->insertRow(row);
        fields_->setItem(row, 0, new QTableWidgetItem(q(f.name)));
        fields_->setItem(row, 1, new QTableWidgetItem(q(f.key)));
        fields_->setItem(row, 2, new QTableWidgetItem(ValueEditor::typeLabel(f.type)));
        fields_->setItem(row, 3, new QTableWidgetItem(displayValue(f.defaultValue, f, db)));
        fields_->setItem(row, 4, new QTableWidgetItem(fieldSettings(f, db)));
    }
    fields_->resizeColumnsToContents();
    if (selected >= 0 && selected < fields_->rowCount()) {
        fields_->selectRow(selected);
    }
}

void DatabaseEditor::updateIssues() {
    auto issues = doc_->project().database.validate();
    if (issues.empty()) {
        issues_->hide();
        return;
    }
    QStringList lines;
    for (std::size_t i = 0; i < issues.size() && i < 20; ++i) {
        lines << QStringLiteral("%1: %2").arg(q(issues[i].location), q(issues[i].message));
    }
    issues_->setText(QStringLiteral("⚠ 확인이 필요한 곳 %1건 — %2%3")
                         .arg(issues.size())
                         .arg(lines.first(), issues.size() > 1 ? QStringLiteral(" (마우스를 올리면 전체 목록)") : QString()));
    issues_->setToolTip(lines.join('\n'));
    issues_->show();
}

// ---------------------------------------------------------------- 데이터 종류

bool DatabaseEditor::addType(const DataType& type) {
    bool ok = apply(
        [&](naru::Database& db) {
            if (!naru::isValidDataId(type.key) || db.findType(type.key)) {
                return false;
            }
            db.types.push_back(type);
            return true;
        },
        QStringLiteral("데이터 종류 추가: %1").arg(q(type.name)));
    if (ok) {
        select(q(type.key));
    }
    return ok;
}

void DatabaseEditor::addTypeFromTemplate(naru::DataTemplate kind) {
    DataType t = naru::makeTemplate(kind, doc_->project().database);
    QString name = q(t.name);
    QString key = q(t.key);
    const auto& db = doc_->project().database;
    if (!askNameAndKey(this, QStringLiteral("데이터 종류 추가"), QStringLiteral("키 (영문)"), name, key,
                       [&db](const QString& k) -> QString {
                           if (!naru::isValidDataId(k.toStdString())) {
                               return QStringLiteral("키는 영문·숫자·_·- 로 쓰세요.");
                           }
                           if (db.findType(k.toStdString())) {
                               return QStringLiteral("같은 키의 데이터 종류가 이미 있습니다.");
                           }
                           return {};
                       })) {
        return;
    }
    t.name = name.toStdString();
    t.key = key.toStdString();
    addType(t);
}

void DatabaseEditor::renameType() {
    const DataType* t = currentType();
    if (!t) {
        return;
    }
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("이름 변경"), QStringLiteral("데이터 종류 이름"),
                                         QLineEdit::Normal, q(t->name), &ok)
                       .trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    std::string key = t->key;
    apply(
        [&](naru::Database& db) {
            db.findType(key)->name = name.toStdString();
            return true;
        },
        QStringLiteral("데이터 종류 이름 변경: %1").arg(name));
}

void DatabaseEditor::removeType() {
    const DataType* t = currentType();
    if (!t) {
        return;
    }
    std::string key = t->key;
    QStringList refs;
    for (const auto& r : doc_->project().database.referencesTo(key)) {
        if (r.rfind(key + ".", 0) != 0) {
            refs << q(r);
        }
    }
    if (!refs.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("삭제할 수 없음"),
                             QStringLiteral("다른 데이터가 '%1'을(를) 참조하고 있습니다:\n%2\n\n먼저 그 속성을 지우거나 참조 대상을 바꾸세요.")
                                 .arg(q(t->name), refs.join('\n')));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("데이터 종류 삭제"),
                              QStringLiteral("'%1'과(와) 항목 %2개를 삭제할까요? (Ctrl+Z로 되돌릴 수 있습니다)")
                                  .arg(q(t->name))
                                  .arg(t->records.size())) != QMessageBox::Yes) {
        return;
    }
    QString name = q(t->name);
    apply(
        [&](naru::Database& db) {
            db.types.erase(db.types.begin() + db.indexOfType(key));
            return true;
        },
        QStringLiteral("데이터 종류 삭제: %1").arg(name));
    rebuildAll();
}

// ---------------------------------------------------------------- 항목

bool DatabaseEditor::addRecord(const QString& typeKey, const DataRecord& record) {
    bool ok = apply(
        [&](naru::Database& db) {
            DataType* t = db.findType(typeKey.toStdString());
            if (!t || !naru::isValidDataId(record.id) || t->findRecord(record.id)) {
                return false;
            }
            t->records.push_back(record);
            return true;
        },
        QStringLiteral("항목 추가: %1").arg(q(record.name)));
    if (ok) {
        select(typeKey, q(record.id));
    }
    return ok;
}

void DatabaseEditor::addRecordInteractive() {
    const DataType* t = currentType();
    if (!t) {
        return;
    }
    QString name = QStringLiteral("새 %1").arg(q(t->name));
    QString id = q(t->newRecordId(t->key));
    if (!askNameAndKey(this, QStringLiteral("항목 추가"), QStringLiteral("ID (영문)"), name, id,
                       [t](const QString& k) -> QString {
                           if (!naru::isValidDataId(k.toStdString())) {
                               return QStringLiteral("ID는 영문·숫자·_·- 로 쓰세요.");
                           }
                           if (t->findRecord(k.toStdString())) {
                               return QStringLiteral("같은 ID의 항목이 이미 있습니다.");
                           }
                           return {};
                       })) {
        return;
    }
    addRecord(typeKey_, DataRecord{id.toStdString(), name.toStdString(), {}});
}

void DatabaseEditor::duplicateRecord() {
    const DataType* t = currentType();
    const DataRecord* r = currentRecord();
    if (!t || !r) {
        return;
    }
    DataRecord copy = *r;
    copy.id = t->newRecordId(r->id);
    copy.name = r->name + " 복사";
    addRecord(typeKey_, copy);
}

void DatabaseEditor::removeRecord() {
    const DataRecord* r = currentRecord();
    if (!r) {
        return;
    }
    std::string type = typeKey_.toStdString();
    std::string id = r->id;
    QString name = q(r->name);
    apply(
        [&](naru::Database& db) {
            DataType* t = db.findType(type);
            t->records.erase(t->records.begin() + t->indexOfRecord(id));
            return true;
        },
        QStringLiteral("항목 삭제: %1").arg(name));
    rebuildAll();
}

void DatabaseEditor::renameRecord(const QString& name) {
    if (name.isEmpty()) {
        rebuildForm();
        return;
    }
    std::string type = typeKey_.toStdString();
    std::string id = recordId_.toStdString();
    apply(
        [&](naru::Database& db) {
            DataType* t = db.findType(type);
            DataRecord* r = t ? t->findRecord(id) : nullptr;
            if (!r) {
                return false;
            }
            r->name = name.toStdString();
            return true;
        },
        QStringLiteral("항목 이름 변경: %1").arg(name));
}

bool DatabaseEditor::setRecordValue(const QString& typeKey, const QString& recordId, const QString& fieldKey,
                                    const naru::Value& value) {
    return apply(
        [&](naru::Database& db) {
            DataType* t = db.findType(typeKey.toStdString());
            DataRecord* r = t ? t->findRecord(recordId.toStdString()) : nullptr;
            if (!r || !t->findField(fieldKey.toStdString())) {
                return false;
            }
            r->values[fieldKey.toStdString()] = value;
            return true;
        },
        QStringLiteral("값 변경: %1.%2").arg(recordId, fieldKey), typeKey + "/" + recordId + "." + fieldKey);
}

// ---------------------------------------------------------------- 속성 정의

void DatabaseEditor::addFieldInteractive() {
    const DataType* t = currentType();
    if (!t) {
        return;
    }
    FieldDef f;
    f.name = "새 속성";
    f.type = FieldType::Integer;
    f.defaultValue = std::int64_t{0};
    FieldDialog dialog(f, *t, doc_->project().database, true, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    FieldDef added = dialog.field();
    std::string type = t->key;
    apply(
        [&](naru::Database& db) {
            db.findType(type)->fields.push_back(added);
            return true;
        },
        QStringLiteral("속성 추가: %1").arg(q(added.name)));
    rebuildForm();
    fields_->selectRow(fields_->rowCount() - 1);
}

bool DatabaseEditor::setField(const QString& typeKey, int index, const FieldDef& field, bool askBeforeReset) {
    const DataType* t = doc_->project().database.findType(typeKey.toStdString());
    if (!t || index < 0 || index >= static_cast<int>(t->fields.size())) {
        return false;
    }
    const std::string key = t->fields[static_cast<std::size_t>(index)].key;
    // 기존 값을 새 정의로 변환한다. 변환할 수 없는 값은 지워서 기본값으로 돌아가게 한다.
    int reset = 0;
    for (const auto& r : t->records) {
        auto it = r.values.find(key);
        if (it != r.values.end() && !naru::convertValue(it->second, field)) {
            ++reset;
        }
    }
    if (reset > 0 && askBeforeReset &&
        QMessageBox::question(this, QStringLiteral("속성 변경"),
                              QStringLiteral("항목 %1개의 값을 새 형식으로 바꿀 수 없어 기본값으로 돌아갑니다. 계속할까요?")
                                  .arg(reset)) != QMessageBox::Yes) {
        return false;
    }
    bool ok = apply(
        [&](naru::Database& db) {
            DataType* type = db.findType(typeKey.toStdString());
            FieldDef updated = field;
            updated.key = key;
            type->fields[static_cast<std::size_t>(index)] = updated;
            for (auto& r : type->records) {
                auto it = r.values.find(key);
                if (it == r.values.end()) {
                    continue;
                }
                if (auto converted = naru::convertValue(it->second, updated)) {
                    it->second = *converted;
                } else {
                    r.values.erase(it);
                }
            }
            return true;
        },
        QStringLiteral("속성 변경: %1").arg(q(field.name)));
    rebuildForm();
    return ok;
}

void DatabaseEditor::editFieldInteractive() {
    const DataType* t = currentType();
    int row = fields_->currentRow();
    if (!t || row < 0 || row >= static_cast<int>(t->fields.size())) {
        return;
    }
    FieldDialog dialog(t->fields[static_cast<std::size_t>(row)], *t, doc_->project().database, false, this);
    if (dialog.exec() == QDialog::Accepted) {
        setField(typeKey_, row, dialog.field(), true);
    }
}

void DatabaseEditor::removeField() {
    const DataType* t = currentType();
    int row = fields_->currentRow();
    if (!t || row < 0 || row >= static_cast<int>(t->fields.size())) {
        return;
    }
    const FieldDef& f = t->fields[static_cast<std::size_t>(row)];
    int used = static_cast<int>(std::count_if(t->records.begin(), t->records.end(),
                                              [&f](const DataRecord& r) { return r.values.contains(f.key); }));
    if (used > 0 && QMessageBox::question(this, QStringLiteral("속성 삭제"),
                                          QStringLiteral("'%1' 속성과 항목 %2개의 값을 삭제할까요? (Ctrl+Z로 되돌릴 수 있습니다)")
                                              .arg(q(f.name))
                                              .arg(used)) != QMessageBox::Yes) {
        return;
    }
    std::string type = t->key;
    std::string key = f.key;
    QString name = q(f.name);
    apply(
        [&](naru::Database& db) {
            DataType* dt = db.findType(type);
            dt->fields.erase(dt->fields.begin() + dt->indexOfField(key));
            for (auto& r : dt->records) {
                r.values.erase(key);
            }
            return true;
        },
        QStringLiteral("속성 삭제: %1").arg(name));
    rebuildForm();
}

void DatabaseEditor::moveField(int delta) {
    const DataType* t = currentType();
    int row = fields_->currentRow();
    int to = row + delta;
    if (!t || row < 0 || to < 0 || to >= static_cast<int>(t->fields.size())) {
        return;
    }
    std::string type = t->key;
    apply(
        [&](naru::Database& db) {
            auto& fields = db.findType(type)->fields;
            std::swap(fields[static_cast<std::size_t>(row)], fields[static_cast<std::size_t>(to)]);
            return true;
        },
        QStringLiteral("속성 순서 변경"));
    fields_->selectRow(to);
    rebuildForm();
}
