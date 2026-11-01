#pragma once
#include "naru/runtime/Input.h"
#include <QDialog>
class QCheckBox;
class QDoubleSpinBox;
class QListWidget;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QLabel;
class QDialogButtonBox;

class KeyCombinationDialog : public QDialog {
public:
    explicit KeyCombinationDialog(const std::vector<std::string>& selected = {}, QWidget* parent = nullptr);
    std::vector<std::string> keys() const;
private:
    void validate();
    QListWidget* keys_;
    QLabel* summary_;
    QDialogButtonBox* buttons_;
};

class InputSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit InputSettingsDialog(const std::string& source, QWidget* parent = nullptr);
    std::string source() const;
    const naru::InputSettings& settings() const { return settings_; }
    bool valid() const { return valid_; }
    bool addCombination(const std::vector<std::string>& keys);
private:
    void parseSource();
    void refreshForm();
    void refreshKeys();
    void updateSource();
    std::string currentAction() const;
    naru::InputSettings settings_;
    bool valid_ = false;
    bool loading_ = false;
    QWidget* form_;
    QCheckBox* enabled_;
    QDoubleSpinBox* speed_;
    QListWidget* actions_;
    QListWidget* keys_;
    QComboBox* keyChoice_;
    QLineEdit* actionName_;
    QPlainTextEdit* source_;
    QLabel* errors_;
    QDialogButtonBox* buttons_;
};
