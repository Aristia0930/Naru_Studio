#pragma once
#include "naru/project/Project.h"
#include <QDialog>
#include <QHash>
class ProjectDocument;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QSpinBox;
class QLabel;

class ImageSettingsDialog : public QDialog {
    Q_OBJECT
public:
    ImageSettingsDialog(ProjectDocument* doc, const naru::SceneObject& object, QWidget* parent = nullptr);
    ImageSettingsDialog(ProjectDocument* doc, const naru::Scene& scene, QWidget* parent = nullptr);
    naru::ObjectImage objectImage() const;
    naru::BackgroundImage backgroundImage() const;
private:
    void build(const std::string& asset);
    void updatePreview();
    ProjectDocument* doc_;
    bool background_;
    naru::ObjectImage object_;
    naru::BackgroundImage scene_;
    QComboBox* assets_ = nullptr;
    QComboBox* mode_ = nullptr;
    QHash<QString, QDoubleSpinBox*> numbers_;
    QCheckBox* smooth_ = nullptr;
    QCheckBox* flipX_ = nullptr;
    QCheckBox* flipY_ = nullptr;
    QSpinBox* order_ = nullptr;
    QLabel* preview_ = nullptr;
    QLabel* status_ = nullptr;
};
