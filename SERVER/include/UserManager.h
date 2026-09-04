#pragma once

#include <cstdint>
#include <string>

#include "DBManager.h"     // 依赖：用户表/会话存储（接口见下方）
#include "Utils.h"
#include "http_protocol.h" // 依赖：Status 枚举（复用业务码）

namespace lancloud {

    // 用户信息（登录成功 / 查询后返回，与 api.md 的 JSON 字段一一对应）
    struct UserInfo {
        std::int64_t user_id = 0;
        std::string  username;
        std::string  token;           // 登录后非空
        std::int64_t total_quota = 0; // 字节
        std::int64_t used_quota = 0; // 字节
    };

    // 统一操作结果：Server 层拿到后直接组装 JSON 响应
    struct UserResult {
        bool        ok = false;
        int         code = 0;         // 对应 Status 枚举值（2001/2002/2003/1002…）
        std::string message;
        UserInfo    info;
    };

    class UserManager {
    public:
        explicit UserManager(DBManager& db);

        // 注册：用户名已存在返回 kUserExists；成功默认配额 10GB
        UserResult registerUser(const std::string& username, const std::string& password);

        // 登录：校验密码，签发新 token 并入库；返回完整 UserInfo
        UserResult login(const std::string& username, const std::string& password);

        // 登出：使 token 失效（库中置空）
        bool logout(const std::string& token);

        // 鉴权：token 有效返回 user_id，无效返回 -1
        // （供 FileManager 所有 /api/file/* 接口的中间件调用）
        std::int64_t verifyToken(const std::string& token) const;

        // 查询用户信息（含配额，供 /user/info 与首页展示）
        UserResult getUserInfo(std::int64_t user_id) const;

    private:
        // 密码哈希：sha256 直接哈希，不加盐（项目约定，见安全提示）
        static std::string hashPassword(const std::string& password) {
            return utils::sha256(password);
        }

        DBManager& db_;   // 引用注入，生命周期由 Server 管理
    };

} // namespace lancloud
