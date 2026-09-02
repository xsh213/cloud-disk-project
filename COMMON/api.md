# LANCloudDisk API 文档

> 契约文档，两端（server / client）共用，修改需同步。

## 1. 通用约定

### 1.1 Base URL

```
http://<server_ip>:<port>/api
```

- `server_ip`：服务端所在主机的局域网 IP（`ipconfig` 查看 IPv4 地址）
- `port`：**默认 8080**，服务端与客户端均可通过配置修改，两端必须一致
- 示例：`http://192.168.1.100:8080/api/user/login`

### 1.2 数据格式

- 所有请求/响应均为 **UTF-8 JSON**，`Content-Type: application/json`
- 分片上传 body 与下载响应为 **原始二进制流**（`application/octet-stream`）

### 1.3 认证方式

- 登录成功后服务端返回 `token`
- 客户端后续所有 `/api/file/*` 请求必须在请求头携带：

```
Authorization: Bearer <token>
```

- `token` 无效或过期 → HTTP 401，业务码 `1002`

### 1.4 统一响应结构

所有接口（除下载外）返回统一 JSON：

```json
{
  "code": 0,
  "message": "ok",
  "data": { }
}
```

- HTTP 状态码表达大类（401/403/404/413…）
- `data.code` 表达精确业务码（见第 6 节错误码表）

### 1.5 分片大小

- 默认 **4MB**（`4194304` 字节），由服务端在 `upload/init` 时下发，两端不写死

### 1.6 文件定位

- 文件/目录一律用 `file_id`（数据库主键）定位
- `parent_id` 构成多级目录树，根目录 `parent_id = 0`
- `path` 仅作展示用途

---

## 2. 用户管理

### 2.1 注册

```
POST /api/user/register
```

请求：

```json
{
  "username": "alice",
  "password": "123456"
}
```

响应（成功，HTTP 200）：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "user_id": 1,
    "username": "alice"
  }
}
```

错误：用户名已存在 → `2001`；参数非法 → `1001`

> 密码存储：`salt + SHA-256` 哈希，禁止明文入库。

### 2.2 登录

```
POST /api/user/login
```

请求：

```json
{
  "username": "alice",
  "password": "123456"
}
```

响应（成功，HTTP 200）：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "token": "<token>",
    "user_id": 1,
    "username": "alice",
    "total_quota": 10737418240,
    "used_quota": 1048576
  }
}
```

- `total_quota` / `used_quota` 单位：字节；客户端首页空间用量直接取此二值
- 错误：用户不存在 → `2002`；密码错误 → `2003`

### 2.3 登出

```
POST /api/user/logout
```

请求头携带 token。使当前 token 失效。响应 `code = 0` 即可。

### 2.4 用户信息

```
GET /api/user/info
```

请求头携带 token。响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "user_id": 1,
    "username": "alice",
    "total_quota": 10737418240,
    "used_quota": 1048576,
    "file_count": 12
  }
}
```

---

## 3. 文件管理（目录树）

### 3.1 列目录

```
GET /api/file/list?parent_id=0
```

请求头携带 token。响应（成功，HTTP 200）：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "items": [
      {
        "file_id": 10,
        "name": "doc",
        "parent_id": 0,
        "is_dir": true,
        "size": 0,
        "created_at": "2026-09-02 10:00:00",
        "updated_at": "2026-09-02 10:00:00"
      },
      {
        "file_id": 11,
        "name": "a.txt",
        "parent_id": 0,
        "is_dir": false,
        "size": 2048,
        "created_at": "2026-09-02 10:05:00",
        "updated_at": "2026-09-02 10:05:00"
      }
    ]
  }
}
```

### 3.2 创建文件夹

```
POST /api/file/mkdir
```

请求：

```json
{
  "parent_id": 10,
  "name": "report"
}
```

响应 `data`：

```json
{ "file_id": 15, "name": "report", "parent_id": 10, "is_dir": true }
```

### 3.3 重命名

```
POST /api/file/rename
```

请求：

```json
{
  "file_id": 11,
  "new_name": "b.txt"
}
```

响应 `data`：`{ "file_id": 11, "name": "b.txt" }`

错误：文件不存在 → `3001`；同目录同名冲突 → `3002`

### 3.4 移动

```
POST /api/file/move
```

请求：

```json
{
  "file_id": 11,
  "new_parent_id": 10
}
```

响应 `data`：`{ "file_id": 11, "parent_id": 10 }`

### 3.5 删除

```
POST /api/file/delete
```

请求：

```json
{ "file_id": 10 }
```

- 目录删除时**递归删除整棵子树**，服务端同时释放磁盘与配额
- 响应 `data`：`{ "file_id": 10, "released_size": 4096 }`

### 3.6 目录同步（增量）

```
GET /api/file/sync?parent_id=0&since=<unix时间戳>
```

