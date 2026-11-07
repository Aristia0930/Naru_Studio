#pragma once
#include "ProjectDocument.h"
#include "naru/project/ImageLayout.h"
#include <QDir>
#include <QFileInfo>
#include <QImageReader>

inline QImage loadProjectImage(ProjectDocument* doc, const std::string& id, QString* error = nullptr) {
    if (id.empty()) return {};
    const auto* a = naru::findImageAsset(doc->project(), id);
    if (!a) { if (error) *error = QStringLiteral("이미지 에셋을 찾을 수 없습니다: ") + QString::fromStdString(id); return {}; }
    const auto invalid = naru::validateAssets({*a}, std::filesystem::path(doc->directory().toStdWString()));
    if (!invalid.empty()) { if (error) *error = QString::fromStdString(invalid); return {}; }
    const auto path = QDir(doc->directory()).filePath(QString::fromStdString(a->path));
    QImageReader reader(path);
    const auto size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 32 * 1024 * 1024 || QFileInfo(path).size() > 64 * 1024 * 1024) {
        if (error) *error = QStringLiteral("이미지 파일이 없거나 손상되었거나 너무 큽니다: ") + QString::fromStdString(id);
        return {};
    }
    auto image = reader.read();
    if (image.isNull() && error) *error = reader.errorString();
    return image;
}
