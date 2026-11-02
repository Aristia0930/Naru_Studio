#pragma once

#include "naru/project/Asset.h"
#include <QWidget>
#include <QStringList>

class ProjectDocument;
class QListWidget;
class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;
struct SDL_AudioStream;

class AssetBrowser : public QWidget {
    Q_OBJECT
public:
    explicit AssetBrowser(ProjectDocument* document, QWidget* parent = nullptr);
    ~AssetBrowser() override;

    // Shared by the UI and the editor integration test. Errors do not alter the catalog.
    QStringList importFiles(const QStringList& files);
    bool reimport(const std::string& id, const QString& source, QString* error);
    bool renameAsset(const std::string& id, const QString& name);
    bool unregisterAsset(const std::string& id);
    QString assetState(const naru::Asset& asset) const;
    void refresh();
    void stopAudio();

private:
    const naru::Asset* selectedAsset() const;
    void preview();
    void playAudio();
    bool prepare(const QString& source, naru::Asset& asset, QString* error);
    void commit(std::vector<naru::Asset> after, const QString& text);
    QString checkedPath(const naru::Asset& asset) const;

    ProjectDocument* doc_;
    QLineEdit* search_;
    QComboBox* kind_;
    QListWidget* list_;
    QLabel* image_;
    QLabel* details_;
    QLabel* playback_;
    QPushButton* play_;
    SDL_AudioStream* audio_ = nullptr;
    bool audioInitialized_ = false;
    int fontId_ = -1;
};
