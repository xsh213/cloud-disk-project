#include "FileManager.h"
#include "StorageEngine.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QList>
#include <QPair>
#include <QSet>
#include <QVector>
#include <QUuid>
#include <QDirIterator>

namespace netdisk
{
    namespace server
    {
namespace detail
{
bool initDatabase(
    QSqlDatabase& database,
    const QString& databasePath
);
bool initStorageDirectory();
bool addFile(
    QSqlDatabase& database,
    const QString& sourcePath,
    const QString& owner
);
bool addUploadedFile(
    QSqlDatabase& database,
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256
);
void listFiles(QSqlDatabase& database, const QString& owner);
bool deleteFile(
    QSqlDatabase& database,
    int fileId,
    const QString& owner
);
bool renameFile(
    QSqlDatabase& database,
    int fileId,
    const QString& owner,
    const QString& newFileName
);
QString getFilePath(
    QSqlDatabase& database,
    int fileId,
    const QString& owner
);
QString getFileSha256(
    QSqlDatabase& database,
    int fileId,
    const QString& owner
);
QString getFileVersionSha256(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner
);
void listFileVersions(
    QSqlDatabase& database,
    int fileId,
    const QString& owner
);
bool addFileVersion(
    QSqlDatabase& database,
    int fileId,
    const QString& sourcePath,
    const QString& owner
);
bool addUploadedFileVersion(
    QSqlDatabase& database,
    int fileId,
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256
);
QString getFileVersionPath(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner
);
bool restoreFileVersion(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner
);
bool deleteFileVersion(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner
);
QString calculateFileSha256(const QString& filePath);
QString findFilePathBySha256(
    QSqlDatabase& database,
    const QString& sha256
);
bool instantUploadFile(
    QSqlDatabase& database,
    const QString& fileName,
    const QString& sha256,
    const QString& owner
);

static bool backfillMissingSha256(
    QSqlDatabase& database
);
static bool recoverInterruptedDeletions(
    QSqlDatabase& database
);

static bool isValidSha256Value(
    const QString& sha256)
{
    const QString normalizedSha256 =
        sha256.trimmed().toLower();

    if (normalizedSha256.length() != 64)
    {
        return false;
    }

    for (const QChar character :
    normalizedSha256)
    {
        const bool isValidCharacter =
            (character >= '0' &&
                character <= '9') ||
            (character >= 'a' &&
                character <= 'f');

        if (!isValidCharacter)
        {
            return false;
        }
    }

    return true;
}

// =====================================================
// 1. 初始化数据库
// =====================================================
bool initDatabase(
    QSqlDatabase& database,
    const QString& databasePath)
{
    database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(databasePath);

    if (!database.open())
    {
        qDebug() << "Database open failed:"
            << database.lastError().text();

        return false;
    }

    qDebug() << "Database opened successfully!";

    QSqlQuery query(database);

    // 开启SQLite外键约束
    if (!query.exec("PRAGMA foreign_keys = ON"))
    {
        qDebug() << "Enable foreign keys failed:"
            << query.lastError().text();

        return false;
    }

    // 逻辑文件表
    QString filesTableSql =
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "filename TEXT NOT NULL,"
        "size INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "path TEXT NOT NULL,"
        "upload_time TEXT NOT NULL"
        ")";

    if (!query.exec(filesTableSql))
    {
        qDebug() << "Create files table failed:"
            << query.lastError().text();

        return false;
    }

    qDebug() << "files table ready!";

    // 文件历史版本表
    QString versionsTableSql =
        "CREATE TABLE IF NOT EXISTS file_versions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "file_id INTEGER NOT NULL,"
        "version_number INTEGER NOT NULL,"
        "size INTEGER NOT NULL,"
        "path TEXT NOT NULL,"
        "sha256 TEXT,"
        "upload_time TEXT NOT NULL,"
        "is_complete INTEGER NOT NULL DEFAULT 1,"
        "FOREIGN KEY(file_id) REFERENCES files(id) ON DELETE CASCADE,"
        "UNIQUE(file_id, version_number)"
        ")";

    if (!query.exec(versionsTableSql))
    {
        qDebug() << "Create file_versions table failed:"
            << query.lastError().text();

        return false;
    }

    qDebug() << "file_versions table ready!";

    // 将旧files表中的现有文件自动登记为v1
    QString migrateSql =
        "INSERT OR IGNORE INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "SELECT id, 1, size, path, NULL, upload_time, 1 "
        "FROM files";

    if (!query.exec(migrateSql))
    {
        qDebug() << "Existing file migration failed:"
            << query.lastError().text();

        return false;
    }

    qDebug() << "Existing files migrated to version 1!";

    // 先恢复异常中断的删除操作
    if (!recoverInterruptedDeletions(database))
    {
        qDebug() << "Interrupted deletion recovery failed.";
        return false;
    }

    // 恢复完成后，再补齐缺失的SHA-256
    if (!backfillMissingSha256(database))
    {
        qDebug() << "Backfill missing SHA-256 failed.";
        return false;
    }

    return true;
}

// =====================================================
// 2. 初始化服务器总存储目录
// =====================================================
bool initStorageDirectory()
{
    QString storageDirectoryPath = "data/files";

    QDir storageDirectory;

    // 如果目录不存在，则自动创建
    if (!storageDirectory.exists(storageDirectoryPath))
    {
        if (!storageDirectory.mkpath(storageDirectoryPath))
        {
            qDebug() << "Failed to create storage directory.";

            return false;
        }
    }

    qDebug() << "Storage directory ready:"
        << storageDirectoryPath;

    return true;
}


// =====================================================
// 3. 添加文件
// =====================================================
bool addFile(
    QSqlDatabase& database,
    const QString& sourcePath,
    const QString& owner)
{
    // 检查用户名，防止生成非法存储路径
    if (owner.isEmpty() ||
        owner.contains("/") ||
        owner.contains("\\"))
    {
        qDebug() << "Invalid owner.";
        return false;
    }

    QFileInfo fileInfo(sourcePath);

    if (!fileInfo.exists() || !fileInfo.isFile())
    {
        qDebug() << "Source file does not exist:"
            << sourcePath;

        return false;
    }

    const QString fileName = fileInfo.fileName();
    qint64 fileSize = fileInfo.size();

    QString fileSha256 =
        calculateFileSha256(sourcePath);

    if (fileSha256.isEmpty())
    {
        qDebug() << "Calculate file SHA-256 failed.";
        return false;
    }

    QString userStorageDirectoryPath =
        "data/files/" + owner;

    QDir storageDirectory;

    if (!storageDirectory.exists(userStorageDirectoryPath) &&
        !storageDirectory.mkpath(userStorageDirectoryPath))
    {
        qDebug() << "Failed to create user storage directory:"
            << userStorageDirectoryPath;

        return false;
    }

    QString targetPath =
        userStorageDirectoryPath + "/" + fileName;

    // 检查数据库中是否存在同名文件
    QSqlQuery checkQuery(database);

    checkQuery.prepare(
        "SELECT id "
        "FROM files "
        "WHERE filename = ? AND owner = ?"
    );

    checkQuery.addBindValue(fileName);
    checkQuery.addBindValue(owner);

    if (!checkQuery.exec())
    {
        qDebug() << "Check duplicate file failed:"
            << checkQuery.lastError().text();

        return false;
    }

    if (checkQuery.next())
    {
        qDebug() << "File already exists for this user:"
            << fileName;

        return false;
    }

    if (QFile::exists(targetPath))
    {
        qDebug() << "Physical file already exists:"
            << targetPath;

        return false;
    }

    // 先复制真实文件
    if (!QFile::copy(sourcePath, targetPath))
    {
        qDebug() << "Failed to copy file.";
        return false;
    }

    QString uploadTime =
        QDateTime::currentDateTime()
        .toString("yyyy-MM-dd HH:mm:ss");

    // files和file_versions必须同时写入成功
    if (!database.transaction())
    {
        qDebug() << "Failed to start add-file transaction.";

        QFile::remove(targetPath);
        return false;
    }

    QSqlQuery insertFileQuery(database);

    insertFileQuery.prepare(
        "INSERT INTO files "
        "(filename, size, owner, path, upload_time) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    insertFileQuery.addBindValue(fileName);
    insertFileQuery.addBindValue(fileSize);
    insertFileQuery.addBindValue(owner);
    insertFileQuery.addBindValue(targetPath);
    insertFileQuery.addBindValue(uploadTime);

    if (!insertFileQuery.exec())
    {
        qDebug() << "Insert file record failed:"
            << insertFileQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    int fileId =
        insertFileQuery.lastInsertId().toInt();

    // 新文件立即登记为版本1，不需要重启服务器
    QSqlQuery insertVersionQuery(database);

    insertVersionQuery.prepare(
        "INSERT INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "VALUES (?, 1, ?, ?, ?, ?, 1)"
    );

    insertVersionQuery.addBindValue(fileId);
    insertVersionQuery.addBindValue(fileSize);
    insertVersionQuery.addBindValue(targetPath);
    insertVersionQuery.addBindValue(fileSha256);
    insertVersionQuery.addBindValue(uploadTime);

    if (!insertVersionQuery.exec())
    {
        qDebug() << "Insert version 1 failed:"
            << insertVersionQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    if (!database.commit())
    {
        qDebug() << "Commit add-file transaction failed:"
            << database.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    qDebug() << "File added successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Initial version: 1";
    qDebug() << "Path:" << targetPath;

    return true;
}

bool addUploadedFile(
    QSqlDatabase& database,
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256)
{
    QFileInfo fileInfo(temporaryFilePath);

    if (!fileInfo.exists() ||
        !fileInfo.isFile())
    {
        qDebug() << "Temporary upload file does not exist:"
            << temporaryFilePath;

        return false;
    }

    const QString normalizedClientSha256 =
        clientSha256.trimmed().toLower();

    if (!isValidSha256Value(
        normalizedClientSha256))
    {
        qDebug() << "Invalid client SHA-256 format.";

        if (!QFile::remove(temporaryFilePath))
        {
            qDebug() << "Failed to remove invalid upload file:"
                << temporaryFilePath;
        }

        return false;
    }

    const QString serverSha256 =
        calculateFileSha256(temporaryFilePath)
        .toLower();

    if (serverSha256.isEmpty())
    {
        qDebug() << "Calculate uploaded file SHA-256 failed.";

        QFile::remove(temporaryFilePath);
        return false;
    }

    if (serverSha256 != normalizedClientSha256)
    {
        qDebug() << "Uploaded file SHA-256 mismatch.";
        qDebug() << "Client SHA-256:"
            << normalizedClientSha256;
        qDebug() << "Server SHA-256:"
            << serverSha256;

        if (!QFile::remove(temporaryFilePath))
        {
            qDebug() << "Failed to remove corrupted upload file:"
                << temporaryFilePath;
        }

        return false;
    }

    qDebug() << "Uploaded file SHA-256 verified.";

    const bool isFileAdded =
        addFile(
            database,
            temporaryFilePath,
            owner
        );

    // addFile会复制到正式存储目录，因此临时文件不再需要
    if (!QFile::remove(temporaryFilePath))
    {
        qDebug() << "Warning: failed to clean temporary upload file:"
            << temporaryFilePath;
    }

    return isFileAdded;
}

// =====================================================
// 4. 查询某个用户的文件列表
// =====================================================
void listFiles(QSqlDatabase& database,
    const QString& owner)
{
    QSqlQuery selectQuery(database);


    selectQuery.prepare(
        "SELECT id, filename, size, path, upload_time "
        "FROM files "
        "WHERE owner = ? "
        "ORDER BY id ASC"
    );


    selectQuery.addBindValue(owner);


    if (!selectQuery.exec())
    {
        qDebug() << "Query files failed:"
            << selectQuery.lastError().text();

        return;
    }


    qDebug() << "";
    qDebug() << "===== File List =====";
    qDebug() << "Owner:" << owner;


    bool hasFile = false;


    while (selectQuery.next())
    {
        hasFile = true;


        const int fileId =
            selectQuery.value("id").toInt();


        const QString fileName =
            selectQuery.value("filename").toString();


        const qint64 fileSize =
            selectQuery.value("size").toLongLong();


        const QString storedPath =
            selectQuery.value("path").toString();


        const QString uploadTime =
            selectQuery.value("upload_time").toString();


        qDebug() << "ID:" << fileId;
        qDebug() << "Filename:" << fileName;
        qDebug() << "Size:" << fileSize;
        qDebug() << "Path:" << storedPath;
        qDebug() << "Upload time:" << uploadTime;
        qDebug() << "----------------";
    }


    if (!hasFile)
    {
        qDebug() << "No files found.";
    }


    qDebug() << "=====================";
}


// =====================================================
// 5. 删除文件
// =====================================================
bool deleteFile(
    QSqlDatabase& database,
    int fileId,
    const QString& owner)
{
    struct RenamedFile
    {
        QString originalPath;
        QString temporaryPath;
    };

    QVector<RenamedFile> renamedFiles;

    auto restoreRenamedFiles = [&renamedFiles]()
        {
            bool isRestoreSuccessful = true;

            for (int i = renamedFiles.size() - 1; i >= 0; --i)
            {
                const RenamedFile& renamedFile =
                    renamedFiles.at(i);

                if (QFile::exists(renamedFile.temporaryPath) &&
                    !QFile::rename(
                        renamedFile.temporaryPath,
                        renamedFile.originalPath))
                {
                    qDebug() << "Failed to restore file:"
                        << renamedFile.temporaryPath;

                    isRestoreSuccessful = false;
                }
            }

            return isRestoreSuccessful;
        };

    if (!database.transaction())
    {
        qDebug() << "Failed to start database transaction:"
            << database.lastError().text();

        return false;
    }

    // 查询文件，并同时验证用户权限
    QSqlQuery findQuery(database);

    findQuery.prepare(
        "SELECT filename, path "
        "FROM files "
        "WHERE id = ? AND owner = ?"
    );

    findQuery.addBindValue(fileId);
    findQuery.addBindValue(owner);

    if (!findQuery.exec())
    {
        qDebug() << "Find file failed:"
            << findQuery.lastError().text();

        database.rollback();
        return false;
    }

    if (!findQuery.next())
    {
        qDebug() << "File not found or permission denied.";

        database.rollback();
        return false;
    }

    const QString fileName =
        findQuery.value("filename").toString();

    // 使用集合保存路径，防止最新版本路径被重复处理
    QSet<QString> physicalPaths;

    const QString latestPath =
        findQuery.value("path").toString();

    if (!latestPath.isEmpty())
    {
        physicalPaths.insert(latestPath);
    }

    // 查询该文件的全部历史版本路径
    QSqlQuery versionQuery(database);

    versionQuery.prepare(
        "SELECT path "
        "FROM file_versions "
        "WHERE file_id = ?"
    );

    versionQuery.addBindValue(fileId);

    if (!versionQuery.exec())
    {
        qDebug() << "Find file versions failed:"
            << versionQuery.lastError().text();

        database.rollback();
        return false;
    }

    while (versionQuery.next())
    {
        const QString versionPath =
            versionQuery.value("path").toString();

        if (!versionPath.isEmpty())
        {
            physicalPaths.insert(versionPath);
        }
    }

    // 先将所有仍存在的实体文件临时改名
    for (const QString& originalPath : physicalPaths)
    {
        if (!QFile::exists(originalPath))
        {
            qDebug() << "Warning: physical file is missing:"
                << originalPath;

            continue;
        }

        const QString temporaryPath =
            originalPath +
            ".deleting." +
            QUuid::createUuid().toString(
                QUuid::WithoutBraces
            );

        if (!QFile::rename(
            originalPath,
            temporaryPath))
        {
            qDebug() << "Failed to prepare file for deletion:"
                << originalPath;

            restoreRenamedFiles();
            database.rollback();
            return false;
        }

        renamedFiles.append(
            { originalPath, temporaryPath }
        );
    }

    // 显式删除所有历史版本记录
    QSqlQuery deleteVersionsQuery(database);

    deleteVersionsQuery.prepare(
        "DELETE FROM file_versions "
        "WHERE file_id = ?"
    );

    deleteVersionsQuery.addBindValue(fileId);

    if (!deleteVersionsQuery.exec())
    {
        qDebug() << "Delete file versions failed:"
            << deleteVersionsQuery.lastError().text();

        database.rollback();
        restoreRenamedFiles();
        return false;
    }

    // 删除逻辑文件记录
    QSqlQuery deleteFileQuery(database);

    deleteFileQuery.prepare(
        "DELETE FROM files "
        "WHERE id = ? AND owner = ?"
    );

    deleteFileQuery.addBindValue(fileId);
    deleteFileQuery.addBindValue(owner);

    if (!deleteFileQuery.exec() ||
        deleteFileQuery.numRowsAffected() != 1)
    {
        qDebug() << "Delete file record failed:"
            << deleteFileQuery.lastError().text();

        database.rollback();
        restoreRenamedFiles();
        return false;
    }

    if (!database.commit())
    {
        qDebug() << "Database commit failed:"
            << database.lastError().text();

        database.rollback();
        restoreRenamedFiles();
        return false;
    }

    // 数据库提交成功后，再真正清理临时文件
    bool isCleanupSuccessful = true;

    for (const RenamedFile& renamedFile : renamedFiles)
    {
        if (!QFile::remove(renamedFile.temporaryPath))
        {
            qDebug() << "Warning: temporary file cleanup failed:"
                << renamedFile.temporaryPath;

            isCleanupSuccessful = false;
        }
    }

    qDebug() << "File deleted successfully:"
        << fileName;

    if (!isCleanupSuccessful)
    {
        qDebug() << "Warning: some temporary files require"
            << "later cleanup.";
    }

    return true;
}

// =====================================================
// 6. 重命名文件
// =====================================================
bool renameFile(QSqlDatabase& database,
    int fileId,
    const QString& owner,
    const QString& newFileName)
{
    // -------------------------------------------------
    // 检查新文件名
    // -------------------------------------------------
    if (newFileName.isEmpty())
    {
        qDebug() << "New filename cannot be empty.";

        return false;
    }


    // -------------------------------------------------
    // 防止新文件名中出现路径
    // -------------------------------------------------
    if (newFileName.contains("/") ||
        newFileName.contains("\\"))
    {
        qDebug() << "Invalid filename.";

        return false;
    }


    // -------------------------------------------------
    // 查询原文件
    // -------------------------------------------------
    QSqlQuery findQuery(database);


    findQuery.prepare(
        "SELECT filename, path "
        "FROM files "
        "WHERE id = ? AND owner = ?"
    );


    findQuery.addBindValue(fileId);
    findQuery.addBindValue(owner);


    if (!findQuery.exec())
    {
        qDebug() << "Find file failed:"
            << findQuery.lastError().text();

        return false;
    }


    if (!findQuery.next())
    {
        qDebug() << "File not found or permission denied.";

        return false;
    }


    QString oldFileName =
        findQuery.value("filename").toString();


    QString oldPath =
        findQuery.value("path").toString();


    // -------------------------------------------------
    // 如果新旧文件名完全相同
    // -------------------------------------------------
    if (oldFileName == newFileName)
    {
        qDebug() << "New filename is the same as the old filename.";

        return false;
    }


    QFileInfo oldFileInfo(oldPath);


    QString directory =
        oldFileInfo.absolutePath();


    QString newPath =
        directory + "/" + newFileName;


    // -------------------------------------------------
    // 检查新文件名是否已经存在
    // -------------------------------------------------
    if (QFile::exists(newPath))
    {
        qDebug() << "A file with the new name already exists.";

        return false;
    }


    // -------------------------------------------------
    // 先修改真实文件名
    // -------------------------------------------------
    if (!QFile::rename(oldPath, newPath))
    {
        qDebug() << "Failed to rename physical file.";

        return false;
    }


    // -------------------------------------------------
    // 修改数据库记录
    // -------------------------------------------------
    QSqlQuery updateQuery(database);


    updateQuery.prepare(
        "UPDATE files "
        "SET filename = ?, path = ? "
        "WHERE id = ? AND owner = ?"
    );


    updateQuery.addBindValue(newFileName);
    updateQuery.addBindValue(newPath);
    updateQuery.addBindValue(fileId);
    updateQuery.addBindValue(owner);


    // -------------------------------------------------
    // 数据库修改失败
    // 则将真实文件恢复原名
    // -------------------------------------------------
    if (!updateQuery.exec())
    {
        qDebug() << "Database rename failed:"
            << updateQuery.lastError().text();


        QFile::rename(
            newPath,
            oldPath
        );


        return false;
    }


    qDebug() << "File renamed successfully:";
    qDebug() << oldFileName
        << "->"
        << newFileName;


    return true;
}

// =====================================================
// 7. 根据文件 ID 获取真实文件路径
// =====================================================
QString getFilePath(QSqlDatabase& database,
    int fileId,
    const QString& owner)
{
    QSqlQuery query(database);

    query.prepare(
        "SELECT path "
        "FROM files "
        "WHERE id = ? AND owner = ?"
    );

    query.addBindValue(fileId);
    query.addBindValue(owner);

    if (!query.exec())
    {
        qDebug() << "Get file path failed:"
            << query.lastError().text();

        return "";
    }

    if (!query.next())
    {
        qDebug() << "File not found or permission denied.";

        return "";
    }

    QString filePath =
        query.value("path").toString();

    // 再确认磁盘上的真实文件确实存在
    if (!QFile::exists(filePath))
    {
        qDebug() << "Physical file does not exist:"
            << filePath;

        return "";
    }

    return filePath;
}

QString getFileSha256(
    QSqlDatabase& database,
    int fileId,
    const QString& owner)
{
    QSqlQuery query(database);

    query.prepare(
        "SELECT v.sha256 "
        "FROM files f "
        "INNER JOIN file_versions v "
        "ON v.file_id = f.id "
        "AND v.path = f.path "
        "WHERE f.id = ? "
        "AND f.owner = ? "
        "AND v.is_complete = 1 "
        "LIMIT 1"
    );

    query.addBindValue(fileId);
    query.addBindValue(owner);

    if (!query.exec())
    {
        qDebug() << "Get file SHA-256 failed:"
            << query.lastError().text();

        return "";
    }

    if (!query.next())
    {
        qDebug() << "File not found or permission denied.";

        return "";
    }

    const QString sha256 =
        query.value("sha256")
        .toString()
        .trimmed()
        .toLower();

    if (sha256.isEmpty())
    {
        qDebug() << "File SHA-256 is missing.";
        return "";
    }

    return sha256;
}
QString getFileVersionSha256(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    if (versionNumber <= 0)
    {
        qDebug() << "Invalid version number.";
        return "";
    }

    QSqlQuery query(database);

    query.prepare(
        "SELECT v.sha256 "
        "FROM file_versions v "
        "INNER JOIN files f "
        "ON v.file_id = f.id "
        "WHERE v.file_id = ? "
        "AND v.version_number = ? "
        "AND f.owner = ? "
        "AND v.is_complete = 1 "
        "LIMIT 1"
    );

    query.addBindValue(fileId);
    query.addBindValue(versionNumber);
    query.addBindValue(owner);

    if (!query.exec())
    {
        qDebug() << "Get file version SHA-256 failed:"
            << query.lastError().text();

        return "";
    }

    if (!query.next())
    {
        qDebug() << "File version not found or permission denied.";

        return "";
    }

    const QString sha256 =
        query.value("sha256")
        .toString()
        .trimmed()
        .toLower();

    if (sha256.isEmpty())
    {
        qDebug() << "File version SHA-256 is missing.";
        return "";
    }

    return sha256;
}

// =====================================================
// 8. 查询文件的全部历史版本
// =====================================================
void listFileVersions(
    QSqlDatabase& database,
    int fileId,
    const QString& owner)
{
    QSqlQuery query(database);

    query.prepare(
        "SELECT v.version_number, v.size, v.path, "
        "v.sha256, v.upload_time, v.is_complete "
        "FROM file_versions v "
        "INNER JOIN files f ON v.file_id = f.id "
        "WHERE v.file_id = ? AND f.owner = ? "
        "ORDER BY v.version_number ASC"
    );

    query.addBindValue(fileId);
    query.addBindValue(owner);

    if (!query.exec())
    {
        qDebug() << "Query file versions failed:"
            << query.lastError().text();

        return;
    }

    qDebug() << "";
    qDebug() << "===== File Version List =====";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Owner:" << owner;

    bool hasVersion = false;

    while (query.next())
    {
        hasVersion = true;

        qDebug() << "Version:"
            << query.value("version_number").toInt();

        qDebug() << "Size:"
            << query.value("size").toLongLong();

        qDebug() << "Path:"
            << query.value("path").toString();

        qDebug() << "SHA-256:"
            << query.value("sha256").toString();

        qDebug() << "Upload time:"
            << query.value("upload_time").toString();

        qDebug() << "Complete:"
            << query.value("is_complete").toBool();

        qDebug() << "----------------";
    }

    if (!hasVersion)
    {
        qDebug() << "No versions found.";
    }

    qDebug() << "=============================";
}

// =====================================================
// 9. 为现有文件添加新版本
// =====================================================
bool addFileVersion(
    QSqlDatabase& database,
    int fileId,
    const QString& sourcePath,
    const QString& owner)
{
    // 检查用户
    if (owner.isEmpty() ||
        owner.contains("/") ||
        owner.contains("\\"))
    {
        qDebug() << "Invalid owner.";

        return false;
    }

    // 检查新版本源文件
    QFileInfo sourceInfo(sourcePath);

    if (!sourceInfo.exists() || !sourceInfo.isFile())
    {
        qDebug() << "Version source file does not exist:"
            << sourcePath;

        return false;
    }
    QString fileSha256 =
        calculateFileSha256(sourcePath);

    if (fileSha256.isEmpty())
    {
        qDebug() << "Calculate version SHA-256 failed.";
        return false;
    }

    // 查询逻辑文件，并验证所有者
    QSqlQuery fileQuery(database);

    fileQuery.prepare(
        "SELECT filename "
        "FROM files "
        "WHERE id = ? AND owner = ?"
    );

    fileQuery.addBindValue(fileId);
    fileQuery.addBindValue(owner);

    if (!fileQuery.exec())
    {
        qDebug() << "Find logical file failed:"
            << fileQuery.lastError().text();

        return false;
    }

    if (!fileQuery.next())
    {
        qDebug() << "File not found or permission denied.";

        return false;
    }

    QString logicalFileName =
        fileQuery.value("filename").toString();

    // 计算下一个版本号
    QSqlQuery versionQuery(database);

    versionQuery.prepare(
        "SELECT COALESCE(MAX(version_number), 0) + 1 "
        "AS next_version "
        "FROM file_versions "
        "WHERE file_id = ?"
    );

    versionQuery.addBindValue(fileId);

    if (!versionQuery.exec() || !versionQuery.next())
    {
        qDebug() << "Get next version number failed:"
            << versionQuery.lastError().text();

        return false;
    }

    int nextVersionNumber =
        versionQuery.value("next_version").toInt();

    // 创建该文件专用的版本目录
    QString versionDirectory =
        "data/files/" +
        owner +
        "/" +
        QString::number(fileId);

    QDir storageDirectory;

    if (!storageDirectory.exists(versionDirectory))
    {
        if (!storageDirectory.mkpath(versionDirectory))
        {
            qDebug() << "Create version directory failed:"
                << versionDirectory;

            return false;
        }
    }

    // 生成新版本的磁盘路径
    QString versionFileName =
        QString("v%1_%2")
        .arg(nextVersionNumber)
        .arg(logicalFileName);

    QString targetPath =
        versionDirectory +
        "/" +
        versionFileName;

    if (QFile::exists(targetPath))
    {
        qDebug() << "Version physical file already exists:"
            << targetPath;

        return false;
    }

    // 先将新版本复制到服务器存储目录
    if (!QFile::copy(sourcePath, targetPath))
    {
        qDebug() << "Copy new version failed.";

        return false;
    }

    qint64 fileSize = sourceInfo.size();

    QString uploadTime =
        QDateTime::currentDateTime()
        .toString("yyyy-MM-dd HH:mm:ss");

    // 开启数据库事务
    if (!database.transaction())
    {
        qDebug() << "Start version transaction failed.";

        QFile::remove(targetPath);

        return false;
    }

    // 写入历史版本表
    QSqlQuery insertQuery(database);

    insertQuery.prepare(
        "INSERT INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "VALUES (?, ?, ?, ?, ?, ?, 1)"
    );

    insertQuery.addBindValue(fileId);
    insertQuery.addBindValue(nextVersionNumber);
    insertQuery.addBindValue(fileSize);
    insertQuery.addBindValue(targetPath);
    insertQuery.addBindValue(fileSha256);
    insertQuery.addBindValue(uploadTime);

    if (!insertQuery.exec())
    {
        qDebug() << "Insert new version failed:"
            << insertQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    // files表始终指向最新版本
    QSqlQuery updateQuery(database);

    updateQuery.prepare(
        "UPDATE files "
        "SET size = ?, path = ?, upload_time = ? "
        "WHERE id = ? AND owner = ?"
    );

    updateQuery.addBindValue(fileSize);
    updateQuery.addBindValue(targetPath);
    updateQuery.addBindValue(uploadTime);
    updateQuery.addBindValue(fileId);
    updateQuery.addBindValue(owner);

    if (!updateQuery.exec() ||
        updateQuery.numRowsAffected() != 1)
    {
        qDebug() << "Update latest file version failed:"
            << updateQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    if (!database.commit())
    {
        qDebug() << "Commit new version failed:"
            << database.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    qDebug() << "New file version added successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Version:" << nextVersionNumber;
    qDebug() << "Path:" << targetPath;

    return true;
}
bool addUploadedFileVersion(
    QSqlDatabase& database,
    int fileId,
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256)
{
    QFileInfo fileInfo(temporaryFilePath);

    if (!fileInfo.exists() ||
        !fileInfo.isFile())
    {
        qDebug() << "Temporary version file does not exist:"
            << temporaryFilePath;

        return false;
    }

    const QString normalizedClientSha256 =
        clientSha256.trimmed().toLower();

    if (!isValidSha256Value(
        normalizedClientSha256))
    {
        qDebug() << "Invalid client version SHA-256 format.";

        if (!QFile::remove(temporaryFilePath))
        {
            qDebug() << "Failed to remove invalid version file:"
                << temporaryFilePath;
        }

        return false;
    }

    const QString serverSha256 =
        calculateFileSha256(temporaryFilePath)
        .toLower();

    if (serverSha256.isEmpty())
    {
        qDebug() << "Calculate uploaded version SHA-256 failed.";

        QFile::remove(temporaryFilePath);
        return false;
    }

    if (serverSha256 != normalizedClientSha256)
    {
        qDebug() << "Uploaded version SHA-256 mismatch.";
        qDebug() << "Client SHA-256:"
            << normalizedClientSha256;
        qDebug() << "Server SHA-256:"
            << serverSha256;

        if (!QFile::remove(temporaryFilePath))
        {
            qDebug() << "Failed to remove corrupted version file:"
                << temporaryFilePath;
        }

        return false;
    }

    qDebug() << "Uploaded version SHA-256 verified.";

    const bool isVersionAdded =
        addFileVersion(
            database,
            fileId,
            temporaryFilePath,
            owner
        );

    // addFileVersion会复制到正式版本目录
    if (!QFile::remove(temporaryFilePath))
    {
        qDebug() << "Warning: failed to clean temporary version file:"
            << temporaryFilePath;
    }

    return isVersionAdded;
}

// =====================================================
// 10. 获取指定历史版本的真实存储路径
// =====================================================
QString getFileVersionPath(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    if (versionNumber <= 0)
    {
        qDebug() << "Invalid version number.";
        return "";
    }

    QSqlQuery query(database);

    query.prepare(
        "SELECT v.path "
        "FROM file_versions v "
        "INNER JOIN files f ON v.file_id = f.id "
        "WHERE v.file_id = ? "
        "AND v.version_number = ? "
        "AND f.owner = ? "
        "AND v.is_complete = 1"
    );

    query.addBindValue(fileId);
    query.addBindValue(versionNumber);
    query.addBindValue(owner);

    if (!query.exec())
    {
        qDebug() << "Get file version path failed:"
            << query.lastError().text();

        return "";
    }

    if (!query.next())
    {
        qDebug() << "File version not found or permission denied.";
        return "";
    }

    QString versionPath =
        query.value("path").toString();

    if (!QFile::exists(versionPath))
    {
        qDebug() << "Version physical file does not exist:"
            << versionPath;

        return "";
    }

    return versionPath;
}


// =====================================================
// 11. 恢复指定历史版本
// =====================================================
bool restoreFileVersion(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    QString versionPath =
        getFileVersionPath(
            database,
            fileId,
            versionNumber,
            owner
        );

    if (versionPath.isEmpty())
    {
        qDebug() << "Restore file version failed.";
        return false;
    }

    // 恢复时不覆盖历史记录，
    // 而是将指定版本复制为一个新的最新版本
    if (!addFileVersion(
        database,
        fileId,
        versionPath,
        owner))
    {
        qDebug() << "Create restored version failed.";
        return false;
    }

    qDebug() << "File version restored successfully!";
    qDebug() << "Restored from version:" << versionNumber;

    return true;
}


// =====================================================
// 12. 删除指定历史版本
// =====================================================
bool deleteFileVersion(
    QSqlDatabase& database,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    if (versionNumber <= 0)
    {
        qDebug() << "Invalid version number.";
        return false;
    }

    // 查询指定版本，同时验证文件所有者
    QSqlQuery findQuery(database);

    findQuery.prepare(
        "SELECT v.path, f.path AS latest_path "
        "FROM file_versions v "
        "INNER JOIN files f ON v.file_id = f.id "
        "WHERE v.file_id = ? "
        "AND v.version_number = ? "
        "AND f.owner = ?"
    );

    findQuery.addBindValue(fileId);
    findQuery.addBindValue(versionNumber);
    findQuery.addBindValue(owner);

    if (!findQuery.exec())
    {
        qDebug() << "Find file version failed:"
            << findQuery.lastError().text();

        return false;
    }

    if (!findQuery.next())
    {
        qDebug() << "File version not found or permission denied.";
        return false;
    }

    QString versionPath =
        findQuery.value("path").toString();

    QString latestPath =
        findQuery.value("latest_path").toString();

    // files表指向的版本是当前最新版本，不能单独删除
    if (versionPath == latestPath)
    {
        qDebug() << "The latest version cannot be deleted.";
        return false;
    }

    if (!QFile::exists(versionPath))
    {
        qDebug() << "Version physical file does not exist:"
            << versionPath;

        return false;
    }

    // 先临时改名，数据库操作失败时可以恢复
    QString temporaryPath =
        versionPath + ".deleting";

    if (QFile::exists(temporaryPath))
    {
        qDebug() << "Temporary deletion file already exists:"
            << temporaryPath;

        return false;
    }

    if (!QFile::rename(versionPath, temporaryPath))
    {
        qDebug() << "Prepare version deletion failed.";
        return false;
    }

    if (!database.transaction())
    {
        qDebug() << "Start version deletion transaction failed.";

        QFile::rename(temporaryPath, versionPath);
        return false;
    }

    QSqlQuery deleteQuery(database);

    deleteQuery.prepare(
        "DELETE FROM file_versions "
        "WHERE file_id = ? AND version_number = ?"
    );

    deleteQuery.addBindValue(fileId);
    deleteQuery.addBindValue(versionNumber);

    if (!deleteQuery.exec() ||
        deleteQuery.numRowsAffected() != 1)
    {
        qDebug() << "Delete file version record failed:"
            << deleteQuery.lastError().text();

        database.rollback();
        QFile::rename(temporaryPath, versionPath);

        return false;
    }

    if (!database.commit())
    {
        qDebug() << "Commit version deletion failed:"
            << database.lastError().text();

        database.rollback();
        QFile::rename(temporaryPath, versionPath);

        return false;
    }

    if (!QFile::remove(temporaryPath))
    {
        qDebug() << "Warning: version record was deleted,"
            << "but temporary physical file remains:"
            << temporaryPath;

        return false;
    }

    qDebug() << "File version deleted successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Deleted version:" << versionNumber;

    return true;
}

// =====================================================
// 13. 计算文件的SHA-256值
// =====================================================
QString calculateFileSha256(
    const QString& filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Open file for SHA-256 failed:"
            << filePath;

        return "";
    }

    QCryptographicHash hash(
        QCryptographicHash::Sha256
    );

    const qint64 HASH_BUFFER_SIZE =
        1024 * 1024;

    while (!file.atEnd())
    {
        const QByteArray fileChunk =
            file.read(HASH_BUFFER_SIZE);

        if (fileChunk.isEmpty() &&
            file.error() != QFile::NoError)
        {
            qDebug() << "Read file for SHA-256 failed:"
                << file.errorString();

            return "";
        }

        hash.addData(fileChunk);
    }

    QString sha256 =
        QString::fromLatin1(
            hash.result().toHex()
        );

    qDebug() << "SHA-256 calculated successfully:";
    qDebug() << sha256;

    return sha256;
}

// =====================================================
// 14. 为旧版本自动补齐SHA-256
// =====================================================
static bool backfillMissingSha256(
    QSqlDatabase& database)
{
    QSqlQuery selectQuery(database);

    if (!selectQuery.exec(
        "SELECT id, path "
        "FROM file_versions "
        "WHERE sha256 IS NULL OR sha256 = ''"))
    {
        qDebug() << "Query missing SHA-256 failed:"
            << selectQuery.lastError().text();

        return false;
    }

    QList<QPair<int, QString>> versionFiles;

    while (selectQuery.next())
    {
        versionFiles.append(
            qMakePair(
                selectQuery.value("id").toInt(),
                selectQuery.value("path").toString()
            )
        );
    }

    int updatedCount = 0;

    for (const auto& versionFile : versionFiles)
    {
        int versionId = versionFile.first;
        QString versionPath = versionFile.second;

        QString sha256 =
            calculateFileSha256(versionPath);

        if (sha256.isEmpty())
        {
            qDebug() << "Skip missing or unreadable version:"
                << versionPath;

            continue;
        }

        QSqlQuery updateQuery(database);

        updateQuery.prepare(
            "UPDATE file_versions "
            "SET sha256 = ? "
            "WHERE id = ?"
        );

        updateQuery.addBindValue(sha256);
        updateQuery.addBindValue(versionId);

        if (!updateQuery.exec())
        {
            qDebug() << "Update SHA-256 failed:"
                << updateQuery.lastError().text();

            return false;
        }

        ++updatedCount;
    }

    qDebug() << "Missing SHA-256 records updated:"
        << updatedCount;

    return true;
}

// =====================================================
// 15. 根据SHA-256查找服务器已有文件
// =====================================================
QString findFilePathBySha256(
    QSqlDatabase& database,
    const QString& sha256)
{
    const QString normalizedSha256 =
        sha256.trimmed().toLower();

    if (!isValidSha256Value(normalizedSha256))
    {
        qDebug() << "Invalid SHA-256 value.";
        return "";
    }

    QSqlQuery query(database);

    query.prepare(
        "SELECT path "
        "FROM file_versions "
        "WHERE sha256 = ? "
        "AND is_complete = 1 "
        "ORDER BY id ASC"
    );

    query.addBindValue(normalizedSha256);

    if (!query.exec())
    {
        qDebug() << "Find duplicate file failed:"
            << query.lastError().text();

        return "";
    }

    QStringList candidatePaths;

    while (query.next())
    {
        candidatePaths.append(
            query.value("path").toString()
        );
    }

    query.finish();

    for (const QString& existingPath :
        candidatePaths)
    {
        bool isPhysicalFileValid =
            QFile::exists(existingPath);

        if (isPhysicalFileValid)
        {
            const QString actualSha256 =
                calculateFileSha256(existingPath)
                .toLower();

            isPhysicalFileValid =
                !actualSha256.isEmpty() &&
                actualSha256 ==
                normalizedSha256;
        }

        if (isPhysicalFileValid)
        {
            qDebug() << "Verified duplicate file found:";
            qDebug() << existingPath;

            return existingPath;
        }

        // 文件丢失或内容损坏，将对应版本标记为不完整
        QSqlQuery invalidateQuery(database);

        invalidateQuery.prepare(
            "UPDATE file_versions "
            "SET is_complete = 0 "
            "WHERE path = ?"
        );

        invalidateQuery.addBindValue(existingPath);

        if (!invalidateQuery.exec())
        {
            qDebug() << "Invalidate damaged file failed:"
                << invalidateQuery.lastError().text();
        }

        qDebug() << "Damaged duplicate candidate rejected:"
            << existingPath;
    }

    qDebug() << "No verified duplicate file found.";
    return "";
}

// =====================================================
// 16. 根据SHA-256完成基础秒传
// =====================================================
bool instantUploadFile(
    QSqlDatabase& database,
    const QString& fileName,
    const QString& sha256,
    const QString& owner)
{
    if (owner.isEmpty() ||
        owner.contains("/") ||
        owner.contains("\\"))
    {
        qDebug() << "Invalid owner.";
        return false;
    }

    if (fileName.isEmpty() ||
        fileName.contains("/") ||
        fileName.contains("\\"))
    {
        qDebug() << "Invalid filename.";
        return false;
    }

    QString normalizedSha256 =
        sha256.trimmed().toLower();

    if (!isValidSha256Value(normalizedSha256))
    {
        qDebug() << "Invalid SHA-256 value.";
        return false;
    }

    // 查找服务器中已经存在的相同内容
    QString existingPath =
        findFilePathBySha256(
            database,
            normalizedSha256
        );

    if (existingPath.isEmpty())
    {
        qDebug() << "Instant upload unavailable:"
            << "matching content was not found.";

        return false;
    }

    QFileInfo existingInfo(existingPath);

    if (!existingInfo.exists() ||
        !existingInfo.isFile())
    {
        qDebug() << "Existing physical file is unavailable.";
        return false;
    }

    // 同一用户不能存在同名逻辑文件
    QSqlQuery duplicateQuery(database);

    duplicateQuery.prepare(
        "SELECT id "
        "FROM files "
        "WHERE filename = ? AND owner = ?"
    );

    duplicateQuery.addBindValue(fileName);
    duplicateQuery.addBindValue(owner);

    if (!duplicateQuery.exec())
    {
        qDebug() << "Check instant-upload duplicate failed:"
            << duplicateQuery.lastError().text();

        return false;
    }

    if (duplicateQuery.next())
    {
        qDebug() << "File already exists for this user:"
            << fileName;

        return false;
    }

    qint64 fileSize =
        existingInfo.size();

    QString uploadTime =
        QDateTime::currentDateTime()
        .toString("yyyy-MM-dd HH:mm:ss");

    if (!database.transaction())
    {
        qDebug() << "Start instant-upload transaction failed.";
        return false;
    }

    // 先建立逻辑文件记录以获得fileId
    QSqlQuery insertFileQuery(database);

    insertFileQuery.prepare(
        "INSERT INTO files "
        "(filename, size, owner, path, upload_time) "
        "VALUES (?, ?, ?, '', ?)"
    );

    insertFileQuery.addBindValue(fileName);
    insertFileQuery.addBindValue(fileSize);
    insertFileQuery.addBindValue(owner);
    insertFileQuery.addBindValue(uploadTime);

    if (!insertFileQuery.exec())
    {
        qDebug() << "Insert instant-upload file failed:"
            << insertFileQuery.lastError().text();

        database.rollback();
        return false;
    }

    int fileId =
        insertFileQuery.lastInsertId().toInt();

    QString versionDirectory =
        "data/files/" +
        owner +
        "/" +
        QString::number(fileId);

    QDir storageDirectory;

    if (!storageDirectory.exists(versionDirectory) &&
        !storageDirectory.mkpath(versionDirectory))
    {
        qDebug() << "Create instant-upload directory failed:"
            << versionDirectory;

        database.rollback();
        return false;
    }

    QString targetPath =
        versionDirectory +
        "/v1_" +
        fileName;

    if (QFile::exists(targetPath))
    {
        qDebug() << "Instant-upload target already exists:"
            << targetPath;

        database.rollback();
        return false;
    }

    // 服务器内部复制，无需客户端重新传输文件内容
    if (!QFile::copy(existingPath, targetPath))
    {
        qDebug() << "Server-side instant copy failed.";

        database.rollback();
        return false;
    }

    QSqlQuery updateFileQuery(database);

    updateFileQuery.prepare(
        "UPDATE files "
        "SET path = ? "
        "WHERE id = ? AND owner = ?"
    );

    updateFileQuery.addBindValue(targetPath);
    updateFileQuery.addBindValue(fileId);
    updateFileQuery.addBindValue(owner);

    if (!updateFileQuery.exec() ||
        updateFileQuery.numRowsAffected() != 1)
    {
        qDebug() << "Update instant-upload path failed:"
            << updateFileQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    QSqlQuery insertVersionQuery(database);

    insertVersionQuery.prepare(
        "INSERT INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "VALUES (?, 1, ?, ?, ?, ?, 1)"
    );

    insertVersionQuery.addBindValue(fileId);
    insertVersionQuery.addBindValue(fileSize);
    insertVersionQuery.addBindValue(targetPath);
    insertVersionQuery.addBindValue(normalizedSha256);
    insertVersionQuery.addBindValue(uploadTime);

    if (!insertVersionQuery.exec())
    {
        qDebug() << "Insert instant-upload version failed:"
            << insertVersionQuery.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    if (!database.commit())
    {
        qDebug() << "Commit instant upload failed:"
            << database.lastError().text();

        database.rollback();
        QFile::remove(targetPath);

        return false;
    }

    qDebug() << "Instant upload completed successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Filename:" << fileName;
    qDebug() << "SHA-256:" << normalizedSha256;
    qDebug() << "Path:" << targetPath;

    return true;
}

static bool recoverInterruptedDeletions(
    QSqlDatabase& database)
{
    const QString STORAGE_DIRECTORY =
        "data/files";

    QDir storageDirectory(STORAGE_DIRECTORY);

    if (!storageDirectory.exists())
    {
        return true;
    }

    QDirIterator iterator(
        STORAGE_DIRECTORY,
        QStringList() << "*.deleting*",
        QDir::Files,
        QDirIterator::Subdirectories
    );

    int restoredFileCount = 0;
    int removedFileCount = 0;
    int conflictFileCount = 0;

    while (iterator.hasNext())
    {
        const QString temporaryPath =
            iterator.next();

        QString originalPath;

        const int markerPosition =
            temporaryPath.lastIndexOf(".deleting.");

        if (markerPosition >= 0)
        {
            originalPath =
                temporaryPath.left(markerPosition);
        }
        else if (temporaryPath.endsWith(".deleting"))
        {
            originalPath =
                temporaryPath.left(
                    temporaryPath.length() -
                    QString(".deleting").length()
                );
        }
        else
        {
            continue;
        }

        QSqlQuery referenceQuery(database);

        referenceQuery.prepare(
            "SELECT 1 FROM files "
            "WHERE path = ? "
            "UNION ALL "
            "SELECT 1 FROM file_versions "
            "WHERE path = ? "
            "LIMIT 1"
        );

        referenceQuery.addBindValue(originalPath);
        referenceQuery.addBindValue(originalPath);

        if (!referenceQuery.exec())
        {
            qDebug() << "Check temporary file reference failed:"
                << referenceQuery.lastError().text();

            return false;
        }

        const bool isReferenced =
            referenceQuery.next();

        if (isReferenced)
        {
            // 数据库仍引用原路径，说明删除事务没有提交
            if (!QFile::exists(originalPath))
            {
                if (!QFile::rename(
                    temporaryPath,
                    originalPath))
                {
                    qDebug() << "Failed to restore interrupted file:"
                        << temporaryPath;

                    return false;
                }

                ++restoredFileCount;

                qDebug() << "Interrupted file restored:"
                    << originalPath;
            }
            else
            {
                // 原文件和临时文件同时存在，不能贸然覆盖
                ++conflictFileCount;

                qDebug() << "Warning: recovery conflict detected:"
                    << temporaryPath;
            }
        }
        else
        {
            // 数据库已不再引用原路径，说明事务已经提交
            if (QFile::remove(temporaryPath))
            {
                ++removedFileCount;

                qDebug() << "Unreferenced temporary file removed:"
                    << temporaryPath;
            }
            else
            {
                qDebug() << "Warning: failed to remove temporary file:"
                    << temporaryPath;
            }
        }
    }

    qDebug() << "Interrupted deletion recovery completed.";
    qDebug() << "Restored files:" << restoredFileCount;
    qDebug() << "Removed temporary files:" << removedFileCount;
    qDebug() << "Recovery conflicts:" << conflictFileCount;

    return true;
}

} // namespace detail

FileManager::FileManager(
    QSqlDatabase& database,
    StorageEngine& storageEngine)
    : m_database(database),
      m_storageEngine(storageEngine)
{
}

bool FileManager::initialize(const QString& databasePath)
{
    if (!m_storageEngine.initialize())
    {
        return false;
    }

    return detail::initDatabase(m_database, databasePath);
}

bool FileManager::addFile(
    const QString& sourcePath,
    const QString& owner)
{
    return detail::addFile(m_database, sourcePath, owner);
}

bool FileManager::addUploadedFile(
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256)
{
    return detail::addUploadedFile(
        m_database,
        temporaryFilePath,
        owner,
        clientSha256
    );
}

void FileManager::listFiles(const QString& owner)
{
    detail::listFiles(m_database, owner);
}

bool FileManager::deleteFile(int fileId, const QString& owner)
{
    return detail::deleteFile(m_database, fileId, owner);
}

bool FileManager::renameFile(
    int fileId,
    const QString& owner,
    const QString& newFileName)
{
    return detail::renameFile(
        m_database,
        fileId,
        owner,
        newFileName
    );
}

QString FileManager::getFilePath(
    int fileId,
    const QString& owner)
{
    return detail::getFilePath(m_database, fileId, owner);
}

QString FileManager::getFileSha256(
    int fileId,
    const QString& owner)
{
    return detail::getFileSha256(m_database, fileId, owner);
}

QString FileManager::getFileVersionSha256(
    int fileId,
    int versionNumber,
    const QString& owner)
{
    return detail::getFileVersionSha256(
        m_database,
        fileId,
        versionNumber,
        owner
    );
}

void FileManager::listFileVersions(
    int fileId,
    const QString& owner)
{
    detail::listFileVersions(m_database, fileId, owner);
}

bool FileManager::addFileVersion(
    int fileId,
    const QString& sourcePath,
    const QString& owner)
{
    return detail::addFileVersion(
        m_database,
        fileId,
        sourcePath,
        owner
    );
}

bool FileManager::addUploadedFileVersion(
    int fileId,
    const QString& temporaryFilePath,
    const QString& owner,
    const QString& clientSha256)
{
    return detail::addUploadedFileVersion(
        m_database,
        fileId,
        temporaryFilePath,
        owner,
        clientSha256
    );
}

QString FileManager::getFileVersionPath(
    int fileId,
    int versionNumber,
    const QString& owner)
{
    return detail::getFileVersionPath(
        m_database,
        fileId,
        versionNumber,
        owner
    );
}

bool FileManager::restoreFileVersion(
    int fileId,
    int versionNumber,
    const QString& owner)
{
    return detail::restoreFileVersion(
        m_database,
        fileId,
        versionNumber,
        owner
    );
}

bool FileManager::deleteFileVersion(
    int fileId,
    int versionNumber,
    const QString& owner)
{
    return detail::deleteFileVersion(
        m_database,
        fileId,
        versionNumber,
        owner
    );
}

bool FileManager::instantUploadFile(
    const QString& fileName,
    const QString& sha256,
    const QString& owner)
{
    return detail::instantUploadFile(
        m_database,
        fileName,
        sha256,
        owner
    );
}

} // namespace server
} // namespace netdisk
