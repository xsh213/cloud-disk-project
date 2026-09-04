#ifndef FILEUTIL_H
#define FILEUTIL_H

#include <QString>
#include <QByteArray>

class FileUtil {
public:
    // 获取文件大小（字节）
    static qint64 getFileSize(const QString &filePath);

    // 读取文件的指定分片（从 offset 开始读 size 字节）
    static QByteArray readChunk(const QString &filePath, qint64 offset, int size);

    // 计算文件总分片数（每片 MAX_CHUNK_SIZE 字节）
    static int getTotalChunks(const QString &filePath, int chunkSize);
};

#endif // FILEUTIL_H