- 返回 `since` 之后该用户的所有变更条目（新增 / 改名 / 移动 / 删除），供客户端"目录同步"功能增量拉取
- 客户端首次同步 `since=0` 全量拉取；后续携带上次的时间戳

响应 `data`：

```json
{
  "items": [
    {
      "file_id": 11,
      "name": "b.txt",
      "parent_id": 10,
      "is_dir": false,
      "size": 2048,
      "updated_at": 1690000000,
      "deleted": false
    }
  ]
}
```

> 第一版 MVP 可暂缓实现，接口位已预留；客户端先用 `list` 轮询全树。

---

## 4. 上传（分片 + 断点续传 + 秒传）

> 三段式流程：`init` → 逐个 `chunk` → `complete`。

### 4.1 初始化上传

```
POST /api/file/upload/init
```

请求：

```json
{
  "parent_id": 10,
  "file_name": "big.iso",
  "file_size": 5368709120,
  "file_hash": "<sha256>"
}
```

- `file_hash`：客户端对整个文件计算的 SHA-256
- 响应分两种情况：

**① 秒传命中**（同用户 + 同哈希 + 同大小，服务端已有该文件）：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "completed": true,
    "file_id": 88,
    "dedup": true
  }
}
```

客户端收到 `completed: true` 即可直接视为上传成功，无需再传分片。

**② 需要分片上传**（同时返回断点续传所需信息）：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "completed": false,
    "upload_id": "u_xxx",
    "chunk_size": 4194304,
    "chunk_total": 1280,
    "exists_chunks": [0, 1, 2, 5]
  }
}
```

- `upload_id`：本次上传会话唯一标识
- `exists_chunks`：已上传过的分片索引数组，客户端只需补传缺失分片（断点续传核心）

错误：配额不足 → `3004`（HTTP 413）

### 4.2 上传分片

```
PUT /api/file/upload/chunk?upload_id=u_xxx&chunk_index=3
```

- body：原始二进制分片数据，`Content-Type: application/octet-stream`
- 每个分片大小 = `chunk_size`（最后一片可不足）
- **幂等**：重复上传同一 `chunk_index` 直接返回成功，不报错

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "upload_id": "u_xxx",
    "chunk_index": 3,
    "received": 4194304
  }
}
```

错误：`upload_id` 无效 → `4001`

### 4.3 完成上传

```
POST /api/file/upload/complete
```

请求：

```json
{ "upload_id": "u_xxx" }
```

服务端动作：

1. 校验所有分片齐全
2. 合并分片为完整文件
3. 重新计算整体 SHA-256，与 `init` 时 `file_hash` 比对
4. 一致 → 写入文件元数据、扣除配额

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "file_id": 88,
    "size": 5368709120
  }
}
```

错误：分片缺失 → `4002`；哈希校验失败 → `4003`

### 4.4 查询上传进度

```
GET /api/file/upload/info?upload_id=u_xxx
```

- 客户端恢复会话 / 断点续传前调用，获取已上传分片

响应：

```json
{
  "code": 0,
  "message": "ok",
  "data": {
    "upload_id": "u_xxx",
    "chunk_size": 4194304,
    "chunk_total": 1280,
    "exists_chunks": [0, 1, 2, 5]
  }
}
```

---

## 5. 下载（支持断点续传）

### 5.1 下载文件

```
GET /api/file/download?file_id=88
```

请求头携带 token，可选 `Range` 头：

```
Range: bytes=0-4194303
```

响应头：

```
Content-Type: application/octet-stream
Content-Length: 4194304
Content-Disposition: attachment; filename="big.iso"
Accept-Ranges: bytes
```

- 不带 `Range`：返回 `200 OK` + 完整二进制流
- 带 `Range`：返回 `206 Partial Content` + `Content-Range: bytes 0-4194303/5368709120` + 对应字节段
- **`Range` 支持是客户端下载断点续传、暂停/恢复的关键**，服务端必须实现

错误：文件不存在 → `3001`；无权限 → `3003`

---

## 6. 错误码表

| code | 含义 | 建议 HTTP 状态码 |
|---|---|---|
| 0 | 成功 | 200 |
| 1001 | 参数缺失 / 非法 | 400 |
| 1002 | token 无效或过期 | 401 |
| 2001 | 用户名已存在 | 409 |
| 2002 | 用户不存在 | 404 |
| 2003 | 密码错误 | 401 |
| 3001 | 文件 / 目录不存在 | 404 |
| 3002 | 同名文件已存在 | 409 |
| 3003 | 无权限 | 403 |
| 3004 | 磁盘配额不足 | 413 |
| 4001 | upload_id 无效 | 404 |
| 4002 | 分片缺失 | 409 |
| 4003 | 哈希校验失败 | 422 |
| 5000 | 服务器内部错误 | 500 |
