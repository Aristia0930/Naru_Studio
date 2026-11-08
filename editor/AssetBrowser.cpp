#include "AssetBrowser.h"
#include "ProjectDocument.h"
#include "naru/project/ImageLayout.h"
#include "naru/audio/AudioDecoder.h"
#include "naru/runtime/Audio.h"

#include <SDL3/SDL.h>
#include <QBuffer>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRawFont>
#include <QSaveFile>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QUndoCommand>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <set>

namespace {
constexpr qint64 kMaxBytes = 64 * 1024 * 1024;
const QString kFilter = QStringLiteral("지원 에셋 (*.png *.jpg *.jpeg *.bmp *.wav *.mp3 *.ttf *.otf)");

QString kindName(const std::string& kind) {
    if (kind == "image") return QStringLiteral("이미지");
    if (kind == "audio") return QStringLiteral("소리");
    return QStringLiteral("글꼴");
}

QString hash(const QByteArray& data) {
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

// Decode the exact bytes that will be copied, including audio/font data.
bool decode(const QByteArray& bytes, const QString& suffix, std::string& kind, QString* error) {
    if (QStringList{"png", "jpg", "jpeg", "bmp"}.contains(suffix)) {
        kind = "image";
        QBuffer buffer;
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QSize size = reader.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 32 * 1024 * 1024 || reader.read().isNull()) {
            *error = QStringLiteral("이미지가 손상되었거나 너무 큽니다 (최대 3,200만 픽셀).");
            return false;
        }
        return true;
    }
    if (suffix == "wav" || suffix == "mp3") {
        kind = "audio";
        naru::DecodedAudio decoded;
        auto message = naru::decodeAudio(bytes.constData(), bytes.size(), suffix.toStdString(), decoded);
        if (!message.empty()) { *error = QString::fromStdString(message); return false; }
        return true;
    }
    if (suffix == "ttf" || suffix == "otf") {
        kind = "font";
        if (QRawFont(bytes, 24).isValid()) return true;
        *error = QStringLiteral("글꼴 파일을 읽을 수 없습니다.");
        return false;
    }
    *error = QStringLiteral("지원 형식: PNG/JPG/BMP, WAV/MP3, TTF/OTF.");
    return false;
}

class AssetCommand : public QUndoCommand {
public:
    AssetCommand(ProjectDocument* doc, std::vector<naru::Asset> after, std::vector<std::string> groups, const QString& text)
        : QUndoCommand(text), doc_(doc), before_(doc->project().assets), after_(std::move(after)),
          beforeGroups_(doc->project().assetGroups), afterGroups_(std::move(groups)) {}
    void redo() override { doc_->setAssetCatalog(after_, afterGroups_); }
    void undo() override { doc_->setAssetCatalog(before_, beforeGroups_); }
private:
    ProjectDocument* doc_;
    std::vector<naru::Asset> before_, after_;
    std::vector<std::string> beforeGroups_, afterGroups_;
};

class AssetIdCommand : public QUndoCommand {
public:
    AssetIdCommand(ProjectDocument* doc, const std::string& before, const std::string& after)
        : QUndoCommand(QStringLiteral("에셋 ID 변경")), doc_(doc), before_(doc->project().assets), after_(before_),
          beforeScenes_(doc->project().scenes), afterScenes_(beforeScenes_),
          beforeCommon_(doc->project().commonEvents), afterCommon_(beforeCommon_) {
        for (auto& a : after_) if (a.id == before) a.id = after;
        for (auto& s : afterScenes_) {
            if (s.backgroundImage.asset == before) s.backgroundImage.asset = after;
            for (auto& o : s.objects) {
                if (o.image.asset == before) o.image.asset = after;
                naru::renameAudioReferences(o.event.commands, before, after);
            }
        }
        for (auto& event : afterCommon_) naru::renameAudioReferences(event.event.commands, before, after);
    }
    void redo() override { doc_->setAssetIdentity(after_, afterScenes_, afterCommon_); }
    void undo() override { doc_->setAssetIdentity(before_, beforeScenes_, beforeCommon_); }
private:
    ProjectDocument* doc_;
    std::vector<naru::Asset> before_, after_;
    std::vector<naru::Scene> beforeScenes_, afterScenes_;
    std::vector<naru::CommonEvent> beforeCommon_, afterCommon_;
};
}

