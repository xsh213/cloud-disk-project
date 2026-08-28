#include "StorageManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>

// =====================================================
// 1. 初始化数据库
// =====================================================
bool initDatabase(QSqlDatabase& db)
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("server.db");

    if (!db.open())
    {
        qDebug() << "Database open failed:"
            << db.lastError().text();

        return false;
    }

    qDebug() << "Database opened successfully!";

    QSqlQuery query(db);

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

    return true;
}


// =====================================================
// 2. 初始化服务器总存储目录
// =====================================================
bool initStorageDirectory()
{
    QString storageDir = "data/files";

    QDir dir;

    // 如果目录不存在，则自动创建
    if (!dir.exists(storageDir))
    {
        if (!dir.mkpath(storageDir))
        {
            qDebug() << "Failed to create storage directory.";

            return false;
        }
    }

    qDebug() << "Storage directory ready:"
        << storageDir;

    return true;
}


// =====================================================
// 3. 添加文件
// =====================================================
bool addFile(
    QSqlDatabase& db,
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

    QString filename = fileInfo.fileName();
    qint64 fileSize = fileInfo.size();

    QString userStorageDir =
        "data/files/" + owner;

    QDir dir;

    if (!dir.exists(userStorageDir) &&
        !dir.mkpath(userStorageDir))
    {
        qDebug() << "Failed to create user storage directory:"
            << userStorageDir;

        return false;
    }

    QString targetPath =
        userStorageDir + "/" + filename;

    // 检查数据库中是否存在同名文件
    QSqlQuery checkQuery(db);

    checkQuery.prepare(
        "SELECT id "
        "FROM files "
        "WHERE filename = ? AND owner = ?"
    );

    checkQuery.addBindValue(filename);
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
            << filename;

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
    if (!db.transaction())
    {
        qDebug() << "Failed to start add-file transaction.";

        QFile::remove(targetPath);
        return false;
    }

    QSqlQuery insertFileQuery(db);

    insertFileQuery.prepare(
        "INSERT INTO files "
        "(filename, size, owner, path, upload_time) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    insertFileQuery.addBindValue(filename);
    insertFileQuery.addBindValue(fileSize);
    insertFileQuery.addBindValue(owner);
    insertFileQuery.addBindValue(targetPath);
    insertFileQuery.addBindValue(uploadTime);

    if (!insertFileQuery.exec())
    {
        qDebug() << "Insert file record failed:"
            << insertFileQuery.lastError().text();

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    int fileId =
        insertFileQuery.lastInsertId().toInt();

    // 新文件立即登记为版本1，不需要重启服务器
    QSqlQuery insertVersionQuery(db);

    insertVersionQuery.prepare(
        "INSERT INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "VALUES (?, 1, ?, ?, NULL, ?, 1)"
    );

    insertVersionQuery.addBindValue(fileId);
    insertVersionQuery.addBindValue(fileSize);
    insertVersionQuery.addBindValue(targetPath);
    insertVersionQuery.addBindValue(uploadTime);

    if (!insertVersionQuery.exec())
    {
        qDebug() << "Insert version 1 failed:"
            << insertVersionQuery.lastError().text();

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    if (!db.commit())
    {
        qDebug() << "Commit add-file transaction failed:"
            << db.lastError().text();

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    qDebug() << "File added successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Initial version: 1";
    qDebug() << "Path:" << targetPath;

    return true;
}

// =====================================================
// 4. 查询某个用户的文件列表
// =====================================================
void listFiles(QSqlDatabase& db,
    const QString& owner)
{
    QSqlQuery selectQuery(db);


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


        int id =
            selectQuery.value("id").toInt();


        QString filename =
            selectQuery.value("filename").toString();


        qint64 size =
            selectQuery.value("size").toLongLong();


        QString path =
            selectQuery.value("path").toString();


        QString uploadTime =
            selectQuery.value("upload_time").toString();


        qDebug() << "ID:" << id;
        qDebug() << "Filename:" << filename;
        qDebug() << "Size:" << size;
        qDebug() << "Path:" << path;
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
bool deleteFile(QSqlDatabase& db,
    int fileId,
    const QString& owner)
{
    // -------------------------------------------------
    // 查询指定文件
    // -------------------------------------------------
    QSqlQuery findQuery(db);


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


    // 文件不存在或不属于当前用户
    if (!findQuery.next())
    {
        qDebug() << "File not found or permission denied.";

        return false;
    }


    QString filename =
        findQuery.value("filename").toString();


    QString filePath =
        findQuery.value("path").toString();


    // -------------------------------------------------
    // 检查真实文件是否存在
    // -------------------------------------------------
    if (!QFile::exists(filePath))
    {
        qDebug() << "Physical file does not exist:"
            << filePath;

        return false;
    }


    // -------------------------------------------------
    // 临时修改文件名
    //
    // 这样数据库删除失败时
    // 还能把文件恢复回来
    // -------------------------------------------------
    QString tempPath =
        filePath + ".deleting";


    if (QFile::exists(tempPath))
    {
        QFile::remove(tempPath);
    }


    if (!QFile::rename(filePath, tempPath))
    {
        qDebug() << "Failed to prepare file for deletion.";

        return false;
    }


    // -------------------------------------------------
    // 开启 SQLite 事务
    // -------------------------------------------------
    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction.";


        QFile::rename(
            tempPath,
            filePath
        );


        return false;
    }


    // -------------------------------------------------
    // 删除数据库记录
    // -------------------------------------------------
    QSqlQuery deleteQuery(db);


    deleteQuery.prepare(
        "DELETE FROM files "
        "WHERE id = ? AND owner = ?"
    );


    deleteQuery.addBindValue(fileId);
    deleteQuery.addBindValue(owner);


    if (!deleteQuery.exec())
    {
        qDebug() << "Delete database record failed:"
            << deleteQuery.lastError().text();


        db.rollback();


        QFile::rename(
            tempPath,
            filePath
        );


        return false;
    }


    // -------------------------------------------------
    // 提交数据库事务
    // -------------------------------------------------
    if (!db.commit())
    {
        qDebug() << "Database commit failed:"
            << db.lastError().text();


        db.rollback();


        QFile::rename(
            tempPath,
            filePath
        );


        return false;
    }


    // -------------------------------------------------
    // 真正删除磁盘文件
    // -------------------------------------------------
    if (!QFile::remove(tempPath))
    {
        qDebug() << "Warning: database record deleted,"
            << "but physical file could not be removed:"
            << tempPath;

        return false;
    }


    qDebug() << "File deleted successfully:"
        << filename;


    return true;
}


// =====================================================
// 6. 重命名文件
// =====================================================
bool renameFile(QSqlDatabase& db,
    int fileId,
    const QString& owner,
    const QString& newFilename)
{
    // -------------------------------------------------
    // 检查新文件名
    // -------------------------------------------------
    if (newFilename.isEmpty())
    {
        qDebug() << "New filename cannot be empty.";

        return false;
    }


    // -------------------------------------------------
    // 防止新文件名中出现路径
    // -------------------------------------------------
    if (newFilename.contains("/") ||
        newFilename.contains("\\"))
    {
        qDebug() << "Invalid filename.";

        return false;
    }


    // -------------------------------------------------
    // 查询原文件
    // -------------------------------------------------
    QSqlQuery findQuery(db);


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


    QString oldFilename =
        findQuery.value("filename").toString();


    QString oldPath =
        findQuery.value("path").toString();


    // -------------------------------------------------
    // 如果新旧文件名完全相同
    // -------------------------------------------------
    if (oldFilename == newFilename)
    {
        qDebug() << "New filename is the same as the old filename.";

        return false;
    }


    QFileInfo oldFileInfo(oldPath);


    QString directory =
        oldFileInfo.absolutePath();


    QString newPath =
        directory + "/" + newFilename;


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
    QSqlQuery updateQuery(db);


    updateQuery.prepare(
        "UPDATE files "
        "SET filename = ?, path = ? "
        "WHERE id = ? AND owner = ?"
    );


    updateQuery.addBindValue(newFilename);
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
    qDebug() << oldFilename
        << "->"
        << newFilename;


    return true;
}

// =====================================================
// 7. 根据文件 ID 获取真实文件路径
// =====================================================
QString getFilePath(QSqlDatabase& db,
    int fileId,
    const QString& owner)
{
    QSqlQuery query(db);

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

// =====================================================
// 8. 查询文件的全部历史版本
// =====================================================
void listFileVersions(
    QSqlDatabase& db,
    int fileId,
    const QString& owner)
{
    QSqlQuery query(db);

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
    QSqlDatabase& db,
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

    // 查询逻辑文件，并验证所有者
    QSqlQuery fileQuery(db);

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

    QString logicalFilename =
        fileQuery.value("filename").toString();

    // 计算下一个版本号
    QSqlQuery versionQuery(db);

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

    int nextVersion =
        versionQuery.value("next_version").toInt();

    // 创建该文件专用的版本目录
    QString versionDirectory =
        "data/files/" +
        owner +
        "/" +
        QString::number(fileId);

    QDir dir;

    if (!dir.exists(versionDirectory))
    {
        if (!dir.mkpath(versionDirectory))
        {
            qDebug() << "Create version directory failed:"
                << versionDirectory;

            return false;
        }
    }

    // 生成新版本的磁盘路径
    QString versionFilename =
        QString("v%1_%2")
        .arg(nextVersion)
        .arg(logicalFilename);

    QString targetPath =
        versionDirectory +
        "/" +
        versionFilename;

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
    if (!db.transaction())
    {
        qDebug() << "Start version transaction failed.";

        QFile::remove(targetPath);

        return false;
    }

    // 写入历史版本表
    QSqlQuery insertQuery(db);

    insertQuery.prepare(
        "INSERT INTO file_versions "
        "(file_id, version_number, size, path, sha256, "
        "upload_time, is_complete) "
        "VALUES (?, ?, ?, ?, NULL, ?, 1)"
    );

    insertQuery.addBindValue(fileId);
    insertQuery.addBindValue(nextVersion);
    insertQuery.addBindValue(fileSize);
    insertQuery.addBindValue(targetPath);
    insertQuery.addBindValue(uploadTime);

    if (!insertQuery.exec())
    {
        qDebug() << "Insert new version failed:"
            << insertQuery.lastError().text();

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    // files表始终指向最新版本
    QSqlQuery updateQuery(db);

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

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    if (!db.commit())
    {
        qDebug() << "Commit new version failed:"
            << db.lastError().text();

        db.rollback();
        QFile::remove(targetPath);

        return false;
    }

    qDebug() << "New file version added successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Version:" << nextVersion;
    qDebug() << "Path:" << targetPath;

    return true;
}

// =====================================================
// 10. 获取指定历史版本的真实存储路径
// =====================================================
QString getFileVersionPath(
    QSqlDatabase& db,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    if (versionNumber <= 0)
    {
        qDebug() << "Invalid version number.";
        return "";
    }

    QSqlQuery query(db);

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
    QSqlDatabase& db,
    int fileId,
    int versionNumber,
    const QString& owner)
{
    QString versionPath =
        getFileVersionPath(
            db,
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
        db,
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
    QSqlDatabase& db,
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
    QSqlQuery findQuery(db);

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
    QString tempPath =
        versionPath + ".deleting";

    if (QFile::exists(tempPath))
    {
        qDebug() << "Temporary deletion file already exists:"
            << tempPath;

        return false;
    }

    if (!QFile::rename(versionPath, tempPath))
    {
        qDebug() << "Prepare version deletion failed.";
        return false;
    }

    if (!db.transaction())
    {
        qDebug() << "Start version deletion transaction failed.";

        QFile::rename(tempPath, versionPath);
        return false;
    }

    QSqlQuery deleteQuery(db);

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

        db.rollback();
        QFile::rename(tempPath, versionPath);

        return false;
    }

    if (!db.commit())
    {
        qDebug() << "Commit version deletion failed:"
            << db.lastError().text();

        db.rollback();
        QFile::rename(tempPath, versionPath);

        return false;
    }

    if (!QFile::remove(tempPath))
    {
        qDebug() << "Warning: version record was deleted,"
            << "but temporary physical file remains:"
            << tempPath;

        return false;
    }

    qDebug() << "File version deleted successfully!";
    qDebug() << "File ID:" << fileId;
    qDebug() << "Deleted version:" << versionNumber;

    return true;
}