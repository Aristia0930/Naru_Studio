#include "NetworkSettingsDialog.h"
#include "NetworkFields.h"
#include <QListWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <functional>
#include <algorithm>

NetworkSettingsDialog::NetworkSettingsDialog(const naru::Project& project, QWidget* parent)
    : QDialog(parent), project_(project), entries_(project.networkConnections) {
    setWindowTitle(QStringLiteral("네트워크 접속 설정")); resize(850, 560);
    auto* layout = new QVBoxLayout(this);
    auto* help = new QLabel(QStringLiteral("이벤트와 Python에서 접속 ID를 함께 사용합니다. 설정은 project.json에 저장됩니다.\n서버 전용 비밀번호는 넣지 마세요. ID 변경/제거 전 해당 이벤트와 Python 호출을 먼저 수정하세요."), this);
    help->setWordWrap(true); layout->addWidget(help);
    auto* row = new QHBoxLayout; layout->addLayout(row, 1);
    auto* left = new QVBoxLayout; row->addLayout(left);
    list_ = new QListWidget(this); list_->setObjectName("networkConnections"); left->addWidget(list_);
    for (const auto& e : entries_) list_->addItem(QString::fromStdString(e.id));
    auto* add = new QPushButton(QStringLiteral("접속 추가"), this); add->setObjectName("networkAdd"); left->addWidget(add);
    auto* remove = new QPushButton(QStringLiteral("선택 접속 제거"), this); left->addWidget(remove);
    auto* form = new QFormLayout; row->addLayout(form, 2);
    id_ = new QLineEdit(this); id_->setObjectName("networkConnectionId");
    url_ = new QLineEdit(this); url_->setObjectName("networkBaseUrl"); url_->setPlaceholderText("https://api.example.com");
    timeout_ = new QDoubleSpinBox(this); timeout_->setObjectName("networkTimeout"); timeout_->setRange(0.1, 60); timeout_->setDecimals(1); timeout_->setSuffix(QStringLiteral(" 초"));
    headers_ = new NetworkFields(QStringLiteral("공통 헤더 이름"), QStringLiteral("값"), this); headers_->setObjectName("networkCommonHeaders");
    form->addRow(QStringLiteral("접속 ID"), id_); form->addRow(QStringLiteral("서버 기본 주소"), url_);
    form->addRow(QStringLiteral("제한 시간"), timeout_); form->addRow(QStringLiteral("헤더 (선택)"), headers_);
    connect(list_, &QListWidget::currentRowChanged, this, [this](int index) {
        try { saveRow(); } catch (const std::exception& e) { QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what())); list_->blockSignals(true); list_->setCurrentRow(current_); list_->blockSignals(false); return; }
        loadRow(index);
    });
    connect(add, &QPushButton::clicked, this, [this] {
        try { saveRow(); } catch (const std::exception& e) { QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what())); return; }
        naru::NetworkConnection e; int suffix = 1;
        do { e.id = "server_" + std::to_string(suffix++); }
        while (std::any_of(entries_.begin(), entries_.end(), [&](const auto& item) { return item.id == e.id; }));
        e.baseUrl = "https://api.example.com";
        entries_.push_back(e); list_->addItem(QString::fromStdString(e.id)); list_->setCurrentRow(list_->count()-1);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        if (current_ < 0) return;
        const int index = current_; current_ = -1; entries_.erase(entries_.begin()+index);
        list_->blockSignals(true); delete list_->takeItem(index); list_->blockSignals(false);
        list_->setCurrentRow(-1); list_->setCurrentRow(std::min(index, list_->count()-1));
        if (entries_.empty()) loadRow(-1);
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &NetworkSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    if (!entries_.empty()) list_->setCurrentRow(0); else loadRow(-1);
}
void NetworkSettingsDialog::saveRow() {
    if (current_ < 0) return;
    auto e = entries_[current_];
    e.id = id_->text().trimmed().toStdString(); e.baseUrl = url_->text().trimmed().toStdString();
    e.timeoutMs = static_cast<int>(timeout_->value()*1000); e.headers = headers_->values(true);
    entries_[current_] = e; list_->item(current_)->setText(QString::fromStdString(e.id));
}
void NetworkSettingsDialog::loadRow(int row) {
    current_ = row;
    QWidget* fields[] = {id_, url_, timeout_, headers_};
    for (auto* widget : fields) widget->setEnabled(row >= 0);
    const auto e = row >= 0 ? entries_[row] : naru::NetworkConnection{};
    id_->setText(QString::fromStdString(e.id)); url_->setText(QString::fromStdString(e.baseUrl));
    timeout_->setValue(e.timeoutMs/1000.0); headers_->setValues(e.headers);
}
void NetworkSettingsDialog::accept() {
    try {
        saveRow(); naru::validateConnections(entries_);
        std::function<void(const std::vector<naru::EventCommand>&)> scan = [&](const auto& list) {
            for (const auto& c : list) {
                if (c.type == naru::EventCommandType::NetworkRequest && std::none_of(entries_.begin(), entries_.end(), [&](const auto& e) { return e.id == c.network.connection; }))
                    throw std::invalid_argument("이벤트에서 사용하는 접속 ID를 먼저 수정하세요: " + c.network.connection);
                scan(c.thenCommands); scan(c.elseCommands); for (const auto& b : c.choices) scan(b.commands);
            }
        };
        for (const auto& scene : project_.scenes) for (const auto& o : scene.objects) scan(o.event.commands);
        for (const auto& c : project_.commonEvents) scan(c.event.commands);
        QDialog::accept();
    } catch (const std::exception& e) { QMessageBox::warning(this, windowTitle(), QString::fromUtf8(e.what())); }
}
