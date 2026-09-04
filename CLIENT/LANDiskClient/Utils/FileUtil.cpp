#include "FileUtil.h"
#include <QFile>
#include <QDebug>

qint64 FileUtil::getFileSize(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << filePath;
        return -1;
    }
    qint64 size = file.size();
    file.close();
    return size;
}

QByteArray FileUtil::readChunk(const QString &filePath, qint64 offset, int size) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << filePath;
        return QByteArray();
    }
    if (!file.seek(offset)) {
        qDebug() << "无法定位到偏移量:" << offset;
        file.close();
        return QByteArray();
    }
    QByteArray data = file.read(size);
    file.close();
    return data;
}

int FileUtil::getTotalChunks(const QString &filePath, int chunkSize) {
    qint64 size = getFileSize(filePath);
    if (size <= 0) return 0;
    return (size + chunkSize - 1) / chunkSize;  // 向上取整
}