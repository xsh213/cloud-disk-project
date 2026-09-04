#include "UserManager.h"

// ============================================================
// UserManager.cpp — 注册 / 登录 / 登出 / 鉴权 / 用户信息
// 说明：密码 sha256 直接哈希
// ============================================================

namespace lancloud {

    namespace {
        constexpr std::int64_t kDefaultQuota = 10LL * 1024 * 1024 * 1024; // 10GB
    }

    UserManager::UserManager(DBManager& db) : db_(db) {}

    UserResult UserManager::registerUser(const std::string& username, const std::string& password) {
        const std::string uname = utils::trim(username);

        // 参数校验（api 层约定：用户名 1~32，密码 6~64）
        if (uname.empty() || uname.size() > 32) {
            return { false, static_cast<int>(Status::kInvalidParams), "username invalid", {} };
        }
        if (password.empty() || password.size() < 6 || password.size() > 64) {
            return { false, static_cast<int>(Status::kInvalidParams), "password invalid", {} };
        }

        std::int64_t user_id = 0;
        if (!db_.createUser(uname, hashPassword(password), kDefaultQuota, user_id)) {
            return { false, static_cast<int>(Status::kUserExists), "username already exists", {} };
        }

        UserInfo info;
        info.user_id = user_id;
        info.username = uname;
        info.total_quota = kDefaultQuota;
        info.used_quota = 0;
        return { true, static_cast<int>(Status::kOk), "ok", info };
    }

    UserResult UserManager::login(const std::string& username, const std::string& password) {
        const std::string uname = utils::trim(username);

        UserRecord rec;
        if (!db_.getUserByName(uname, rec)) {
            return { false, static_cast<int>(Status::kUserNotExist), "user not found", {} };
        }

        if (rec.password_hash != hashPassword(password)) {
            return { false, static_cast<int>(Status::kWrongPassword), "wrong password", {} };
        }

        // 签发新 token 并入库（服务端重启后登录态仍有效）
        const std::string token = utils::randomHex(32);
        db_.setToken(rec.user_id, token);

        UserInfo info;
        info.user_id = rec.user_id;
        info.username = rec.username;
        info.token = token;
        info.total_quota = rec.total_quota;
        info.used_quota = rec.used_quota;
        return { true, static_cast<int>(Status::kOk), "ok", info };
    }

    bool UserManager::logout(const std::string& token) {
        std::int64_t user_id = 0;
        if (!db_.getUserIdByToken(token, user_id)) return false;
        return db_.clearToken(user_id);
    }

    std::int64_t UserManager::verifyToken(const std::string& token) const {
        if (token.empty()) return -1;
        std::int64_t user_id = 0;
        return db_.getUserIdByToken(token, user_id) ? user_id : -1;
    }

    UserResult UserManager::getUserInfo(std::int64_t user_id) const {
        UserRecord rec;
        if (!db_.getUserById(user_id, rec)) {
            return { false, static_cast<int>(Status::kUserNotExist), "user not found", {} };
        }
        UserInfo info;
        info.user_id = rec.user_id;
        info.username = rec.username;
        info.total_quota = rec.total_quota;
        info.used_quota = rec.used_quota;
        return { true, static_cast<int>(Status::kOk), "ok", info };
    }

} // namespace lancloud
