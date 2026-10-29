#include "VariablesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdint>

namespace {

enum Column { NameColumn, KindColumn, ValueColumn, DescriptionColumn };

} // namespace

QString VariablesDialog::kindLabel(const naru::Value& value) {
    if (std::holds_alternative<bool>(value)) return QStringLiteral("스위치");
    if (std::holds_alternative<double>(value)) return QStringLiteral("소수");
    if (std::holds_alternative<std::string>(value)) return QStringLiteral("글자");
    return QStringLiteral("정수");
}

VariablesDialog::VariablesDialog(const std::vector<naru::VariableDef>& variables, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("변수·스위치"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("게임 전체에서 쓰는 변수입니다. 게임 실행(Ctrl+R)과 이벤트 테스트(F6)는 "
                                           "여기 적은 처음 값으로 시작합니다.\n"
                                           "스위치는 참/거짓 하나만 담는 변수입니다 (예: 문이 열렸는가). "
                                           "이벤트의 변수 칸에서 목록으로 고를 수 있고, 스크립트에서는 "
                                           "ctx.variables.get(\"이름\")으로 읽습니다."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({QStringLiteral("이름"), QStringLiteral("종류"), QStringLiteral("처음 값"),
                                       QStringLiteral("설명")});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setSectionResizeMode(DescriptionColumn, QHeaderView::Stretch);
    table_->setColumnWidth(NameColumn, 160);
    table_->setColumnWidth(KindColumn, 90);
    table_->setColumnWidth(ValueColumn, 120);

    auto* row = new QHBoxLayout;
    row->addWidget(table_, 1);
    auto* buttons = new QVBoxLayout;
    auto* add = new QPushButton(QStringLiteral("변수 추가"), this);
    auto* addSwitch = new QPushButton(QStringLiteral("스위치 추가"), this);
    auto* remove = new QPushButton(QStringLiteral("삭제"), this);
    auto* up = new QPushButton(QStringLiteral("위로"), this);
    auto* down = new QPushButton(QStringLiteral("아래로"), this);
    connect(add, &QPushButton::clicked, this, [this] {
        Entry e;
        e.text = QStringLiteral("0");
        addRow(e);
        table_->setCurrentCell(table_->rowCount() - 1, NameColumn);
        rows_.back().name->setFocus();
    });
    connect(addSwitch, &QPushButton::clicked, this, [this] {
        Entry e;
        e.kind = Switch;
        addRow(e);
        table_->setCurrentCell(table_->rowCount() - 1, NameColumn);
        rows_.back().name->setFocus();
    });
    connect(remove, &QPushButton::clicked, this, &VariablesDialog::removeCurrent);
    connect(up, &QPushButton::clicked, this, [this] { moveCurrent(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveCurrent(1); });
    for (auto* b : {add, addSwitch, remove, up, down}) {
        buttons->addWidget(b);
    }
    buttons->addStretch();
    row->addLayout(buttons);
    layout->addLayout(row);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &VariablesDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);

    std::vector<Entry> list;
    for (const auto& v : variables) {
        Entry e;
        e.name = QString::fromStdString(v.name);
        e.description = QString::fromStdString(v.description);
        if (const auto* b = std::get_if<bool>(&v.initial)) {
            e.kind = Switch;
            e.on = *b;
        } else {
            e.kind = std::holds_alternative<double>(v.initial)        ? Decimal
                     : std::holds_alternative<std::string>(v.initial) ? Text
                                                                      : Integer;
            e.text = QString::fromStdString(naru::toDisplayString(v.initial));
        }
        list.push_back(e);
    }
    fill(list, list.empty() ? -1 : 0);
    resize(760, 420);
}

void VariablesDialog::addRow(const Entry& entry) {
    int r = table_->rowCount();
    table_->insertRow(r);
    Row row{};
    row.name = new QLineEdit(entry.name, table_);
    row.name->setPlaceholderText(QStringLiteral("예: gold"));
    row.kind = new QComboBox(table_);
    row.kind->addItems({QStringLiteral("정수"), QStringLiteral("소수"), QStringLiteral("글자"), QStringLiteral("스위치")});
    row.kind->setCurrentIndex(entry.kind);
    auto* value = new QWidget(table_);
    auto* valueLayout = new QHBoxLayout(value);
    valueLayout->setContentsMargins(0, 0, 0, 0);
    row.text = new QLineEdit(entry.text, value);
    row.boolean = new QComboBox(value);
    row.boolean->addItems({QStringLiteral("참 (켜짐)"), QStringLiteral("거짓 (꺼짐)")});
    row.boolean->setCurrentIndex(entry.on ? 0 : 1);
    valueLayout->addWidget(row.text);
    valueLayout->addWidget(row.boolean);
    row.description = new QLineEdit(entry.description, table_);

    auto sync = [row] {
        bool isSwitch = row.kind->currentIndex() == Switch;
        row.text->setVisible(!isSwitch);
        row.boolean->setVisible(isSwitch);
        row.text->setPlaceholderText(row.kind->currentIndex() == Text ? QStringLiteral("비워 두면 빈 글자") : QString());
    };
    connect(row.kind, &QComboBox::currentIndexChanged, this, sync);
    sync();
    // 칸을 누르면 그 줄이 선택되어 삭제·위로·아래로가 그 줄에 적용된다.
    for (QWidget* w : {static_cast<QWidget*>(row.name), static_cast<QWidget*>(row.kind),
                       static_cast<QWidget*>(row.text), static_cast<QWidget*>(row.boolean),
                       static_cast<QWidget*>(row.description)}) {
        w->installEventFilter(this);
        w->setProperty("naruRow", r);
    }
    table_->setCellWidget(r, NameColumn, row.name);
    table_->setCellWidget(r, KindColumn, row.kind);
    table_->setCellWidget(r, ValueColumn, value);
    table_->setCellWidget(r, DescriptionColumn, row.description);
    rows_.push_back(row);
}

bool VariablesDialog::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FocusIn) {
        QVariant r = watched->property("naruRow");
        if (r.isValid()) {
            table_->selectRow(r.toInt());
        }
    }
    return QDialog::eventFilter(watched, event);
}

