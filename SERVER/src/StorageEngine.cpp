#include "StorageEngine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

namespace netdisk::server {

namespace {

constexpr qint64 HASH_BUFFER_SIZE = 1024 * 1024;

} // namespace

StorageEngine::StorageEngine(const QString& storageRoot)
    : m_storageRoot(storageRoot)
{
}

bool StorageEngine::initialize() const
{
    QDir storageDirectory;

    return storageDirectory.exists(m_storageRoot) ||
           storageDirectory.mkpath(m_storageRoot);
}

QString StorageEngine::calculateFileSha256(
    const QString& filePath) const
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);

    while (!file.atEnd())
    {
        const QByteArray fileChunk = file.read(HASH_BUFFER_SIZE);

        if (fileChunk.isEmpty() && file.error() != QFile::NoError)
        {
            return {};
        }

        hash.addData(fileChunk);
    }

    return QString::fromLatin1(hash.result().toHex());
}

bool StorageEngine::isValidSha256(const QString& sha256) const
{
    const QString normalizedSha256 = sha256.trimmed().toLower();

    if (normalizedSha256.length() != 64)
    {
        return false;
    }

    for (const QChar character : normalizedSha256)
    {
        const bool isDecimalDigit =
            character >= QLatin1Char('0') &&
            character <= QLatin1Char('9');
        const bool isHexadecimalLetter =
            character >= QLatin1Char('a') &&
            character <= QLatin1Char('f');

        if (!isDecimalDigit && !isHexadecimalLetter)
        {
            return false;
        }
    }

    return true;
}

QString StorageEngine::storageRoot() const
{
    return m_storageRoot;
}

} // namespace netdisk::server
