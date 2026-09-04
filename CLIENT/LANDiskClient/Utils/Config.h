#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

namespace Config {
    // 服务端地址
    const QString BASE_URL = "http://10.55.202.204:8080/api";

    // 分片大小：4MB
    const int MAX_CHUNK_SIZE = 4 * 1024 * 1024;

    // 接口路径
    const QString LOGIN_PATH = "/auth/login";
    const QString REGISTER_PATH = "/auth/register";
}

#endif // CONFIG_H