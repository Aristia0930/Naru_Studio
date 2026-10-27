#pragma once

#include "naru/events/EventData.h"

#include <QDialog>
#include <QStringList>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;
class QStackedWidget;

// 이벤트 명령 하나의 입력 폼. 명령 종류에 맞는 칸만 보여 준다.
class CommandDialog : public QDialog {
    Q_OBJECT

public:
    CommandDialog(const naru::EventCommand& command, const QStringList& scripts, QWidget* parent = nullptr);

    naru::EventCommand command() const { return command_; }

    static QString typeLabel(naru::EventCommandType type);

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
};
