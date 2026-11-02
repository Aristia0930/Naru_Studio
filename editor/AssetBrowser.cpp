#include "AssetBrowser.h"
#include "ProjectDocument.h"

#include <SDL3/SDL.h>
#include <QBuffer>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
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
#include <QTimer>
#include <QUndoCommand>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace {
constexpr qint64 kMaxBytes = 64 * 1024 * 1024;
const QString kFilter = QStringLiteral("지원 에셋 (*.png *.jpg *.jpeg *.bmp *.wav *.ttf *.otf)");

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
    if (suffix == "wav") {
        kind = "audio";
        SDL_AudioSpec spec{};
        Uint8* samples = nullptr;
        Uint32 length = 0;
        if (bytes.size() < 44 || bytes.mid(0, 4) != "RIFF" || bytes.mid(8, 4) != "WAVE") {
            *error = QStringLiteral("WAV 파일이 손상되었습니다.");
            return false;
        }
        bool ok = SDL_LoadWAV_IO(SDL_IOFromConstMem(bytes.constData(), bytes.size()), true, &spec, &samples, &length);
        SDL_free(samples);
        if (!ok || !length || spec.freq <= 0 || spec.channels <= 0) {
            *error = QStringLiteral("읽을 수 없는 WAV입니다: ") + QString::fromUtf8(SDL_GetError());
            return false;
        }
        return true;
    }
    if (suffix == "ttf" || suffix == "otf") {
        kind = "font";
        if (QRawFont(bytes, 24).isValid()) return true;
        *error = QStringLiteral("글꼴 파일을 읽을 수 없습니다.");
        return false;
    }
    *error = QStringLiteral("지원 형식: PNG/JPG/BMP, WAV, TTF/OTF.");
    return false;
}

class AssetCommand : public QUndoCommand {
public:
    AssetCommand(ProjectDocument* doc, std::vector<naru::Asset> after, const QString& text)
        : QUndoCommand(text), doc_(doc), before_(doc->project().assets), after_(std::move(after)) {}
    void redo() override { doc_->setAssets(after_); }
    void undo() override { doc_->setAssets(before_); }
private:
    ProjectDocument* doc_;
    std::vector<naru::Asset> before_, after_;
};
}

AssetBrowser::AssetBrowser(ProjectDocument* document, QWidget* parent) : QWidget(parent), doc_(document) {
    setObjectName(QStringLiteral("assetBrowser"));
    auto* layout = new QVBoxLayout(this);
    auto* tools = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("가져오기..."), this);
    auto* reload = new QPushButton(QStringLiteral("새로 고침"), this);
    tools->addWidget(add); tools->addWidget(reload);
    layout->addLayout(tools);
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("assetSearch"));
    search_->setPlaceholderText(QStringLiteral("이름·ID·원본 파일명 검색"));
    kind_ = new QComboBox(this);
    kind_->addItem(QStringLiteral("모든 종류"), QString());
    kind_->addItem(QStringLiteral("이미지"), "image");
    kind_->addItem(QStringLiteral("소리 (WAV)"), "audio");
    kind_->addItem(QStringLiteral("글꼴"), "font");
    layout->addWidget(search_); layout->addWidget(kind_);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("assetList"));
    list_->setIconSize(QSize(40, 40));
    layout->addWidget(list_, 1);
    auto* editTools = new QHBoxLayout;
    auto* rename = new QPushButton(QStringLiteral("이름 변경"), this);
    auto* reimportButton = new QPushButton(QStringLiteral("재가져오기..."), this);
    auto* remove = new QPushButton(QStringLiteral("등록 해제"), this);
    editTools->addWidget(rename); editTools->addWidget(reimportButton); editTools->addWidget(remove);
    layout->addLayout(editTools);
    image_ = new QLabel(this);
    image_->setAlignment(Qt::AlignCenter);
    image_->setMinimumHeight(145);
    layout->addWidget(image_);
    details_ = new QLabel(this);
    details_->setTextFormat(Qt::PlainText);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(details_);
    auto* audioTools = new QHBoxLayout;
    play_ = new QPushButton(QStringLiteral("미리듣기"), this);
    play_->setObjectName(QStringLiteral("assetPlay"));
    auto* stop = new QPushButton(QStringLiteral("정지"), this);
    audioTools->addWidget(play_); audioTools->addWidget(stop);
    layout->addLayout(audioTools);
    playback_ = new QLabel(this);
    playback_->setObjectName(QStringLiteral("assetPlayback"));
    playback_->setWordWrap(true);
    layout->addWidget(playback_);
    connect(add, &QPushButton::clicked, this, [this] {
        const auto paths = QFileDialog::getOpenFileNames(this, QStringLiteral("에셋 가져오기"), {}, kFilter);
        const auto errors = importFiles(paths);
        if (!errors.empty()) QMessageBox::warning(this, QStringLiteral("가져오기 결과"), errors.join('\n'));
    });
    connect(reload, &QPushButton::clicked, this, &AssetBrowser::refresh);
    connect(search_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { preview(); });
    connect(doc_, &ProjectDocument::assetsChanged, this, &AssetBrowser::refresh);
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
            const auto source = QFileDialog::getOpenFileName(this, QStringLiteral("새 원본 선택 (ID 유지)"), {}, kFilter);
            QString error;
            if (!source.isEmpty() && !reimport(id, source, &error))
                QMessageBox::warning(this, QStringLiteral("재가져오기 실패"), error);
        }
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (const auto* a = selectedAsset()) {
            const auto id = a->id;
            if (QMessageBox::question(this, QStringLiteral("등록 해제"),
                QStringLiteral("목록에서 해제할까요? 파일은 보존하며 실행 취소할 수 있습니다.")) == QMessageBox::Yes)
                unregisterAsset(id);
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
    const auto query = search_->text().trimmed();
    const auto kind = kind_->currentData().toString().toStdString();
    list_->blockSignals(true);
    list_->clear();
    int row = 0, selection = -1;
    for (const auto& a : doc_->project().assets) {
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
        if (a.id == id) selection = row;
        ++row;
    }
    list_->setCurrentRow(selection >= 0 ? selection : (row ? 0 : -1));
    list_->blockSignals(false);
    preview();
}

