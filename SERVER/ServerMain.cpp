#include <QCoreApplication>
#include "StorageManager.h"

#include <QSqlDatabase>
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

using namespace netdisk::server;

// =====================================================
// 主函数
// =====================================================
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);


    QSqlDatabase db;


    // -------------------------------------------------
    // 初始化 SQLite 数据库
    // -------------------------------------------------
    if (!initDatabase(db))
    {
        return -1;
    }


    // -------------------------------------------------
    // 初始化服务器文件存储目录
    // -------------------------------------------------
    if (!initStorageDirectory())
    {
        return -1;
    }


    // 当前测试用户
    QString currentUser =
        "user01";

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
    bool addSuccess = addFile(
        db,
        "D:/sizheng.pdf",
        currentUser
    );

    if (addSuccess)
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

    listFiles(
        db,
        currentUser
    );

    // -------------------------------------------------
// 一次性测试：为ID 5添加v2
// 只运行一次，成功后必须关闭
// -------------------------------------------------
/*
    bool versionSuccess = addFileVersion(
        db,
        1,
        "D:/sizheng.pdf",
        currentUser
    );

    if (versionSuccess)
    {
        qDebug() << "Version test succeeded!";
    }
    else
    {
        qDebug() << "Version test failed!";
    }

    listFileVersions(
        db,
        1,
        currentUser
    );
*/
// -------------------------------------------------
// 一次性测试：将v1恢复为新的最新版本
// 成功后必须关闭
// -------------------------------------------------
    /*
bool isRestoreSuccessful = restoreFileVersion(
    db,
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

listFileVersions(
    db,
    1,
    currentUser
);
*/

    // -------------------------------------------------
    // 测试 3：删除文件
    // 一次性测试已经完成，当前关闭
    // -------------------------------------------------

    /*
    bool deleteSuccess = deleteFile(
        db,
        4,
        currentUser
    );

    if (deleteSuccess)
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
    renameFile(
        db,
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
QString filePath = getFilePath(
    db,
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

// 安全测试代码放这里
QString versionPath = getFileVersionPath(
    db,
    1,
    1,
    currentUser
);

// -------------------------------------------------
// 安全测试：读取v1路径，并验证最新版本不能删除
// -------------------------------------------------
/*
QString versionOnePath = getFileVersionPath(
    db,
    1,
    1,
    currentUser
);

if (!versionOnePath.isEmpty())
{
    qDebug() << "Version 1 ready for download:"
        << versionOnePath;
}

bool isLatestDeleteAllowed = deleteFileVersion(
    db,
    1,
    3,
    currentUser
);

if (!isLatestDeleteAllowed)
{
    qDebug()
        << "Safety test passed: latest version was protected.";
}

listFileVersions(
    db,
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
    QString testSha256 =
        "58055843f8f14fb4decc09feb0e6ed43e8d81c7fcafa6d86fb209e672b2592fb";

    bool isInstantUploadSuccessful = instantUploadFile(
        db,
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

    listFiles(
        db,
        currentUser
    );*/

    if (!tcpServer.listen(QHostAddress::AnyIPv4, 8888))
    {
        qDebug() << "Server startup failed:"
            << tcpServer.errorString();

        return -1;
    }

    qDebug() << "Server started successfully!";
    qDebug() << "Listening port:"
        << tcpServer.serverPort();

    int exitCode = app.exec();

    db.close();
    return exitCode;
    }
