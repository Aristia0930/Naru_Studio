#include "InputSettingsDialog.h"
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSignalBlocker>
#include <QFontDatabase>
#include <algorithm>

namespace {
QString actionLabel(const std::string& name) {
    if (name == "up") return QStringLiteral("위");
    if (name == "down") return QStringLiteral("아래");
    if (name == "left") return QStringLiteral("왼쪽");
    if (name == "right") return QStringLiteral("오른쪽");
    if (name == "confirm") return QStringLiteral("결정·조사");
    if (name == "cancel") return QStringLiteral("취소");
    return QStringLiteral("사용자 동작");
}
}

KeyCombinationDialog::KeyCombinationDialog(const std::vector<std::string>& selected, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("동시에 누를 키 조합"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("함께 누를 키를 2~4개 선택하세요. 누른 순서와 관계없이 모두 눌린 동안 동작합니다.\n예: Shift + E, W + D, Ctrl + Shift + E"), this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto* filter = new QLineEdit(this);
    filter->setPlaceholderText(QStringLiteral("키 이름 검색"));
    layout->addWidget(filter);
    keys_ = new QListWidget(this);
    keys_->setObjectName(QStringLiteral("combinationKeys"));
    keys_->setSelectionMode(QAbstractItemView::NoSelection);
    for (const auto& name : naru::InputMap::keyNames()) {
        auto* item = new QListWidgetItem(QString::fromStdString(name), keys_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(std::find(selected.begin(), selected.end(), name) != selected.end() ? Qt::Checked : Qt::Unchecked);
    }
    layout->addWidget(keys_, 1);
    summary_ = new QLabel(this);
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->setObjectName(QStringLiteral("combinationButtons"));
    layout->addWidget(buttons_);
    connect(filter, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (int i = 0; i < keys_->count(); ++i) keys_->item(i)->setHidden(!keys_->item(i)->text().contains(text, Qt::CaseInsensitive));
    });
    connect(keys_, &QListWidget::itemChanged, this, [this] { validate(); });
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    validate();
    resize(460, 540);
}

std::vector<std::string> KeyCombinationDialog::keys() const {
    std::vector<std::string> result;
    for (int i = 0; i < keys_->count(); ++i)
        if (keys_->item(i)->checkState() == Qt::Checked) result.push_back(keys_->item(i)->text().toStdString());
    return result;
}

void KeyCombinationDialog::validate() {
    auto selected = keys();
    std::string combination;
    for (const auto& key : selected) { if (!combination.empty()) combination += " + "; combination += key; }
    auto canonical = naru::InputMap::canonicalBinding(combination);
    bool valid = selected.size() >= 2 && selected.size() <= 4 && canonical.has_value();
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(valid);
    summary_->setText(valid ? QStringLiteral("동시 입력: %1").arg(QString::fromStdString(*canonical))
        : QStringLiteral("서로 겹치지 않는 키 2~4개를 선택하세요. Shift와 Left Shift처럼 범위가 겹치는 선택은 사용할 수 없습니다."));
}

InputSettingsDialog::InputSettingsDialog(const std::string& source, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("키 설정 · Python과 함께 편집"));
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("왼쪽 설정과 오른쪽 Python 코드는 같은 값입니다. 어느 쪽을 바꿔도 바로 함께 바뀝니다.\n"
        "확인을 누르면 프로젝트에 적용됩니다. Ctrl+S로 저장하고 게임을 다시 실행하세요. 실행 중인 게임은 실행 당시 설정을 사용합니다."), this);
    help->setWordWrap(true);
    layout->addWidget(help);
    auto* split = new QSplitter(this);
    form_ = new QWidget(split);
    auto* formLayout = new QVBoxLayout(form_);
    enabled_ = new QCheckBox(QStringLiteral("엔진 기본 이동·조사 ON"), form_);
    enabled_->setObjectName(QStringLiteral("inputDefaultControl"));
    enabled_->setToolTip(QStringLiteral("끄면 엔진의 기본 이동·조사를 멈춥니다. 대사·선택지 조작과 Python 입력 읽기는 계속됩니다."));
    formLayout->addWidget(enabled_);
    auto* speedRow = new QFormLayout;
    speed_ = new QDoubleSpinBox(form_);
    speed_->setObjectName(QStringLiteral("inputPlayerSpeed"));
    speed_->setRange(0, 100000);
    speed_->setDecimals(2);
    speed_->setSuffix(QStringLiteral(" px/초"));
    speedRow->addRow(QStringLiteral("기본 이동 속도"), speed_);
    formLayout->addLayout(speedRow);
    actions_ = new QListWidget(form_);
    actions_->setObjectName(QStringLiteral("inputActions"));
    formLayout->addWidget(new QLabel(QStringLiteral("동작 (빈 키 목록 []는 해당 동작 OFF)"), form_));
    formLayout->addWidget(actions_, 1);
    auto* actionRow = new QHBoxLayout;
    actionName_ = new QLineEdit(form_);
    actionName_->setObjectName(QStringLiteral("inputActionName"));
    actionName_->setPlaceholderText(QStringLiteral("새 동작 예: jump"));
    auto* addAction = new QPushButton(QStringLiteral("동작 추가"), form_);
    addAction->setObjectName(QStringLiteral("inputAddAction"));
    auto* removeAction = new QPushButton(QStringLiteral("동작 삭제"), form_);
    actionRow->addWidget(actionName_, 1);
    actionRow->addWidget(addAction);
    actionRow->addWidget(removeAction);
    formLayout->addLayout(actionRow);
    keys_ = new QListWidget(form_);
    keys_->setObjectName(QStringLiteral("inputKeys"));
    keys_->setMaximumHeight(100);
    formLayout->addWidget(new QLabel(QStringLiteral("선택한 동작의 키"), form_));
    formLayout->addWidget(keys_);
    auto* keysRow = new QHBoxLayout;
    keyChoice_ = new QComboBox(form_);
    keyChoice_->setObjectName(QStringLiteral("inputKeyChoice"));
    for (const auto& name : naru::InputMap::keyNames()) keyChoice_->addItem(QString::fromStdString(name));
    auto* addKey = new QPushButton(QStringLiteral("키 추가"), form_);
    addKey->setObjectName(QStringLiteral("inputAddKey"));
    auto* removeKey = new QPushButton(QStringLiteral("선택 키 삭제"), form_);
    auto* clearKeys = new QPushButton(QStringLiteral("동작 OFF"), form_);
    clearKeys->setObjectName(QStringLiteral("inputDisableAction"));
    keysRow->addWidget(keyChoice_, 1);
    keysRow->addWidget(addKey);
    keysRow->addWidget(removeKey);
    keysRow->addWidget(clearKeys);
    formLayout->addLayout(keysRow);
    auto* addChord = new QPushButton(QStringLiteral("동시 키 조합 추가... (2~4개)"), form_);
    addChord->setObjectName(QStringLiteral("inputAddCombination"));
    formLayout->addWidget(addChord);
    connect(addChord, &QPushButton::clicked, this, [this] {
        if (currentAction().empty()) return;
        KeyCombinationDialog dialog({}, this);
        if (dialog.exec() == QDialog::Accepted) addCombination(dialog.keys());
    });
    auto* reset = new QPushButton(QStringLiteral("엔진 기본 설정으로 되돌리기"), form_);
    formLayout->addWidget(reset);
    source_ = new QPlainTextEdit(split);
    source_->setObjectName(QStringLiteral("inputSettingsSource"));
    source_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    source_->setLineWrapMode(QPlainTextEdit::NoWrap);
    split->setSizes({450, 610});
    layout->addWidget(split, 1);
    errors_ = new QLabel(this);
    errors_->setObjectName(QStringLiteral("inputSettingsErrors"));
    errors_->setWordWrap(true);
    layout->addWidget(errors_);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons_);
    connect(source_, &QPlainTextEdit::textChanged, this, &InputSettingsDialog::parseSource);
    connect(actions_, &QListWidget::currentRowChanged, this, [this] { refreshKeys(); });
    connect(enabled_, &QCheckBox::toggled, this, [this](bool value) {
        if (!loading_) { settings_.defaultControl = value; updateSource(); }
    });
    connect(speed_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!loading_) { settings_.playerSpeed = value; updateSource(); }
    });
    connect(addKey, &QPushButton::clicked, this, [this] {
        auto action = currentAction();
        if (action.empty()) return;
        auto& keys = settings_.bindings[action];
        auto name = keyChoice_->currentText().toStdString();
        if (keys.size() >= 64) return;
        if (std::find(keys.begin(), keys.end(), name) == keys.end()) keys.push_back(name);
        updateSource();
    });
    connect(removeKey, &QPushButton::clicked, this, [this] {
        auto action = currentAction();
        if (action.empty() || keys_->currentRow() < 0) return;
        auto& keys = settings_.bindings[action];
        keys.erase(keys.begin() + keys_->currentRow());
        updateSource();
    });
    connect(clearKeys, &QPushButton::clicked, this, [this] {
        auto action = currentAction();
        if (!action.empty()) { settings_.bindings[action].clear(); updateSource(); }
    });
    connect(addAction, &QPushButton::clicked, this, [this] {
        auto name = actionName_->text().trimmed().toStdString();
        if (!naru::InputMap::isValidActionName(name) || settings_.bindings.contains(name) || settings_.bindings.size() >= 128) {
            errors_->setText(QStringLiteral("동작 이름은 영문 소문자로 시작하며 소문자·숫자·_만 사용합니다 (32자까지, 중복 불가)."));
            return;
        }
        settings_.bindings[name] = {};
        updateSource();
        for (int row = 0; row < actions_->count(); ++row)
            if (actions_->item(row)->data(Qt::UserRole).toString().toStdString() == name) actions_->setCurrentRow(row);
        actionName_->clear();
    });
    connect(removeAction, &QPushButton::clicked, this, [this] {
        auto action = currentAction();
        const auto base = naru::InputSettings{};
        if (base.bindings.contains(action)) {
            errors_->setText(QStringLiteral("기본 동작은 삭제 대신 ‘동작 OFF’를 사용하세요."));
            return;
        }
        settings_.bindings.erase(action);
        updateSource();
    });
    connect(reset, &QPushButton::clicked, this, [this] { settings_ = {}; updateSource(); });
    source_->setPlainText(QString::fromStdString(source.empty() ? naru::inputSettingsSource({}) : source));
    parseSource();
    resize(1120, 760);
}