void AssetBrowser::commit(std::vector<naru::Asset> after, const QString& text) {
    doc_->undoStack()->push(new AssetCommand(doc_, std::move(after), text));
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
    if (!asset.kind.empty() && asset.kind != kind) { *error = QStringLiteral("재가져오기는 같은 종류의 파일을 선택하세요."); return false; }
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

QStringList AssetBrowser::importFiles(const QStringList& files) {
    QStringList errors;
    auto after = doc_->project().assets;
    for (const auto& file : files) {
        naru::Asset asset;
        asset.id = (QStringLiteral("asset_") + QUuid::createUuid().toString(QUuid::Id128)).toStdString();
        asset.name = QFileInfo(file).completeBaseName().toStdString();
        QString error;
        if (prepare(file, asset, &error)) after.push_back(std::move(asset));
        else errors << QFileInfo(file).fileName() + QStringLiteral(": ") + error;
    }
    if (after != doc_->project().assets) commit(std::move(after), QStringLiteral("에셋 가져오기"));
    return errors;
}

bool AssetBrowser::reimport(const std::string& id, const QString& source, QString* error) {
    auto after = doc_->project().assets;
    for (auto& a : after) if (a.id == id) {
        if (!prepare(source, a, error)) return false;
        commit(std::move(after), QStringLiteral("에셋 재가져오기")); return true;
    }
    *error = QStringLiteral("에셋이 없습니다."); return false;
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
    auto after = doc_->project().assets;
    const auto removed = std::erase_if(after, [&](const naru::Asset& a) { return a.id == id; });
    if (removed) commit(std::move(after), QStringLiteral("에셋 등록 해제"));
    return removed != 0;
}

void AssetBrowser::preview() {
    stopAudio();
    if (fontId_ >= 0) { QFontDatabase::removeApplicationFont(fontId_); fontId_ = -1; }
    image_->clear(); image_->setFont(font());
    details_->clear(); play_->setEnabled(false);
    const auto* a = selectedAsset();
    if (!a) { image_->setText(QStringLiteral("에셋을 가져오거나 목록에서 선택하세요.")); return; }
    const auto state = assetState(*a);
    details_->setText(QStringLiteral("ID: %1\n파일: %2\n원본 이름: %3\n상태: %4")
        .arg(QString::fromStdString(a->id), QString::fromStdString(a->path), QString::fromStdString(a->sourceName), state));
    if (state != QStringLiteral("정상")) {
        image_->setText(QStringLiteral("파일을 확인한 뒤 재가져오기하세요.")); return;
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
        image_->setText(QStringLiteral("WAV 소리\n미리듣기를 눌러 확인하세요."));
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
    SDL_AudioSpec spec{};
    Uint8* samples = nullptr;
    Uint32 length = 0;
    const auto path = checkedPath(*a).toUtf8();
    bool ok = audioInitialized_ && SDL_LoadWAV(path.constData(), &spec, &samples, &length);
    if (ok) audio_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    ok = ok && audio_ && length <= static_cast<Uint32>(std::numeric_limits<int>::max()) &&
        SDL_PutAudioStreamData(audio_, samples, static_cast<int>(length)) &&
        SDL_FlushAudioStream(audio_) && SDL_ResumeAudioStreamDevice(audio_);
    SDL_free(samples);
    if (!ok) {
        const auto error = QString::fromUtf8(SDL_GetError());
        stopAudio(); playback_->setText(QStringLiteral("미리듣기 실패: ") + error);
    } else playback_->setText(QStringLiteral("재생 중 — 선택 변경·정지·창 종료 시 중지"));
}
