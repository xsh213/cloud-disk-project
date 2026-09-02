#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <vector>

namespace lancloud {
    namespace utils {

        // ============================================================
        // 内部：SHA-256 实现（自包含，无第三方依赖）
        // ============================================================
        namespace {

            constexpr std::uint32_t kSha256K[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

            inline std::uint32_t rotr(std::uint32_t x, std::uint32_t n) {
                return (x >> n) | (x << (32 - n));
            }

            // 支持增量 update 的 SHA-256 上下文
            class Sha256 {
            public:
                Sha256() { init(); }

                void init() {
                    h_[0] = 0x6a09e667; h_[1] = 0xbb67ae85; h_[2] = 0x3c6ef372; h_[3] = 0xa54ff53a;
                    h_[4] = 0x510e527f; h_[5] = 0x9b05688c; h_[6] = 0x1f83d9ab; h_[7] = 0x5be0cd19;
                    bitLen_ = 0;
                    bufLen_ = 0;
                }

                void update(const void* data, std::size_t len) {
                    const auto* p = static_cast<const std::uint8_t*>(data);
                    bitLen_ += static_cast<std::uint64_t>(len) * 8;
                    while (len > 0) {
                        const std::size_t take = std::min(len, sizeof(buf_) - bufLen_);
                        std::memcpy(buf_ + bufLen_, p, take);
                        bufLen_ += take;
                        p += take;
                        len -= take;
                        if (bufLen_ == sizeof(buf_)) {
                            transform(buf_);
                            bufLen_ = 0;
                        }
                    }
                }

                std::string finalHex() {
                    const std::uint64_t totalBits = bitLen_;
                    const std::uint8_t  padByte = 0x80;
                    update(&padByte, 1);
                    std::uint8_t zeros[64] = { 0 };
                    const std::size_t need = (56 - bufLen_ % 64) % 64;
                    update(zeros, need);
                    std::uint8_t lenBytes[8];
                    for (int i = 0; i < 8; ++i) {
                        lenBytes[i] = static_cast<std::uint8_t>((totalBits >> (56 - i * 8)) & 0xff);
                    }
                    update(lenBytes, 8);

                    static const char* hex = "0123456789abcdef";
                    std::string out;
                    out.reserve(64);
                    for (std::uint32_t v : h_) {
                        for (int i = 0; i < 4; ++i) {
                            const std::uint8_t b = static_cast<std::uint8_t>((v >> (24 - i * 8)) & 0xff);
                            out += hex[b >> 4];
                            out += hex[b & 0x0f];
                        }
                    }
                    return out;
                }

            private:
                void transform(const std::uint8_t block[64]) {
                    std::uint32_t w[64];
                    for (int i = 0; i < 16; ++i) {
                        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24)
                            | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16)
                            | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8)
                            | static_cast<std::uint32_t>(block[i * 4 + 3]);
                    }
                    for (int i = 16; i < 64; ++i) {
                        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
                    }

                    std::uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3];
                    std::uint32_t e = h_[4], f = h_[5], g = h_[6], hh = h_[7];

                    for (int i = 0; i < 64; ++i) {
                        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                        const std::uint32_t ch = (e & f) ^ (~e & g);
                        const std::uint32_t t1 = hh + s1 + ch + kSha256K[i] + w[i];
                        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                        const std::uint32_t t2 = s0 + maj;
                        hh = g; g = f; f = e; e = d + t1;
                        d = c; c = b; b = a; a = t1 + t2;
                    }

                    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d;
                    h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += hh;
                }

                std::uint32_t h_[8];
                std::uint64_t bitLen_ = 0;
                std::uint8_t  buf_[64];
                std::size_t   bufLen_ = 0;
            };

        } // namespace

        // ============================================================
        // 公开实现
        // ============================================================

        std::string sha256(const std::string& data) {
            Sha256 ctx;
            ctx.update(data.data(), data.size());
            return ctx.finalHex();
        }

        std::string sha256File(const std::string& filepath) {
            std::ifstream f(filepath, std::ios::binary);
            if (!f) return "";
            Sha256 ctx;
            // 1MB 分块缓冲放堆上（栈上放 1MB 数组会栈溢出，默认栈仅 1MB）
            std::vector<char> buf(1024 * 1024);
            while (f) {
                f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
                const std::streamsize got = f.gcount();
                if (got > 0) ctx.update(buf.data(), static_cast<std::size_t>(got));
            }
            return ctx.finalHex();
        }

        std::string randomHex(std::size_t len) {
            static std::random_device rd;
            static const char* hex = "0123456789abcdef";
            std::string out;
            out.reserve(len);
            for (std::size_t i = 0; i < len; ++i) {
                out += hex[rd() % 16];
            }
            return out;
        }

        std::int64_t nowUnix() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        std::string nowStr() {
            const auto now = std::chrono::system_clock::now();
            const std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &t);
#else
            localtime_r(&t, &tm);
#endif
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            return buf;
        }

        std::string joinPath(const std::string& base, const std::string& name) {
            if (base.empty()) return name;
            const char last = base.back();
            if (last == '/' || last == '\\') return base + name;
            return base + "/" + name;
        }

        bool safeFileName(const std::string& name) {
            if (name.empty() || name == "." || name == "..") return false;
            if (name.size() > 255) return false;
            static const std::string illegal = "/\\:*?\"<>|";
            for (unsigned char c : name) {
                if (c < 32 || illegal.find(static_cast<char>(c)) != std::string::npos) return false;
            }
            // Windows 保留名（不区分大小写，不含扩展名部分）
            const std::string stem = toLower(name.substr(0, name.find('.')));
            static const char* reserved[] = {
                "con", "prn", "aux", "nul",
                "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
                "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9" };
            for (const char* r : reserved) {
                if (stem == r) return false;
            }
            return true;
        }

        bool mkdirs(const std::string& path) {
            std::error_code ec;
            std::filesystem::create_directories(path, ec);
            return !ec;
        }

        std::uint64_t fileSize(const std::string& path) {
            std::error_code ec;
            const auto sz = std::filesystem::file_size(path, ec);
            return ec ? 0 : static_cast<std::uint64_t>(sz);
        }

        bool readFile(const std::string& path, std::string& out) {
            std::ifstream f(path, std::ios::binary);
            if (!f) return false;
            std::ostringstream ss;
            ss << f.rdbuf();
            out = ss.str();
            return true;
        }

        bool writeFile(const std::string& path, const std::string& data) {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            if (!f) return false;
            f.write(data.data(), static_cast<std::streamsize>(data.size()));
            return f.good();
        }

        bool appendFile(const std::string& path, const std::string& data) {
            std::ofstream f(path, std::ios::binary | std::ios::app);
            if (!f) return false;
            f.write(data.data(), static_cast<std::streamsize>(data.size()));
            return f.good();
        }

        bool removeAll(const std::string& path) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            return !ec;
        }

        std::string trim(const std::string& s) {
            const auto b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos) return "";
            const auto e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        std::string toLower(const std::string& s) {
            std::string r = s;
            for (char& c : r) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return r;
        }

    } // namespace utils
} // namespace lancloud
