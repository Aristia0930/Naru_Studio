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
    QStringList importFiles(const QStringList& files, const QStringList& ids = {});
    bool renameAssetId(const std::string& before, const QString& after, QString* error);
    bool reimport(const std::string& id, const QString& source, QString* error);
    bool exportFile(const std::string& id, const QString& destination, QString* error, bool overwrite = false);
    bool addGroup(const QString& name);
    bool renameGroup(const QString& before, const QString& after);
    bool removeGroup(const QString& name);
    bool moveToGroup(const QStringList& ids, const QString& group);
    bool renameAsset(const std::string& id, const QString& name);
    bool unregisterAsset(const std::string& id);
    QString assetState(const naru::Asset& asset) const;
    void refresh();
    void stopAudio();
    void showImportDialog(const QStringList& initialFiles = {});

private:
    const naru::Asset* selectedAsset() const;
    void preview();
    void playAudio();
    bool prepare(const QString& source, naru::Asset& asset, QString* error);
    void commit(std::vector<naru::Asset> after, const QString& text);
    void commit(std::vector<naru::Asset> after, std::vector<std::string> groups, const QString& text);
    QString checkedPath(const naru::Asset& asset) const;
    QString newAssetIdsError(const QStringList& ids) const;

    ProjectDocument* doc_;
    QLineEdit* search_;
    QComboBox* kind_;
    QComboBox* group_;
    QListWidget* list_;
    QLabel* image_;
    QLabel* details_;
    QLabel* playback_;
    QPushButton* play_;
    SDL_AudioStream* audio_ = nullptr;
    bool audioInitialized_ = false;
    int fontId_ = -1;
};
