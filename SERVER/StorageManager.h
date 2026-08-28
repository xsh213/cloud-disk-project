#pragma once

#include <QSqlDatabase>
#include <QString>

// 初始化数据库
bool initDatabase(QSqlDatabase& db);

// 初始化服务器存储目录
bool initStorageDirectory();

// 新增文件
bool addFile(
    QSqlDatabase& db,
    const QString& sourcePath,
    const QString& owner
);

// 查询指定用户的文件列表
void listFiles(
    QSqlDatabase& db,
    const QString& owner
);

// 删除文件
bool deleteFile(
    QSqlDatabase& db,
    int fileId,
    const QString& owner
);

// 重命名文件
bool renameFile(
    QSqlDatabase& db,
    int fileId,
    const QString& owner,
    const QString& newFilename
);

// 获取文件的真实存储路径
QString getFilePath(
    QSqlDatabase& db,
    int fileId,
    const QString& owner
);

// 查询文件的全部历史版本
void listFileVersions(
    QSqlDatabase& db,
    int fileId,
    const QString& owner
);

// 为现有文件添加一个新版本
bool addFileVersion(
    QSqlDatabase& db,
    int fileId,
    const QString& sourcePath,
    const QString& owner
);
