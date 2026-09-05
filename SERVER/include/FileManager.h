#pragma once

#include <QSqlDatabase>
#include <QString>

namespace netdisk::server {

class StorageEngine;

// 文件元数据、版本管理、完整性校验和秒传的统一业务入口。
// HTTP服务端只调用本类，不直接访问底层数据库和实体文件。
class FileManager
{
public:
    FileManager(
        QSqlDatabase& database,
        StorageEngine& storageEngine
    );

    bool initialize(const QString& databasePath = "server.db");

    bool addFile(
        const QString& sourcePath,
        const QString& owner
    );
    bool addUploadedFile(
        const QString& temporaryFilePath,
        const QString& owner,
        const QString& clientSha256
    );
    void listFiles(const QString& owner);
    bool deleteFile(int fileId, const QString& owner);
    bool renameFile(
        int fileId,
        const QString& owner,
        const QString& newFileName
    );
    QString getFilePath(int fileId, const QString& owner);
    QString getFileSha256(int fileId, const QString& owner);
    QString getFileVersionSha256(
        int fileId,
        int versionNumber,
        const QString& owner
    );
    void listFileVersions(int fileId, const QString& owner);
    bool addFileVersion(
        int fileId,
        const QString& sourcePath,
        const QString& owner
    );
    bool addUploadedFileVersion(
        int fileId,
        const QString& temporaryFilePath,
        const QString& owner,
        const QString& clientSha256
    );
    QString getFileVersionPath(
        int fileId,
        int versionNumber,
        const QString& owner
    );
    bool restoreFileVersion(
        int fileId,
        int versionNumber,
        const QString& owner
    );
    bool deleteFileVersion(
        int fileId,
        int versionNumber,
        const QString& owner
    );
    bool instantUploadFile(
        const QString& fileName,
        const QString& sha256,
        const QString& owner
    );

private:
    QSqlDatabase& m_database;
    StorageEngine& m_storageEngine;
};

} // namespace netdisk::server
