#include "GlobalScriptsDialog.h"

#include "naru/python/PythonRuntime.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

GlobalScriptsDialog::GlobalScriptsDialog(const QStringList& files, const std::vector<std::string>& registered,
                                         const QString& scriptsDir, naru::python::PythonRuntime* python,
                                         QWidget* parent)
    : QDialog(parent), scriptsDir_(scriptsDir), python_(python) {
    setWindowTitle(QStringLiteral("전역 스크립트"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("체크한 스크립트는 게임 전체에서 실행됩니다. 위에서부터 순서대로 "
                                           "on_game_start · on_scene_enter · on_scene_exit · on_update 함수가 불립니다.\n"
                                           "다른 스크립트에서는 파일 이름으로 import할 수 있습니다 (예: import game)."),
                            this);
    help->setWordWrap(true);
    layout->addWidget(help);

    list_ = new QListWidget(this);
    // 등록된 것(순서대로) 먼저, 그다음 나머지 파일.
    QStringList ordered;
    for (const std::string& name : registered) {
        ordered << QString::fromStdString(name);
    }
    for (const QString& f : files) {
        if (!ordered.contains(f)) {
            ordered << f;
        }
    }
    for (const QString& name : ordered) {
        auto* item = new QListWidgetItem(name, list_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        bool isRegistered = std::find(registered.begin(), registered.end(), name.toStdString()) != registered.end();
        item->setCheckState(isRegistered ? Qt::Checked : Qt::Unchecked);
        if (!files.contains(name)) {
            item->setText(name + QStringLiteral("  (파일 없음)"));
            item->setData(Qt::UserRole, name);
        }
    }
    connect(list_, &QListWidget::itemChanged, this, &GlobalScriptsDialog::recheck);

    auto* row = new QHBoxLayout;
    row->addWidget(list_, 1);
    auto* buttons = new QVBoxLayout;
    auto* up = new QPushButton(QStringLiteral("위로"), this);
    auto* down = new QPushButton(QStringLiteral("아래로"), this);
    connect(up, &QPushButton::clicked, this, [this] { move(-1); });
    connect(down, &QPushButton::clicked, this, [this] { move(1); });
    buttons->addWidget(up);
    buttons->addWidget(down);
    buttons->addStretch();
    row->addLayout(buttons);
    layout->addLayout(row);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
    resize(520, 420);
    recheck();
}

std::vector<std::string> GlobalScriptsDialog::selected() const {
    std::vector<std::string> result;
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        if (item->checkState() == Qt::Checked) {
            QString name = item->data(Qt::UserRole).isValid() ? item->data(Qt::UserRole).toString() : item->text();
            result.push_back(name.toStdString());
        }
    }
    return result;
}

void GlobalScriptsDialog::move(int delta) {
    int row = list_->currentRow();
    int target = row + delta;
    if (row < 0 || target < 0 || target >= list_->count()) {
        return;
    }
    QListWidgetItem* item = list_->takeItem(row);
    list_->insertItem(target, item);
    list_->setCurrentRow(target);
    recheck();
}

void GlobalScriptsDialog::recheck() {
    std::vector<std::string> names = selected();
    if (names.empty()) {
        status_->setText(QStringLiteral("등록된 전역 스크립트가 없습니다."));
        status_->setStyleSheet(QString());
        return;
    }
    if (!python_) {
        status_->clear();
        return;
    }
    std::vector<std::string> paths;
    for (const std::string& name : names) {
        paths.push_back(QFileInfo(QDir(scriptsDir_).filePath(QString::fromStdString(name))).absoluteFilePath().toStdString());
    }
    QStringList problems;
    for (const auto& e : python_->checkGlobalScripts(paths)) {
        QString where = QFileInfo(QString::fromStdString(e.file)).fileName();
        if (e.line > 0) {
            where += QStringLiteral(":%1").arg(e.line);
        }
        problems << QStringLiteral("%1: %2").arg(where, QString::fromStdString(e.message));
    }
    if (problems.isEmpty()) {
        status_->setText(QStringLiteral("검사 통과: %1개 등록").arg(names.size()));
        status_->setStyleSheet(QStringLiteral("color: #2a7a2a;"));
    } else {
        status_->setText(QStringLiteral("문제:\n") + problems.join('\n'));
        status_->setStyleSheet(QStringLiteral("color: #c02828;"));
    }
}