AssetBrowser::AssetBrowser(ProjectDocument* document, QWidget* parent) : QWidget(parent), doc_(document) {
    setObjectName(QStringLiteral("assetBrowser"));
    auto* outer = new QHBoxLayout(this);
    auto* splitter = new QSplitter(this);
    outer->addWidget(splitter);
    auto* catalog = new QWidget(splitter);
    auto* layout = new QVBoxLayout(catalog);
    auto* previewPanel = new QWidget(splitter);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({440, 440});
    auto* tools = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("에셋 추가..."), this);
    add->setObjectName(QStringLiteral("assetImport"));
    auto* reload = new QPushButton(QStringLiteral("새로 고침"), this);
    tools->addWidget(add); tools->addWidget(reload);
    layout->addLayout(tools);
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("assetSearch"));
    search_->setPlaceholderText(QStringLiteral("이름·ID·원본 파일명 검색"));
    kind_ = new QComboBox(this);
    kind_->addItem(QStringLiteral("모든 종류"), QString());
    kind_->addItem(QStringLiteral("이미지"), "image");
    kind_->addItem(QStringLiteral("소리 (WAV/MP3)"), "audio");
    kind_->addItem(QStringLiteral("글꼴"), "font");
    layout->addWidget(search_); layout->addWidget(kind_);
    group_ = new QComboBox(this);
    group_->setObjectName(QStringLiteral("assetGroupFilter"));
    layout->addWidget(group_);
    auto* groupTools = new QHBoxLayout;
    auto* addGroupButton = new QPushButton(QStringLiteral("그룹 추가..."), this);
    auto* renameGroupButton = new QPushButton(QStringLiteral("그룹 이름 변경..."), this);
    auto* removeGroupButton = new QPushButton(QStringLiteral("그룹 삭제..."), this);
    groupTools->addWidget(addGroupButton);
    groupTools->addWidget(renameGroupButton);
    groupTools->addWidget(removeGroupButton);
    layout->addLayout(groupTools);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("assetList"));
    list_->setIconSize(QSize(40, 40));
    list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(list_, 1);
    auto* editTools = new QHBoxLayout;
    auto* rename = new QPushButton(QStringLiteral("이름 변경"), this);
    auto* reimportButton = new QPushButton(QStringLiteral("파일 교체..."), this);
    reimportButton->setToolTip(QStringLiteral("ID와 이름을 유지하고 같은 종류의 파일로 교체합니다. 실행 취소할 수 있습니다."));
    auto* remove = new QPushButton(QStringLiteral("목록에서 제거..."), this);
    remove->setToolTip(QStringLiteral("목록에서만 제거합니다. 프로젝트 안의 파일은 보존합니다."));
    editTools->addWidget(rename); editTools->addWidget(reimportButton); editTools->addWidget(remove);
    layout->addLayout(editTools);
    auto* renameId = new QPushButton(QStringLiteral("ID 변경..."), this);
    renameId->setObjectName(QStringLiteral("assetRenameId"));
    renameId->setToolTip(QStringLiteral("객체·배경·소리 이벤트 연결을 함께 변경합니다. Python 코드의 ID 문자열은 직접 수정하세요."));
    layout->addWidget(renameId);
    connect(renameId, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            bool ok = false;
            const auto after = QInputDialog::getText(this, QStringLiteral("에셋 ID 변경"),
                QStringLiteral("새 ID (영문·숫자·_·-, 1~64자)\n객체·배경·소리 이벤트 연결은 함께 바꿉니다.\nPython 코드에 직접 쓴 ID는 직접 수정하세요. 실행 중인 게임은 다시 실행하세요."),
                QLineEdit::Normal, QString::fromStdString(id), &ok);
            if (!ok) return;
            QString error;
            if (!renameAssetId(id, after, &error)) QMessageBox::warning(this, QStringLiteral("ID 변경 실패"), error);
        }
    });
    auto* exportButton = new QPushButton(QStringLiteral("파일 내보내기..."), this);
    exportButton->setObjectName(QStringLiteral("assetExport"));
    exportButton->setToolTip(QStringLiteral("선택한 에셋 파일을 프로젝트 밖에 복사합니다."));
    layout->addWidget(exportButton);
    auto* moveGroupButton = new QPushButton(QStringLiteral("선택한 에셋 그룹 이동..."), this);
    layout->addWidget(moveGroupButton);
    connect(group_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(addGroupButton, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, QStringLiteral("그룹 추가"), QStringLiteral("그룹 이름"), QLineEdit::Normal, {}, &ok);
        if (ok && !addGroup(name)) QMessageBox::warning(this, QStringLiteral("그룹 추가 실패"), QStringLiteral("비어 있지 않은 새 그룹 이름을 입력하세요."));
    });
    connect(renameGroupButton, &QPushButton::clicked, this, [this] {
        if (group_->currentIndex() < 2) {
            QMessageBox::information(this, QStringLiteral("그룹 이름 변경"), QStringLiteral("위 목록에서 변경할 그룹을 먼저 고르세요.")); return;
        }
        const auto before = group_->currentData().toString();
        bool ok = false;
        const auto name = QInputDialog::getText(this, QStringLiteral("그룹 이름 변경"), QStringLiteral("새 그룹 이름"), QLineEdit::Normal, before, &ok);
        if (ok && !renameGroup(before, name)) QMessageBox::warning(this, QStringLiteral("그룹 이름 변경 실패"), QStringLiteral("비어 있지 않은 새 그룹 이름을 입력하세요."));
    });
    connect(removeGroupButton, &QPushButton::clicked, this, [this] {
        if (group_->currentIndex() < 2) {
            QMessageBox::information(this, QStringLiteral("그룹 삭제"), QStringLiteral("위 목록에서 삭제할 그룹을 먼저 고르세요.")); return;
        }
        const auto name = group_->currentData().toString();
        if (QMessageBox::question(this, QStringLiteral("그룹 삭제"),
            QStringLiteral("'%1' 그룹을 삭제할까요? 소속 에셋은 미분류로 이동하며 파일은 보존합니다.").arg(name)) == QMessageBox::Yes) removeGroup(name);
    });
    connect(moveGroupButton, &QPushButton::clicked, this, [this] {
        QStringList ids;
        for (const auto* item : list_->selectedItems()) ids.append(item->data(Qt::UserRole).toString());
        if (ids.isEmpty()) return;
        QStringList choices{QStringLiteral("미분류 (그룹 없음)")};
        for (const auto& name : doc_->project().assetGroups) choices.append(QStringLiteral("그룹: %1").arg(QString::fromStdString(name)));
        bool ok = false;
        const auto chosen = QInputDialog::getItem(this, QStringLiteral("에셋 그룹 이동"), QStringLiteral("이동할 그룹"), choices, 0, false, &ok);
        if (ok) {
            const auto index = choices.indexOf(chosen);
            moveToGroup(ids, index <= 0 ? QString() : QString::fromStdString(doc_->project().assetGroups[index - 1]));
        }
    });
    image_ = new QLabel(this);
    image_->setAlignment(Qt::AlignCenter);
    image_->setMinimumHeight(145);
    previewLayout->addWidget(image_, 1);
    details_ = new QLabel(this);
    details_->setTextFormat(Qt::PlainText);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(details_);
    auto* audioTools = new QHBoxLayout;
    play_ = new QPushButton(QStringLiteral("미리듣기"), this);
    play_->setObjectName(QStringLiteral("assetPlay"));
    auto* stop = new QPushButton(QStringLiteral("정지"), this);
    audioTools->addWidget(play_); audioTools->addWidget(stop);
    previewLayout->addLayout(audioTools);
    playback_ = new QLabel(this);
    playback_->setObjectName(QStringLiteral("assetPlayback"));
    playback_->setWordWrap(true);
    previewLayout->addWidget(playback_);
    connect(add, &QPushButton::clicked, this, [this] { showImportDialog(); });
    connect(reload, &QPushButton::clicked, this, &AssetBrowser::refresh);
    connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { preview(); });
    connect(doc_, &ProjectDocument::assetsChanged, this, &AssetBrowser::refresh);
    connect(doc_, &ProjectDocument::objectChanged, this, [this] { preview(); });
    connect(doc_, &ProjectDocument::sceneChanged, this, [this] { preview(); });
    connect(rename, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            bool ok = false;
            auto name = QInputDialog::getText(this, QStringLiteral("에셋 이름"), QStringLiteral("표시 이름"),
                QLineEdit::Normal, QString::fromStdString(a->name), &ok);
            if (ok && !renameAsset(id, name)) QMessageBox::warning(this, QStringLiteral("에셋 이름"), QStringLiteral("이름을 입력하세요."));
        }
    });
    connect(reimportButton, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            const auto source = QFileDialog::getOpenFileName(this, QStringLiteral("교체할 파일 선택 (ID·이름 유지)"), {}, kFilter);
            QString error;
            if (!source.isEmpty() && !reimport(id, source, &error))
                QMessageBox::warning(this, QStringLiteral("파일 교체 실패"), error);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            const auto uses = naru::imageUses(doc_->project(), id);
            if (!uses.empty()) {
                QStringList locations;
                for (const auto& use : uses) locations.append(QString::fromStdString(use));
                QMessageBox::information(this, QStringLiteral("사용 중인 에셋"),
                    QStringLiteral("이미지 설정 또는 소리 이벤트에서 먼저 연결을 지우세요.\n") + locations.join('\n'));
                return;
            }
            if (QMessageBox::question(this, QStringLiteral("목록에서 제거"),
                QStringLiteral("목록에서 제거할까요? 프로젝트 안의 파일은 보존하며 실행 취소할 수 있습니다.")) == QMessageBox::Yes)
                unregisterAsset(id);
        }
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            const auto suffix = QFileInfo(QString::fromStdString(a->path)).suffix();
            const auto destination = QFileDialog::getSaveFileName(this, QStringLiteral("파일 내보내기 (프로젝트 밖에 복사)"),
                QString::fromStdString(a->sourceName), QStringLiteral("에셋 파일 (*.%1)").arg(suffix));
            if (destination.isEmpty()) return;
            QString error;
            if (!exportFile(id, destination, &error, true))
                QMessageBox::warning(this, QStringLiteral("파일 내보내기 실패"), error);
        }
    });
    connect(play_, &QPushButton::clicked, this, &AssetBrowser::playAudio);
    connect(stop, &QPushButton::clicked, this, &AssetBrowser::stopAudio);
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (audio_ && SDL_GetAudioStreamQueued(audio_) == 0) stopAudio();
    });
    timer->start(300);
    refresh();
}

