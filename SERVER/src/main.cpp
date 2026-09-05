#include <QCoreApplication>
#include "FileManager.h"
#include "StorageEngine.h"

#include <QSqlDatabase>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

using netdisk::server::FileManager;
using netdisk::server::StorageEngine;

// =====================================================
// 主函数
// =====================================================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);


    QSqlDatabase database;
    StorageEngine storageEngine;
    FileManager fileManager(database, storageEngine);


    // -------------------------------------------------
    // 初始化 SQLite 数据库
    // -------------------------------------------------
    if (!fileManager.initialize("server.db"))
    {
        return -1;
    }


    // 当前测试用户
    const QString currentUser =
        "user01";
 
    // =================================================
    // =================================================
    // 测试区域
    //
    // 当前开发阶段用于单独测试各个功能。
    //
    // 想测试某项功能时，
    // 取消对应代码的 /* */ 即可。
    //
    // 正式接入服务器后，
    // 这些函数将由服务器收到客户端请求后调用。
    // =================================================


    // -------------------------------------------------
    // 测试 1：添加真实文件
    //
    // 修改下面路径为电脑上的真实文件
    // -------------------------------------------------

    /*
    const bool isFileAdded = fileManager.addFile(
        "D:/sizheng.pdf",
        currentUser
    );

    if (isFileAdded)
    {
        qDebug() << "测试新增成功！";
    }
    else
    {
        qDebug() << "测试新增失败！";
    }
    */

     // -------------------------------------------------
     // 测试 2：查看当前用户文件列表
     // 当前默认开启
     // -------------------------------------------------

    fileManager.listFiles(currentUser);

    // -------------------------------------------------
// 一次性测试：为ID 5添加v2
// 只运行一次，成功后必须关闭
// -------------------------------------------------
/*
    const bool isVersionAdded = fileManager.addFileVersion(
        1,
        "D:/sizheng.pdf",
        currentUser
    );

    if (isVersionAdded)
    {
        qDebug() << "Version test succeeded!";
    }
    else
    {
        qDebug() << "Version test failed!";
    }

    fileManager.listFileVersions(
        1,
        currentUser
    );
*/
// -------------------------------------------------
// 一次性测试：将v1恢复为新的最新版本
// 成功后必须关闭
// -------------------------------------------------
    /*
const bool isRestoreSuccessful = fileManager.restoreFileVersion(
    1,
    1,
    currentUser
);

if (isRestoreSuccessful)
{
    qDebug() << "Restore test succeeded!";
}
else
{
    qDebug() << "Restore test failed!";
}

fileManager.listFileVersions(
    1,
    currentUser
);
*/

    // -------------------------------------------------
    // 测试 3：删除文件
    // 一次性测试已经完成，当前关闭
    // -------------------------------------------------

    /*
    const bool isFileDeleted = fileManager.deleteFile(
        4,
        currentUser
    );

    if (isFileDeleted)
    {
        qDebug() << "Delete test succeeded!";
    }
    else
    {
        qDebug() << "Delete test failed!";
    }
    */
    // -------------------------------------------------
    // 测试 4：重命名文件
    //
    // 将 1 改成数据库中真实存在的文件 ID
    // -------------------------------------------------

    /*
    fileManager.renameFile(
        1,
        currentUser,
        "new_name.pdf"
    );
    */

    // -------------------------------------------------
// 测试 5：获取文件真实路径
//
// 将 1 改成数据库中真实存在的文件 ID
// -------------------------------------------------
/*
const QString filePath = fileManager.getFilePath(
    4,
    currentUser
);

if (!filePath.isEmpty())
{
    qDebug() << "File ready for download:"
             << filePath;
}
*/

// 前面的添加、版本添加、恢复测试都已注释


// -------------------------------------------------
// 安全测试：读取v1路径，并验证最新版本不能删除
// -------------------------------------------------
/*
const QString versionOnePath = fileManager.getFileVersionPath(
    1,
    1,
    currentUser
);

if (!versionOnePath.isEmpty())
{
    qDebug() << "Version 1 ready for download:"
        << versionOnePath;
}

const bool isLatestDeleteAllowed = fileManager.deleteFileVersion(
    1,
    3,
    currentUser
);

if (!isLatestDeleteAllowed)
{
    qDebug()
        << "Safety test passed: latest version was protected.";
}

fileManager.listFileVersions(
    1,
    currentUser
);
*/

// 下面开始服务器代码

    QTcpServer tcpServer;

    // -------------------------------------------------
// 一次性测试：SHA-256秒传
// 成功运行一次后必须注释
// -------------------------------------------------
    /*
    const QString testSha256 =
        "58055843f8f14fb4decc09feb0e6ed43e8d81c7fcafa6d86fb209e672b2592fb";

    const bool isInstantUploadSuccessful = fileManager.instantUploadFile(
        "sizheng_instant_copy.pdf",
        testSha256,
        currentUser
    );

    if (isInstantUploadSuccessful)
    {
        qDebug() << "Instant upload test succeeded!";
    }
    else
    {
        qDebug() << "Instant upload test failed!";
    }

    fileManager.listFiles(currentUser);
    */

    if (!tcpServer.listen(QHostAddress::AnyIPv4, 8888))
    {
        qDebug() << "Server startup failed:"
            << tcpServer.errorString();

        return -1;
    }

    qDebug() << "Server started successfully!";
    qDebug() << "Listening port:"
        << tcpServer.serverPort();

    const int exitCode = app.exec();

    database.close();
    return exitCode;
    }
