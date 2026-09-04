#ifndef HASHUTIL_H
#define HASHUTIL_H

#include <QString>

class HashUtil {
public:
    static QString sha256(const QString &filePath);
};

#endif // HASHUTIL_H