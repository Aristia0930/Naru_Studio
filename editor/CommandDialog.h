#pragma once

#include "naru/data/Database.h"
#include "naru/events/EventData.h"
#include "naru/project/Project.h"

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;

// 이벤트 명령 하나의 입력 폼. 명령 종류에 맞는 칸만 보여 준다.
class CommandDialog : public QDialog {
    Q_OBJECT

public:
    // scene: 대상 객체 목록 ("객체 이동" 등). 없으면 이 객체·플레이어만 고를 수 있다.
    CommandDialog(const naru::EventCommand& command, const QStringList& scripts,
                  const naru::Database* database = nullptr, const naru::Scene* scene = nullptr,
                  QWidget* parent = nullptr);

    naru::EventCommand command() const { return command_; }

    static QString typeLabel(naru::EventCommandType type);
    // 대상 표시 이름: "이 객체", "플레이어", 객체 이름 (장면에 없는 객체면 ID).
    static QString targetLabel(const std::string& target, const naru::Scene* scene);
    static QString directionLabel(const std::string& direction);

private:
    void accept() override;
    bool readValue(QComboBox* typeBox, QLineEdit* edit, QComboBox* boolBox, naru::Value& out);

    naru::EventCommand command_;

    QLineEdit* speaker_ = nullptr;
    QPlainTextEdit* text_ = nullptr;
    QLineEdit* line_ = nullptr;
    QDoubleSpinBox* seconds_ = nullptr;
    QLineEdit* variable_ = nullptr;
    QComboBox* op_ = nullptr;
    QComboBox* valueType_ = nullptr;
    QLineEdit* valueEdit_ = nullptr;
    QComboBox* valueBool_ = nullptr;
    QComboBox* script_ = nullptr;
    QLineEdit* function_ = nullptr;
    QComboBox* dataType_ = nullptr;
    QComboBox* record_ = nullptr;
    QComboBox* field_ = nullptr;
    QComboBox* target_ = nullptr;
    QComboBox* direction_ = nullptr;
    QDoubleSpinBox* distance_ = nullptr;
    QDoubleSpinBox* speed_ = nullptr;
    QCheckBox* waitForEnd_ = nullptr;
    QComboBox* show_ = nullptr;
};
