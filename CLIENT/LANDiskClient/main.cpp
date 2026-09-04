#include <QApplication>
#include <QDebug>
#include "Utils/FileUtil.h"
#include "Utils/HashUtil.h"
#include "Utils/Config.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    qDebug() << "========================================";
    qDebug() << "🚀 开始测试 FileUtil 和 HashUtil 工具类";
    qDebug() << "========================================";

    // ============================================
    // 1. 测试 FileUtil
    // ============================================
    // 注意：把下面路径换成你电脑上真实存在的任意一个小文件
    QString testFile = "C:/Users/TK/source/repos/LANDiskClient/main.cpp";

    qDebug() << "";
    qDebug() << "📁 测试文件路径:" << testFile;

    // 获取文件大小
    qint64 fileSize = FileUtil::getFileSize(testFile);
    if (fileSize < 0) {
        qDebug() << "❌ 文件不存在或无法打开，请检查路径！";
        qDebug() << "========================================";
        return 0;
    }
    qDebug() << "📊 文件大小:" << fileSize << "字节";

    // 计算分片数（按 4MB）
    int totalChunks = FileUtil::getTotalChunks(testFile, Config::MAX_CHUNK_SIZE);
    qDebug() << "📦 总分片数(4MB):" << totalChunks;

    // 读取第一个分片（前 4MB）
    if (totalChunks > 0) {
        QByteArray chunk0 = FileUtil::readChunk(testFile, 0, Config::MAX_CHUNK_SIZE);
        qDebug() << "📄 第 1 个分片大小:" << chunk0.size() << "字节";
    }

    // ============================================
    // 2. 测试 HashUtil (SHA-256)
    // ============================================
    qDebug() << "";
    qDebug() << "🔐 开始计算 SHA-256 哈希值...";
    QString hash = HashUtil::sha256(testFile);
    if (!hash.isEmpty()) {
        qDebug() << "✅ SHA-256:" << hash;
    }
    else {
        qDebug() << "❌ 哈希计算失败";
    }

    qDebug() << "";
    qDebug() << "========================================";
    qDebug() << "✅ 测试完成！";
    qDebug() << "========================================";

    return 0;
}