AssetBrowser::~AssetBrowser() {
    stopAudio();
    if (audioInitialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if (fontId_ >= 0) QFontDatabase::removeApplicationFont(fontId_);
}

void AssetBrowser::showImportDialog(const QStringList& initialFiles) {
    stopAudio();
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("assetImportDialog"));
    dialog.setWindowTitle(QStringLiteral("에셋 추가"));
    dialog.resize(820, 460);
    auto* layout = new QVBoxLayout(&dialog);
    auto* help = new QLabel(QStringLiteral("파일을 선택한 뒤 오른쪽 칸에 ID를 입력하세요 (예: day, night, hero_walk).\n영문·숫자·_·- 1~64자, 프로젝트 안에서 중복 불가. 비우면 자동 생성합니다.\n추가 전 취소하면 프로젝트는 바뀌지 않습니다."), &dialog);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto* choose = new QPushButton(QStringLiteral("파일 선택..."), &dialog);
    choose->setObjectName(QStringLiteral("assetChooseFiles"));
    layout->addWidget(choose);
    auto* files = new QTableWidget(0, 2, &dialog);
    files->setObjectName(QStringLiteral("assetImportFiles"));
    files->setHorizontalHeaderLabels({QStringLiteral("파일"), QStringLiteral("에셋 ID (비우면 자동 생성)")});
    files->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    files->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(files, 1);
    auto* validation = new QLabel(&dialog);
    validation->setObjectName(QStringLiteral("assetIdValidation"));
    validation->setTextFormat(Qt::PlainText); validation->setWordWrap(true);
    layout->addWidget(validation);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    auto* submit = buttons->button(QDialogButtonBox::Ok);
    submit->setText(QStringLiteral("추가"));
    submit->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
    layout->addWidget(buttons);
    const auto readIds = [&] {
        QStringList ids;
        for (int row = 0; row < files->rowCount(); ++row) ids.append(files->item(row, 1)->text());
        return ids;
    };
    const auto validate = [&] {
        const auto error = newAssetIdsError(readIds());
        validation->setText(error);
        submit->setEnabled(files->rowCount() > 0 && error.isEmpty());
    };
    const auto setFiles = [&](const QStringList& paths) {
        files->blockSignals(true);
        files->setRowCount(0);
        for (const auto& path : paths) {
            const auto row = files->rowCount(); files->insertRow(row);
            auto* name = new QTableWidgetItem(QFileInfo(path).fileName());
            name->setData(Qt::UserRole, path); name->setToolTip(path);
            name->setFlags(name->flags() & ~Qt::ItemIsEditable);
            auto* id = new QTableWidgetItem;
            id->setToolTip(QStringLiteral("더블클릭해 ID를 입력하세요. 비워 두면 자동 생성합니다."));
            files->setItem(row, 0, name); files->setItem(row, 1, id);
        }
        files->blockSignals(false); validate();
    };
    connect(files, &QTableWidget::cellChanged, &dialog, [&](int, int) { validate(); });
    connect(choose, &QPushButton::clicked, &dialog, [&] {
        const auto paths = QFileDialog::getOpenFileNames(&dialog, QStringLiteral("추가할 파일 선택"), {}, kFilter);
        if (paths.isEmpty()) return;
        setFiles(paths);
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        validate();
        if (submit->isEnabled()) dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    setFiles(initialFiles);
    if (dialog.exec() != QDialog::Accepted) return;
    QStringList paths;
    for (int row = 0; row < files->rowCount(); ++row) paths.append(files->item(row, 0)->data(Qt::UserRole).toString());
    const auto errors = importFiles(paths, readIds());
    if (!errors.empty()) QMessageBox::warning(this, QStringLiteral("추가 결과"), errors.join('\n'));
}

QString AssetBrowser::checkedPath(const naru::Asset& asset) const {
    auto directory = std::filesystem::path(doc_->directory().toStdWString());
    if (!naru::validateAssets({asset}, directory).empty()) return {};
    return QDir(doc_->directory()).filePath(QString::fromStdString(asset.path));
}

QString AssetBrowser::assetState(const naru::Asset& a) const {
    auto path = checkedPath(a);
    if (path.isEmpty()) return QStringLiteral("경로 오류");
    QFile file(path);
    if (!file.exists()) return QStringLiteral("파일 없음");
    if (file.size() <= 0 || file.size() > kMaxBytes || !file.open(QIODevice::ReadOnly)) return QStringLiteral("읽기 오류");
    if (hash(file.readAll()).toStdString() != a.sha256) return QStringLiteral("파일 변경됨");
    return QStringLiteral("정상");
}

const naru::Asset* AssetBrowser::selectedAsset() const {
    const auto* item = list_->currentItem();
    if (!item) return nullptr;
    const auto id = item->data(Qt::UserRole).toString().toStdString();
    for (const auto& a : doc_->project().assets) if (a.id == id) return &a;
    return nullptr;
}

void AssetBrowser::refresh() {
    const auto* selected = selectedAsset();
    const std::string id = selected ? selected->id : std::string();
    const auto groupName = group_->currentData().toString();
    const bool allGroups = group_->currentIndex() <= 0;
    group_->blockSignals(true);
    group_->clear();
    group_->addItem(QStringLiteral("모든 그룹"));
    group_->addItem(QStringLiteral("미분류"), QString());
    for (const auto& name : doc_->project().assetGroups) group_->addItem(QStringLiteral("그룹: %1").arg(QString::fromStdString(name)), QString::fromStdString(name));
    const auto groupIndex = allGroups ? 0 : group_->findData(groupName);
    group_->setCurrentIndex(groupIndex < 0 ? 0 : groupIndex);
    group_->blockSignals(false);
    const auto filterGroup = group_->currentData().toString().toStdString();
    const auto query = search_->text().trimmed();
    const auto kind = kind_->currentData().toString().toStdString();
    list_->blockSignals(true);
    list_->clear();
    int row = 0, selection = -1;
    for (const auto& a : doc_->project().assets) {
        if (group_->currentIndex() > 0 && a.group != filterGroup) continue;
        if (!kind.empty() && kind != a.kind) continue;
        if (!QString::fromStdString(a.name + " " + a.id + " " + a.sourceName).contains(query, Qt::CaseInsensitive)) continue;
        const auto state = assetState(a);
        auto* item = new QListWidgetItem(QStringLiteral("%1  [%2] — %3")
            .arg(QString::fromStdString(a.name), kindName(a.kind), state), list_);
        if (a.kind == "image" && state == QStringLiteral("정상")) {
            QImageReader reader(checkedPath(a));
            const auto size = reader.size();
            if (size.isValid() && qint64(size.width()) * size.height() <= 32 * 1024 * 1024) {
                reader.setScaledSize(size.scaled(QSize(40, 40), Qt::KeepAspectRatio));
                item->setIcon(QPixmap::fromImage(reader.read()));
            }
        }
        item->setData(Qt::UserRole, QString::fromStdString(a.id));
        item->setToolTip(QStringLiteral("그룹: %1").arg(a.group.empty() ? QStringLiteral("미분류") : QString::fromStdString(a.group)));
        if (a.id == id) selection = row;
        ++row;
    }
    list_->setCurrentRow(selection >= 0 ? selection : (row ? 0 : -1));
    list_->blockSignals(false);
    preview();
}

void AssetBrowser::commit(std::vector<naru::Asset> after, const QString& text) {
    commit(std::move(after), doc_->project().assetGroups, text);
}

void AssetBrowser::commit(std::vector<naru::Asset> after, std::vector<std::string> groups, const QString& text) {
    doc_->undoStack()->push(new AssetCommand(doc_, std::move(after), std::move(groups), text));
}

bool AssetBrowser::addGroup(const QString& name) {
    auto groups = doc_->project().assetGroups;
    groups.push_back(name.trimmed().toStdString());
    if (!naru::validateAssetGroups(groups, doc_->project().assets).empty()) return false;
    commit(doc_->project().assets, std::move(groups), QStringLiteral("에셋 그룹 추가"));
    group_->setCurrentIndex(group_->findData(name.trimmed()));
    return true;
}

bool AssetBrowser::renameGroup(const QString& before, const QString& after) {
    auto groups = doc_->project().assetGroups;
    auto it = std::find(groups.begin(), groups.end(), before.toStdString());
    if (it == groups.end()) return false;
    *it = after.trimmed().toStdString();
    auto assets = doc_->project().assets;
    for (auto& a : assets) if (a.group == before.toStdString()) a.group = *it;
    if (!naru::validateAssetGroups(groups, assets).empty()) return false;
    if (before == after.trimmed()) return true;
    commit(std::move(assets), std::move(groups), QStringLiteral("에셋 그룹 이름 변경"));
    group_->setCurrentIndex(group_->findData(after.trimmed()));
    return true;
}

bool AssetBrowser::removeGroup(const QString& name) {
    auto groups = doc_->project().assetGroups;
    if (!std::erase(groups, name.toStdString())) return false;
    auto assets = doc_->project().assets;
    for (auto& a : assets) if (a.group == name.toStdString()) a.group.clear();
    commit(std::move(assets), std::move(groups), QStringLiteral("에셋 그룹 삭제"));
    return true;
}

bool AssetBrowser::moveToGroup(const QStringList& ids, const QString& group) {
    auto assets = doc_->project().assets;
    for (const auto& id : ids) {
        auto it = std::find_if(assets.begin(), assets.end(), [&](const auto& a) { return a.id == id.toStdString(); });
        if (it == assets.end()) return false;
        it->group = group.toStdString();
    }
    if (!naru::validateAssetGroups(doc_->project().assetGroups, assets).empty()) return false;
    if (assets != doc_->project().assets) commit(std::move(assets), QStringLiteral("에셋 그룹 이동"));
    return true;
}

bool AssetBrowser::prepare(const QString& source, naru::Asset& asset, QString* error) {
    QFile file(source);
    if (file.size() <= 0 || file.size() > kMaxBytes || !file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("파일을 읽을 수 없거나 64 MiB를 넘습니다."); return false;
    }
    const auto bytes = file.readAll();
    if (bytes.size() != file.size() || bytes.size() > kMaxBytes) { *error = QStringLiteral("원본을 끝까지 읽지 못했습니다."); return false; }
    const auto suffix = QFileInfo(source).suffix().toLower();
    std::string kind;
    if (!decode(bytes, suffix, kind, error)) return false;
    if (!asset.kind.empty() && asset.kind != kind) { *error = QStringLiteral("파일 교체는 같은 종류의 파일을 선택하세요."); return false; }
    asset.kind = kind;
    asset.sourceName = QFileInfo(source).fileName().toStdString();
    asset.sha256 = hash(bytes).toStdString();
    // Immutable copies preserve both Undo and the last saved project after reimport.
    const auto token = QUuid::createUuid().toString(QUuid::Id128);
    asset.path = (QStringLiteral("assets/") + token + "." + suffix).toStdString();
    auto path = checkedPath(asset);
    if (path.isEmpty() || !QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("프로젝트 내부 에셋 경로를 만들 수 없습니다: ") +
            QString::fromStdString(naru::validateAssets({asset}, std::filesystem::path(doc_->directory().toStdWString())));
        return false;
    }
    QSaveFile target(path);
    if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size() || !target.commit()) {
        *error = QStringLiteral("에셋 복사 실패: ") + target.errorString(); return false;
    }
    return true;
}

QString AssetBrowser::newAssetIdsError(const QStringList& ids) const {
    std::set<std::string> used;
    for (const auto& a : doc_->project().assets) used.insert(a.id);
    for (int row = 0; row < ids.size(); ++row) {
        const auto id = ids[row].trimmed().toStdString();
        if (id.empty()) continue;
        if (!naru::isValidAssetId(id)) return QStringLiteral("%1행: ID는 영문·숫자·_·-만 사용해 1~64자로 입력하세요.").arg(row + 1);
        if (!used.insert(id).second) return QStringLiteral("%1행: 이미 사용 중이거나 선택한 파일끼리 중복된 ID입니다: %2").arg(row + 1).arg(QString::fromStdString(id));
    }
    return {};
}

QStringList AssetBrowser::importFiles(const QStringList& files, const QStringList& ids) {
    QStringList errors;
    if (!ids.isEmpty() && ids.size() != files.size()) return {QStringLiteral("파일과 ID 개수가 다릅니다.")};
    const auto validation = newAssetIdsError(ids);
    if (!validation.isEmpty()) return {validation};
    std::set<std::string> used;
    for (const auto& a : doc_->project().assets) used.insert(a.id);
    for (const auto& id : ids) if (!id.trimmed().isEmpty()) used.insert(id.trimmed().toStdString());
    auto after = doc_->project().assets;
    for (int row = 0; row < files.size(); ++row) {
        const auto& file = files[row];
        naru::Asset asset;
        asset.id = ids.isEmpty() ? std::string() : ids[row].trimmed().toStdString();
        if (asset.id.empty()) {
            do { asset.id = (QStringLiteral("asset_") + QUuid::createUuid().toString(QUuid::Id128)).toStdString(); } while (used.contains(asset.id));
            used.insert(asset.id);
        }
        asset.name = QFileInfo(file).completeBaseName().toStdString();
        if (group_->currentIndex() >= 2) asset.group = group_->currentData().toString().toStdString();
        QString error;
        if (prepare(file, asset, &error)) after.push_back(std::move(asset));
        else errors << QFileInfo(file).fileName() + QStringLiteral(": ") + error;
    }
    if (after != doc_->project().assets) commit(std::move(after), QStringLiteral("에셋 추가"));
    return errors;
}

bool AssetBrowser::renameAssetId(const std::string& before, const QString& requested, QString* error) {
    error->clear();
    const auto after = requested.trimmed().toStdString();
    if (!naru::isValidAssetId(after)) { *error = QStringLiteral("ID는 영문·숫자·_·-만 사용해 1~64자로 입력하세요."); return false; }
    bool found = false;
    for (const auto& a : doc_->project().assets) {
        if (a.id == before) found = true;
        else if (a.id == after) { *error = QStringLiteral("이미 사용 중인 ID입니다: ") + QString::fromStdString(after); return false; }
    }
    if (!found) { *error = QStringLiteral("에셋이 없습니다."); return false; }
    if (before == after) return true;
    doc_->undoStack()->push(new AssetIdCommand(doc_, before, after));
    for (int row = 0; row < list_->count(); ++row) if (list_->item(row)->data(Qt::UserRole).toString().toStdString() == after) list_->setCurrentRow(row);
    return true;
}

bool AssetBrowser::reimport(const std::string& id, const QString& source, QString* error) {
    auto after = doc_->project().assets;
    for (auto& a : after) if (a.id == id) {
        if (!prepare(source, a, error)) return false;
        commit(std::move(after), QStringLiteral("에셋 파일 교체")); return true;
    }
    *error = QStringLiteral("에셋이 없습니다."); return false;
}

bool AssetBrowser::exportFile(const std::string& id, const QString& destination, QString* error, bool overwrite) {
    error->clear();
    if (destination.isEmpty()) return false;
    const auto& assets = doc_->project().assets;
    const auto it = std::find_if(assets.begin(), assets.end(), [&](const auto& a) { return a.id == id; });
    if (it == assets.end()) { *error = QStringLiteral("에셋이 없습니다."); return false; }
    QFile source(checkedPath(*it));
    if (source.fileName().isEmpty() || source.size() <= 0 || source.size() > kMaxBytes || !source.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("원본 파일이 없거나 읽을 수 없습니다. 파일 교체로 복구하세요."); return false;
    }
    const auto bytes = source.readAll();
    if (bytes.size() != source.size() || hash(bytes).toStdString() != it->sha256) {
        *error = QStringLiteral("프로젝트 파일이 변경되었습니다. 파일 교체로 확인한 뒤 내보내세요."); return false;
    }
    const QFileInfo target(destination);
    const auto parent = target.dir().canonicalPath();
    const auto base = QDir(doc_->directory()).canonicalPath();
    const auto resolved = QDir(parent).filePath(target.fileName());
    const auto relative = QDir(base).relativeFilePath(resolved);
    if (parent.isEmpty() || base.isEmpty() || target.isSymLink() ||
        (!QDir::isAbsolutePath(relative) && relative != ".." && !relative.startsWith("../"))) {
        *error = QStringLiteral("프로젝트 밖의 정상 폴더를 선택하세요. 프로젝트 파일은 덮어쓸 수 없습니다."); return false;
    }
    if (target.exists() && !overwrite) {
        *error = QStringLiteral("같은 이름의 파일이 있습니다. 덮어쓰기를 확인하거나 다른 이름을 선택하세요."); return false;
    }
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        *error = QStringLiteral("파일을 내보내지 못했습니다: ") + output.errorString(); return false;
    }
    return true;
}

