#include "HashUtil.h"
#include <QCryptographicHash>
#include <QFile>
#include <QDebug>

QString HashUtil::sha256(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << filePath;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    const int BUFFER_SIZE = 1024 * 1024; // 1MB 缓冲区，避免大文件占满内存
    char buffer[BUFFER_SIZE];

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, BUFFER_SIZE);
        if (bytesRead > 0) {
            hash.addData(buffer, static_cast<int>(bytesRead));
        }
    }
    file.close();
    return hash.result().toHex();
}