std::vector<VariablesDialog::Entry> VariablesDialog::entries() const {
    std::vector<Entry> list;
    for (const Row& row : rows_) {
        Entry e;
        e.name = row.name->text().trimmed();
        e.kind = row.kind->currentIndex();
        e.text = row.text->text();
        e.on = row.boolean->currentIndex() == 0;
        e.description = row.description->text().trimmed();
        list.push_back(e);
    }
    return list;
}

void VariablesDialog::fill(const std::vector<Entry>& list, int current) {
    table_->setRowCount(0);
    rows_.clear();
    for (const Entry& e : list) {
        addRow(e);
    }
    if (current >= 0 && current < table_->rowCount()) {
        table_->selectRow(current);
    }
}

void VariablesDialog::moveCurrent(int delta) {
    int r = table_->currentRow();
    int to = r + delta;
    std::vector<Entry> list = entries();
    if (r < 0 || to < 0 || to >= static_cast<int>(list.size())) {
        return;
    }
    std::swap(list[static_cast<std::size_t>(r)], list[static_cast<std::size_t>(to)]);
    fill(list, to);
}

void VariablesDialog::removeCurrent() {
    int r = table_->currentRow();
    std::vector<Entry> list = entries();
    if (r < 0 || r >= static_cast<int>(list.size())) {
        return;
    }
    list.erase(list.begin() + r);
    fill(list, std::min(r, static_cast<int>(list.size()) - 1));
}

void VariablesDialog::accept() {
    std::vector<naru::VariableDef> result;
    QSet<QString> names;
    std::vector<Entry> list = entries();
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Entry& e = list[i];
        auto fail = [this, i](const QString& message) {
            table_->selectRow(static_cast<int>(i));
            QMessageBox::warning(this, windowTitle(), QStringLiteral("%1번째 줄: %2").arg(i + 1).arg(message));
        };
        if (e.name.isEmpty()) {
            fail(QStringLiteral("이름을 입력하세요."));
            return;
        }
        if (e.name.toStdString().size() > 64) {
            fail(QStringLiteral("이름이 너무 깁니다 (64자까지)."));
            return;
        }
        if (names.contains(e.name)) {
            fail(QStringLiteral("이름 '%1'이(가) 이미 있습니다.").arg(e.name));
            return;
        }
        names.insert(e.name);
        naru::VariableDef def;
        def.name = e.name.toStdString();
        def.description = e.description.toStdString();
        bool ok = true;
        switch (e.kind) {
        case Integer: {
            qlonglong v = e.text.trimmed().toLongLong(&ok);
            def.initial = static_cast<std::int64_t>(v);
            break;
        }
        case Decimal: def.initial = e.text.trimmed().toDouble(&ok); break;
        case Text: def.initial = e.text.toStdString(); break;
        default: def.initial = e.on; break;
        }
        if (!ok) {
            fail(QStringLiteral("'%1'의 처음 값은 %2여야 합니다: %3")
                     .arg(e.name, e.kind == Integer ? QStringLiteral("정수(예: 0, 100)") : QStringLiteral("숫자(예: 1.5)"),
                          e.text));
            return;
        }
        result.push_back(def);
    }
    result_ = result;
    QDialog::accept();
}
