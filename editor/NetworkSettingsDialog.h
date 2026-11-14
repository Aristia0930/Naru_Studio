#pragma once
#include "naru/project/Project.h"
#include <QDialog>
class QListWidget;
class QLineEdit;
class QDoubleSpinBox;
class NetworkFields;
class NetworkSettingsDialog : public QDialog {
public:
    NetworkSettingsDialog(const naru::Project& project, QWidget* parent = nullptr);
    const std::vector<naru::NetworkConnection>& connections() const { return entries_; }
private:
    void saveRow();
    void loadRow(int row);
    void accept() override;
    naru::Project project_;
    std::vector<naru::NetworkConnection> entries_;
    QListWidget* list_;
    QLineEdit* id_;
    QLineEdit* url_;
    QDoubleSpinBox* timeout_;
    NetworkFields* headers_;
    int current_ = -1;
};
