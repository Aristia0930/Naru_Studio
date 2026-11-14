#pragma once
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <set>
#include <stdexcept>
#include <map>
#include <string>

// Editable string pairs shared by headers and JSON response mappings.
class NetworkFields : public QWidget {
public:
    QTableWidget* table;
    NetworkFields(const QString& left, const QString& right, QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0);
        table = new QTableWidget(0, 2, this);
        table->setHorizontalHeaderLabels({left, right});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setMinimumHeight(115); table->setMaximumHeight(180);
        layout->addWidget(table);
        auto* buttons = new QHBoxLayout;
        auto* add = new QPushButton(QStringLiteral("행 추가"), this);
        auto* remove = new QPushButton(QStringLiteral("선택 행 제거"), this);
        buttons->addWidget(add); buttons->addWidget(remove); buttons->addStretch(); layout->addLayout(buttons);
        connect(add, &QPushButton::clicked, this, [this] { append({}, {}); table->setCurrentCell(table->rowCount()-1, 0); table->editItem(table->currentItem()); });
        connect(remove, &QPushButton::clicked, this, [this] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
    }
    void append(const QString& key, const QString& value) {
        int row = table->rowCount(); table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(key)); table->setItem(row, 1, new QTableWidgetItem(value));
    }
    void setValues(const std::map<std::string, std::string>& values) {
        table->setRowCount(0);
        for (const auto& [k, v] : values) append(QString::fromStdString(k), QString::fromStdString(v));
    }
    std::map<std::string, std::string> values(bool headers = false) const {
        std::map<std::string, std::string> values;
        std::set<QString> keys;
        for (int row = 0; row < table->rowCount(); ++row) {
            auto key = table->item(row, 0)->text().trimmed();
            auto value = table->item(row, 1)->text();
            auto normalized = headers ? key.toLower() : key;
            if (keys.contains(normalized)) throw std::invalid_argument("표의 이름/경로가 중복됩니다.");
            keys.insert(normalized);
            values.emplace(key.toStdString(), value.toStdString());
        }
        return values;
    }
};
