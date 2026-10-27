#include "StartDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

constexpr int kMaxRecent = 10;
const char* kRecentKey = "recentProjects";

bool isProjectDirectory(const QString& dir) {
    return QFileInfo(QDir(dir).filePath(QStringLiteral("project.json"))).isFile();
}

// 이름에서 폴더 이름으로 쓸 수 없는 문자를 바꾼다.
QString folderNameFor(const QString& name) {
    QString result = name.trimmed();
    static const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (QChar& c : result) {
        if (invalid.contains(c) || c.unicode() < 32) {
            c = QLatin1Char('_');
        }
    }
    return result;
}

class NewProjectDialog : public QDialog {
public:
    explicit NewProjectDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("새 프로젝트"));
        name_ = new QLineEdit(QStringLiteral("새 게임"), this);
        location_ = new QLineEdit(this);
        QSettings settings;
        location_->setText(settings.value(QStringLiteral("lastProjectLocation"),
                                          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                                              QStringLiteral("/NaruProjects"))
                               .toString());
        auto* browse = new QPushButton(QStringLiteral("찾아보기..."), this);
        auto* locationRow = new QHBoxLayout;
        locationRow->addWidget(location_);
        locationRow->addWidget(browse);
        preview_ = new QLabel(this);
        preview_->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto* form = new QFormLayout;
        form->addRow(QStringLiteral("프로젝트 이름"), name_);
        form->addRow(QStringLiteral("저장 위치"), locationRow);
        form->addRow(QStringLiteral("만들 폴더"), preview_);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("만들기"));
        auto* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(560, sizeHint().height());

        connect(browse, &QPushButton::clicked, this, [this] {
            QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("저장 위치"), location_->text());
            if (!dir.isEmpty()) {
                location_->setText(QDir::toNativeSeparators(dir));
            }
        });
        connect(name_, &QLineEdit::textChanged, this, &NewProjectDialog::updatePreview);
        connect(location_, &QLineEdit::textChanged, this, &NewProjectDialog::updatePreview);
        connect(buttons, &QDialogButtonBox::accepted, this, &NewProjectDialog::tryAccept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        updatePreview();
    }

    QString name() const { return name_->text().trimmed(); }
    QString directory() const {
        return QDir::cleanPath(QDir(location_->text().trimmed()).filePath(folderNameFor(name())));
    }

private:
    void updatePreview() { preview_->setText(QDir::toNativeSeparators(directory())); }

    void tryAccept() {
        if (name().isEmpty() || location_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, windowTitle(), QStringLiteral("이름과 저장 위치를 입력하세요."));
            return;
        }
        QDir dir(directory());
        if (dir.exists() && !dir.isEmpty()) {
            QMessageBox::warning(this, windowTitle(),
                                 QStringLiteral("이미 내용이 있는 폴더입니다:\n%1").arg(QDir::toNativeSeparators(directory())));
            return;
        }
        QSettings().setValue(QStringLiteral("lastProjectLocation"), location_->text().trimmed());
        accept();
    }

    QLineEdit* name_;
    QLineEdit* location_;
    QLabel* preview_;
};

} // namespace

StartDialog::StartDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Naru Studio"));

    auto* title = new QLabel(QStringLiteral("<h2>Naru Studio</h2>"), this);
    auto* newButton = new QPushButton(QStringLiteral("새 프로젝트 만들기..."), this);
    auto* openButton = new QPushButton(QStringLiteral("프로젝트 열기..."), this);
    recent_ = new QListWidget(this);

    auto* buttons = new QVBoxLayout;
    buttons->addWidget(newButton);
    buttons->addWidget(openButton);
    buttons->addStretch();

    auto* recentBox = new QVBoxLayout;
    recentBox->addWidget(new QLabel(QStringLiteral("최근 프로젝트 (더블클릭하여 열기)"), this));
    recentBox->addWidget(recent_);

    auto* body = new QHBoxLayout;
    body->addLayout(recentBox, 1);
    body->addLayout(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addLayout(body);
    resize(720, 420);

    connect(newButton, &QPushButton::clicked, this, &StartDialog::newProject);
    connect(openButton, &QPushButton::clicked, this, &StartDialog::openProject);
    connect(recent_, &QListWidget::itemActivated, this, &StartDialog::openRecent);
    reloadRecent();
}

QStringList StartDialog::recentProjects() {
    return QSettings().value(kRecentKey).toStringList();
}

void StartDialog::addRecentProject(const QString& directory) {
    QString dir = QDir::cleanPath(QDir(directory).absolutePath());
    QStringList list = recentProjects();
    list.removeAll(dir);
    list.prepend(dir);
    while (list.size() > kMaxRecent) {
        list.removeLast();
    }
    QSettings().setValue(kRecentKey, list);
}

void StartDialog::reloadRecent() {
    recent_->clear();
    for (const QString& dir : recentProjects()) {
        auto* item = new QListWidgetItem(QStringLiteral("%1\n    %2").arg(QFileInfo(dir).fileName(),
                                                                         QDir::toNativeSeparators(dir)),
                                         recent_);
        item->setData(Qt::UserRole, dir);
        if (!isProjectDirectory(dir)) {
            item->setForeground(Qt::gray);
            item->setToolTip(QStringLiteral("project.json을 찾을 수 없습니다"));
        }
    }
}

void StartDialog::newProject() {
    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    projectDirectory_ = dialog.directory();
    newProjectName_ = dialog.name();
    create_ = true;
    accept();
}

void StartDialog::openProject() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("프로젝트 폴더 선택 (project.json이 있는 폴더)"));
    if (dir.isEmpty()) {
        return;
    }
    if (!isProjectDirectory(dir)) {
        QMessageBox::warning(this, windowTitle(), QStringLiteral("선택한 폴더에 project.json이 없습니다."));
        return;
    }
    projectDirectory_ = dir;
    create_ = false;
    accept();
}

void StartDialog::openRecent() {
    QListWidgetItem* item = recent_->currentItem();
    if (!item) {
        return;
    }
    QString dir = item->data(Qt::UserRole).toString();
    if (!isProjectDirectory(dir)) {
        if (QMessageBox::question(this, windowTitle(),
                                  QStringLiteral("프로젝트를 찾을 수 없습니다. 목록에서 지울까요?\n%1").arg(dir)) ==
            QMessageBox::Yes) {
            QStringList list = recentProjects();
            list.removeAll(dir);
            QSettings().setValue(kRecentKey, list);
            reloadRecent();
        }
        return;
    }
    projectDirectory_ = dir;
    create_ = false;
    accept();
}