bool InputSettingsDialog::addCombination(const std::vector<std::string>& selected) {
    auto action = currentAction();
    if (!valid_ || action.empty() || selected.size() < 2 || selected.size() > 4) return false;
    std::string text;
    for (const auto& key : selected) { if (!text.empty()) text += " + "; text += key; }
    auto canonical = naru::InputMap::canonicalBinding(text);
    if (!canonical) return false;
    auto& entries = settings_.bindings[action];
    if (entries.size() >= 64 || std::find(entries.begin(), entries.end(), *canonical) != entries.end()) return false;
    entries.push_back(*canonical);
    updateSource();
    return true;
}

std::string InputSettingsDialog::source() const { return source_->toPlainText().toStdString(); }
std::string InputSettingsDialog::currentAction() const {
    return actions_->currentItem() ? actions_->currentItem()->data(Qt::UserRole).toString().toStdString() : std::string();
}
void InputSettingsDialog::parseSource() {
    auto parsed = naru::parseInputSettings(source());
    valid_ = parsed.settings.has_value();
    form_->setEnabled(valid_);
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(valid_);
    if (!valid_) {
        errors_->setStyleSheet(QStringLiteral("color: #b00020"));
        errors_->setText(QStringLiteral("%1행: %2").arg(parsed.line).arg(QString::fromStdString(parsed.error)));
        return;
    }
    settings_ = *parsed.settings;
    errors_->setStyleSheet({});
    errors_->setText(QStringLiteral("화면과 코드가 동기화되었습니다. True=ON, False=OFF, 키 목록 []=해당 동작 OFF."));
    refreshForm();
}
void InputSettingsDialog::refreshForm() {
    loading_ = true;
    auto selected = currentAction();
    enabled_->setChecked(settings_.defaultControl);
    speed_->setValue(settings_.playerSpeed);
    QSignalBlocker block(actions_);
    actions_->clear();
    int selectedRow = 0;
    for (const auto& [action, keys] : settings_.bindings) {
        auto* item = new QListWidgetItem(QStringLiteral("%1 (%2)%3").arg(actionLabel(action), QString::fromStdString(action),
            keys.empty() ? QStringLiteral(" — OFF") : QString()), actions_);
        item->setData(Qt::UserRole, QString::fromStdString(action));
        if (action == selected) selectedRow = actions_->count() - 1;
    }
    actions_->setCurrentRow(selectedRow);
    refreshKeys();
    loading_ = false;
}
void InputSettingsDialog::refreshKeys() {
    keys_->clear();
    auto action = currentAction();
    if (auto found = settings_.bindings.find(action); found != settings_.bindings.end())
        for (const auto& key : found->second) keys_->addItem(QString::fromStdString(key));
    if (keys_->count()) keys_->setCurrentRow(0);
}
void InputSettingsDialog::updateSource() {
    if (!valid_) return;
    source_->setPlainText(QString::fromStdString(naru::inputSettingsSource(settings_, source())));
}
