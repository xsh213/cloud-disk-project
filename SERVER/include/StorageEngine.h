#pragma once

#include <QString>

namespace netdisk::server {

// 服务器实体文件存储入口。
// 分片保存、合并和断点续传将在HTTP接口确定后继续扩展。
class StorageEngine
{
public:
    explicit StorageEngine(
        const QString& storageRoot = "data/files"
    );

    bool initialize() const;
    QString calculateFileSha256(const QString& filePath) const;
    bool isValidSha256(const QString& sha256) const;
    QString storageRoot() const;

private:
    QString m_storageRoot;
};

} // namespace netdisk::server
