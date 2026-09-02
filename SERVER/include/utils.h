#pragma once

#include <cstdint>
#include <string>

// ============================================================
// Utils.h — 服务端通用工具函数
// 定位：无状态、无业务逻辑、不依赖数据库、可独立测试
// 说明：供 UserManager / StorageEngine / FileManager 调用
// ============================================================

namespace lancloud {
	namespace utils {

		// ---------- 哈希与安全 ----------

		// 对内存中的数据进行 SHA-256，返回小写十六进制串（64 字符）
		// 用途：密码哈希（配合 salt）、秒传文件哈希
		std::string sha256(const std::string& data);

		// 对大文件流式计算 SHA-256（分块读取，不整文件进内存）
		// 失败（文件打不开）返回空串
		// 用途：客户端/服务端秒传校验，必须与 sha256(data) 输出格式一致
		std::string sha256File(const std::string& filepath);

		// 生成 len 个随机十六进制字符
		// 用途：token、密码 salt
		std::string randomHex(std::size_t len);

		// ---------- 时间 ----------

		// 当前 Unix 时间戳（秒），用于 sync 增量、created_at / updated_at
		std::int64_t nowUnix();

		// 当前时间格式化串："2026-09-02 10:00:00"，用于列表展示
		std::string nowStr();

		// ---------- 路径与文件系统 ----------

		// 拼接路径：joinPath("a", "b") -> "a/b"；自动处理分隔符
		std::string joinPath(const std::string& base, const std::string& name);

		// 校验文件名是否安全合法（防路径穿越 + Windows 非法字符/保留名）
		bool safeFileName(const std::string& name);

		// 递归创建目录（等价 mkdir -p），成功返回 true
		bool mkdirs(const std::string& path);

		// 文件大小（字节），文件不存在或出错返回 0
		std::uint64_t fileSize(const std::string& path);

		// 读整个文件（小文件/元数据用，大文件请用 sha256File 等流式方案）
		bool readFile(const std::string& path, std::string& out);

		// 写入文件（覆盖），成功返回 true
		bool writeFile(const std::string& path, const std::string& data);

		// 追加写入文件，成功返回 true
		bool appendFile(const std::string& path, const std::string& data);

		// 递归删除文件或目录，成功返回 true
		bool removeAll(const std::string& path);

		// ---------- 字符串 ----------

		// 去除首尾空白（空格 / 制表 / 换行）
		std::string trim(const std::string& s);

		// 转小写
		std::string toLower(const std::string& s);

	} // namespace utils
} // namespace lancloud
