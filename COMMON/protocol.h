#pragm#pragma once

#include <cstddef>
#include <string>

namespace lancloud {

    // ---- 路由常量（服务端注册 + 客户端拼接 URL 共用） ----
    // Base URL: http://<server_ip>:<port>/api/...
    // 端口默认 8080，两端均可通过配置修改（见 config::kDefaultPort）
    namespace routes {
        constexpr const char* kRegister = "/api/user/register";
        constexpr const char* kLogin = "/api/user/login";
        constexpr const char* kLogout = "/api/user/logout";
        constexpr const char* kUserInfo = "/api/user/info";

        constexpr const char* kListDir = "/api/file/list";
        constexpr const char* kMkdir = "/api/file/mkdir";
        constexpr const char* kRename = "/api/file/rename";
        constexpr const char* kMove = "/api/file/move";
        constexpr const char* kDelete = "/api/file/delete";
        constexpr const char* kSync = "/api/file/sync";

        constexpr const char* kUploadInit = "/api/file/upload/init";
        constexpr const char* kUploadChunk = "/api/file/upload/chunk";
        constexpr const char* kUploadDone = "/api/file/upload/complete";
        constexpr const char* kUploadInfo = "/api/file/upload/info";

        constexpr const char* kDownload = "/api/file/download";
    }

    // ---- 业务状态码（响应 body 的 code 字段） ----
    enum class Status {
        kOk = 0,
        kInvalidParams = 1001,
        kUnauthorized = 1002,
        kUserExists = 2001,
        kUserNotExist = 2002,
        kWrongPassword = 2003,
        kFileNotExist = 3001,
        kFileExists = 3002,
        kPermissionDenied = 3003,
        kQuotaExceeded = 3004,
        kUploadInvalid = 4001,
        kChunkMissing = 4002,
        kHashMismatch = 4003,
        kInternalError = 5000,
    };

    // ---- JSON 键名常量（两端序列化/反序列化共用，杜绝魔法字符串） ----
    namespace keys {
        // 通用
        constexpr const char* kCode = "code";
        constexpr const char* kMessage = "message";
        constexpr const char* kData = "data";

        // 用户
        constexpr const char* kUsername = "username";
        constexpr const char* kPassword = "password";
        constexpr const char* kToken = "token";
        constexpr const char* kUserId = "user_id";
        constexpr const char* kTotalQuota = "total_quota";
        constexpr const char* kUsedQuota = "used_quota";

        // 文件
        constexpr const char* kFileId = "file_id";
        constexpr const char* kParentId = "parent_id";
        constexpr const char* kFileName = "name";
        constexpr const char* kIsDir = "is_dir";
        constexpr const char* kSize = "size";
        constexpr const char* kItems = "items";

        // 上传/分片
        constexpr const char* kFileSize = "file_size";
        constexpr const char* kFileHash = "file_hash";
        constexpr const char* kUploadId = "upload_id";
        constexpr const char* kChunkSize = "chunk_size";
        constexpr const char* kChunkIndex = "chunk_index";
        constexpr const char* kChunkTotal = "chunk_total";
        constexpr const char* kExistsChunks = "exists_chunks";
        constexpr const char* kCompleted = "completed";
        constexpr const char* kDedup = "dedup";
    }

    // ---- HTTP 头常量 ----
    namespace headers {
        constexpr const char* kAuthorization = "Authorization";
        constexpr const char* kBearer = "Bearer ";
        constexpr const char* kRange = "Range";
        constexpr const char* kContentRange = "Content-Range";
        constexpr const char* kAcceptRanges = "Accept-Ranges";
        constexpr const char* kBytes = "bytes";
    }

    // ---- 配置常量 ----
    namespace config {
        constexpr const int   kDefaultPort = 8080;               // 默认监听端口，两端可配置
        constexpr const char* kDefaultQuota = "10737418240";      // 10GB
        constexpr std::size_t kDefaultChunkSize = 4 * 1024 * 1024;  // 4MB
    }

} // namespace lancloud