bool AssetBrowser::renameAsset(const std::string& id, const QString& name) {
    if (name.trimmed().isEmpty()) return false;
    auto after = doc_->project().assets;
    for (auto& a : after) if (a.id == id) {
        if (a.name != name.trimmed().toStdString()) {
            a.name = name.trimmed().toStdString(); commit(std::move(after), QStringLiteral("에셋 이름 변경"));
        }
        return true;
    }
    return false;
}

bool AssetBrowser::unregisterAsset(const std::string& id) {
    if (!naru::imageUses(doc_->project(), id).empty()) return false;
    auto after = doc_->project().assets;
    const auto removed = std::erase_if(after, [&](const naru::Asset& a) { return a.id == id; });
    if (removed) commit(std::move(after), QStringLiteral("에셋 목록에서 제거"));
    return removed != 0;
}

void AssetBrowser::preview() {
    stopAudio();
    if (fontId_ >= 0) { QFontDatabase::removeApplicationFont(fontId_); fontId_ = -1; }
    image_->clear(); image_->setFont(font());
    details_->clear(); play_->setEnabled(false);
    const auto* a = selectedAsset();
    if (!a) { image_->setText(QStringLiteral("에셋을 추가하거나 목록에서 선택하세요.")); return; }
    const auto state = assetState(*a);
    details_->setToolTip(QStringLiteral("그룹: %1").arg(a->group.empty() ? QStringLiteral("미분류") : QString::fromStdString(a->group)));
    details_->setText(QStringLiteral("ID: %1\n파일: %2\n원본 이름: %3\n상태: %4")
        .arg(QString::fromStdString(a->id), QString::fromStdString(a->path), QString::fromStdString(a->sourceName), state));
    details_->setText(details_->text() + QStringLiteral("\n그룹: %1").arg(a->group.empty() ? QStringLiteral("미분류") : QString::fromStdString(a->group)));
    const auto uses = naru::imageUses(doc_->project(), a->id);
    details_->setText(details_->text() + QStringLiteral("\n사용처: %1").arg(uses.size()));
    for (const auto& use : uses) details_->setText(details_->text() + "\n" + QString::fromStdString(use));
    if (state != QStringLiteral("정상")) {
        image_->setText(QStringLiteral("파일을 확인한 뒤 파일 교체를 사용하세요.")); return;
    }
    auto path = checkedPath(*a);
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) return;
    auto bytes = input.readAll();
    std::string kind;
    QString error;
    if (!decode(bytes, QFileInfo(path).suffix(), kind, &error) || kind != a->kind) {
        image_->setText(error.isEmpty() ? QStringLiteral("에셋 종류가 일치하지 않습니다.") : error); return;
    }
    if (kind == "image") {
        auto image = QImage::fromData(bytes);
        const auto scaled = image.scaled(QSize(320, 145), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPixmap background(320, 145);
        QPainter painter(&background);
        for (int y = 0; y < 145; y += 12) for (int x = 0; x < 320; x += 12)
            painter.fillRect(x, y, 12, 12, ((x / 12 + y / 12) % 2) ? QColor("#b8bec6") : QColor("#e7ebef"));
        painter.drawImage((320 - scaled.width()) / 2, (145 - scaled.height()) / 2, scaled);
        painter.end();
        image_->setPixmap(background);
        details_->setText(details_->text() + QStringLiteral("\n크기: %1 × %2 · %3 바이트 · 알파 %4")
            .arg(image.width()).arg(image.height()).arg(bytes.size()).arg(image.hasAlphaChannel() ? QStringLiteral("있음") : QStringLiteral("없음")));
    } else if (kind == "font") {
        fontId_ = QFontDatabase::addApplicationFontFromData(bytes);
        const auto families = QFontDatabase::applicationFontFamilies(fontId_);
        if (!families.isEmpty()) image_->setFont(QFont(families.front(), 20));
        image_->setText(QStringLiteral("가나다 Naru Studio\nABC abc 0123456789"));
    } else {
        image_->setText(QStringLiteral("WAV/MP3 소리\n미리듣기를 눌러 확인하세요."));
        play_->setEnabled(true);
    }
}

