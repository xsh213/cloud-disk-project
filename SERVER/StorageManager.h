#pragma once

#include <QSqlDatabase>
#include <QString>

namespace netdisk
{
    namespace server
    {

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

// 删除整个文件及其全部版本
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

// 获取文件最新版本的真实存储路径
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

// 获取指定历史版本的真实存储路径
QString getFileVersionPath(
    QSqlDatabase& db,
    int fileId,
    int versionNumber,
    const QString& owner
);

// 恢复指定历史版本
// 恢复操作会生成一个新的最新版本，不会覆盖原历史记录
bool restoreFileVersion(
    QSqlDatabase& db,
    int fileId,
    int versionNumber,
    const QString& owner
);

// 删除指定历史版本
// 不允许删除当前最新版本
bool deleteFileVersion(
    QSqlDatabase& db,
    int fileId,
    int versionNumber,
    const QString& owner
);

// 计算指定文件的SHA-256值
QString calculateFileSha256(
    const QString& filePath
);

// 根据SHA-256查找服务器中已有的完整文件
// 找不到时返回空字符串
QString findFilePathBySha256(
    QSqlDatabase& db,
    const QString& sha256
);

// 根据服务器已有内容完成秒传
bool instantUploadFile(
    QSqlDatabase& db,
    const QString& filename,
    const QString& sha256,
    const QString& owner
);

} // namespace server
} // namespace netdisk