void AssetBrowser::stopAudio() {
    if (audio_) { SDL_DestroyAudioStream(audio_); audio_ = nullptr; }
    playback_->clear();
}

void AssetBrowser::playAudio() {
    stopAudio();
    const auto* a = selectedAsset();
    if (!a || a->kind != "audio" || assetState(*a) != QStringLiteral("정상")) return;
    if (!audioInitialized_) audioInitialized_ = SDL_InitSubSystem(SDL_INIT_AUDIO);
    QFile input(checkedPath(*a));
    if (!input.open(QIODevice::ReadOnly) || input.size() > static_cast<qint64>(naru::kMaxAudioBytes)) return;
    auto bytes = input.readAll(); naru::DecodedAudio decoded;
    auto message = naru::decodeAudio(bytes.constData(), bytes.size(), QFileInfo(input.fileName()).suffix().toLower().toStdString(), decoded);
    if (!message.empty()) { playback_->setText(QString::fromStdString(message)); return; }
    if (audioInitialized_) audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &decoded.spec, nullptr, nullptr);
    bool ok = audio_ && SDL_PutAudioStreamData(audio_, decoded.samples.data(), static_cast<int>(decoded.samples.size() * sizeof(float))) &&
        SDL_FlushAudioStream(audio_) && SDL_ResumeAudioStreamDevice(audio_);
    if (!ok) {
        const auto error = QString::fromUtf8(SDL_GetError());
        stopAudio(); playback_->setText(QStringLiteral("미리듣기 실패: ") + error);
    } else playback_->setText(QStringLiteral("재생 중 — 선택 변경·정지·창 종료 시 중지"));
}
