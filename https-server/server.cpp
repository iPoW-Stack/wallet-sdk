/**
 * HTTPS Server based on uWebSockets
 *
 * 接口：
 *   POST /upload   - JSON body，最大 1MB
 *                    { "key": "<base64>", "so": "<base64>", "name": "<可选文件名>" }
 *                    key: 加密密钥二进制（base64）
 *                    so:  .so 二进制（base64），解码后保存为文件
 *   GET  /health   - 健康检查
 */

#include "uWebSockets-20.62.0/src/App.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <stdexcept>

namespace fs = std::filesystem;

static const std::string SO_DIR = "./uploads/libs";
static constexpr size_t  MAX_BODY = 1 * 1024 * 1024; // 1 MB

// ── base64 解码 ───────────────────────────────────────────────────────────────
static const std::string B64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_decode(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3 / 4);
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' ) { bits -= 6; continue; }
        auto pos = B64_CHARS.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + (int)pos;
        bits += 6;
        if (bits >= 0) {
            out.push_back((char)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ── 极简 JSON 字段提取（只处理字符串值）────────────────────────────────────────
// 返回 pair<是否找到, 字符串值>
static std::pair<bool, std::string> json_get(const std::string& json, const std::string& key) {
    // 找 "key":
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {false, {}};
    pos += needle.size();
    // 跳过空白和冒号
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t')) ++pos;
    if (pos >= json.size() || json[pos] != '"') return {false, {}};
    ++pos; // skip opening "
    std::string val;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) { ++pos; } // skip escape
        val.push_back(json[pos++]);
    }
    return {true, val};
}

// ── 时间戳文件名 ──────────────────────────────────────────────────────────────
static std::string timestamp_name(const std::string& prefix, const std::string& ext) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << prefix << "_"
       << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S")
       << "_" << ms.count() << ext;
    return ss.str();
}

// ── 检测文件类型 ──────────────────────────────────────────────────────────────
static std::string detect_file_type(const std::string& data) {
    if (data.size() < 8) return ".bin";
    
    // 调试：打印前 16 字节
    std::cout << "[DEBUG] File header (first 16 bytes): ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    std::cout << "\n";
    std::cout << "[DEBUG] As ASCII: ";
    for (size_t i = 0; i < 16 && i < data.size(); i++) {
        char c = data[i];
        std::cout << (c >= 32 && c < 127 ? c : '.');
    }
    std::cout << "\n";
    
    // ELF 魔数: 0x7f 'E' 'L' 'F'
    if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
        // 检查 ELF 类型
        if (data.size() >= 18) {
            uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
            std::cout << "[DEBUG] ELF e_type = " << e_type << "\n";
            // ET_DYN (3) = 共享库
            if (e_type == 3) return ".so";
            // ET_REL (1) = 可重定位文件 (.o)
            if (e_type == 1) return ".o";
        }
        return ".so"; // 默认为共享库
    }
    
    // AR 魔数: "!<arch>\n"
    if (data.size() >= 8 && 
        data[0] == '!' && data[1] == '<' && 
        data[2] == 'a' && data[3] == 'r' &&
        data[4] == 'c' && data[5] == 'h' &&
        data[6] == '>' && data[7] == '\n') {
        std::cout << "[DEBUG] Detected AR format\n";
        return ".a";
    }
    
    std::cout << "[DEBUG] Unknown format\n";
    return ".bin"; // 未知格式
}

// ── JSON 响应 ─────────────────────────────────────────────────────────────────
static std::string json_resp(bool ok, const std::string& msg, const std::string& extra = "") {
    std::string body = R"({"ok":)" + std::string(ok ? "true" : "false") +
                       R"(,"msg":")" + msg + "\"";
    if (!extra.empty()) body += "," + extra;
    body += "}";
    return body;
}

int main(int argc, char* argv[]) {
    fs::create_directories(SO_DIR);

    // 端口可通过命令行参数指定，默认 8443
    int PORT = 8443;
    if (argc > 1) {
        try {
            PORT = std::stoi(argv[1]);
            if (PORT < 1024 || PORT > 65535) {
                std::cerr << "Port must be between 1024-65535\n";
                return 1;
            }
        } catch (...) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    uWS::SocketContextOptions ssl_opts{};
    ssl_opts.key_file_name  = "certs/key.pem";
    ssl_opts.cert_file_name = "certs/cert.pem";
    ssl_opts.passphrase     = "";

    // 检查证书文件是否存在
    if (!fs::exists(ssl_opts.key_file_name) || !fs::exists(ssl_opts.cert_file_name)) {
        std::cerr << "Error: SSL certificate files not found!\n";
        std::cerr << "  Expected: " << ssl_opts.key_file_name << "\n";
        std::cerr << "  Expected: " << ssl_opts.cert_file_name << "\n";
        return 1;
    }

    uWS::SSLApp app(ssl_opts);

    // ── GET /health ───────────────────────────────────────────────────────────
    app.get("/health", [](auto* res, auto* /*req*/) {
        res->writeHeader("Content-Type", "application/json")
           ->end(json_resp(true, "server is running"));
    });

    // ── GET /list ─────────────────────────────────────────────────────────────
    // 列出已上传的库文件
    app.get("/list", [](auto* res, auto* /*req*/) {
        std::string files_json = "[";
        bool first = true;
        
        try {
            for (const auto& entry : fs::directory_iterator(SO_DIR)) {
                if (!entry.is_regular_file()) continue;
                
                auto path = entry.path();
                auto size = fs::file_size(path);
                auto ext = path.extension().string();
                auto name = path.filename().string();
                
                if (!first) files_json += ",";
                first = false;
                
                files_json += "{";
                files_json += R"("name":")" + name + "\",";
                files_json += R"("size":)" + std::to_string(size) + ",";
                files_json += R"("ext":")" + ext + "\",";
                files_json += R"("path":")" + path.string() + "\"";
                files_json += "}";
            }
        } catch (...) {
            res->writeStatus("500 Internal Server Error")
               ->writeHeader("Content-Type", "application/json")
               ->end(json_resp(false, "failed to list files"));
            return;
        }
        
        files_json += "]";
        
        std::string body = R"({"ok":true,"files":)" + files_json + "}";
        res->writeHeader("Content-Type", "application/json")
           ->end(body);
    });

    // ── POST /upload ──────────────────────────────────────────────────────────
    // Body (JSON, max 1MB):
    //   { "key": "<base64 binary>", "so": "<base64 binary>", "name": "<optional>" }
    app.post("/upload", [](auto* res, auto* /*req*/) {
        auto body_buf = std::make_shared<std::string>();

        res->onData([res, body_buf](std::string_view chunk, bool is_last) {
            body_buf->append(chunk.data(), chunk.size());

            if (body_buf->size() > MAX_BODY) {
                res->writeStatus("413 Payload Too Large")
                   ->writeHeader("Content-Type", "application/json")
                   ->end(json_resp(false, "payload too large (max 1MB)"));
                return;
            }

            if (!is_last) return;

            // 解析 JSON 字段
            auto [key_found, key_b64] = json_get(*body_buf, "key");
            auto [so_found, so_b64] = json_get(*body_buf, "so");
            auto [name_found, name] = json_get(*body_buf, "name");

            if (!key_found || !so_found) {
                res->writeStatus("400 Bad Request")
                   ->writeHeader("Content-Type", "application/json")
                   ->end(json_resp(false, "both 'key' and 'so' fields are required"));
                return;
            }

            // 解码
            std::string key_bytes = key_b64.empty() ? "" : base64_decode(key_b64);
            std::string so_bytes  = so_b64.empty()  ? "" : base64_decode(so_b64);

            // 检测文件类型
            std::string file_ext = detect_file_type(so_bytes);
            std::string file_type = (file_ext == ".a") ? "static library" :
                                   (file_ext == ".so") ? "shared library" :
                                   (file_ext == ".o") ? "object file" : "binary";

            std::cout << "[UPLOAD] key=" << key_bytes.size()
                      << "B  so=" << so_bytes.size() << "B  type=" << file_type << "\n";

            // 保存文件
            std::string lib_file;
            if (!so_bytes.empty()) {
                // 安全文件名
                std::string base = name.empty() ? "lib" : fs::path(name).stem().string();
                if (base.empty() || base == "." || base == "..") base = "lib";
                lib_file = SO_DIR + "/" + timestamp_name(base, file_ext);

                std::ofstream ofs(lib_file, std::ios::binary);
                if (!ofs) {
                    res->writeStatus("500 Internal Server Error")
                       ->writeHeader("Content-Type", "application/json")
                       ->end(json_resp(false, "failed to save library file"));
                    return;
                }
                ofs.write(so_bytes.data(), (std::streamsize)so_bytes.size());
                ofs.close();
                std::cout << "[LIB] saved " << so_bytes.size()
                          << " bytes -> " << lib_file << " (" << file_type << ")\n";
            }

            // key 在内存中使用（此处仅打印摘要，业务逻辑在此扩展）
            if (!key_bytes.empty()) {
                std::cout << "[KEY] received " << key_bytes.size() << " bytes\n";
                // TODO: 使用 key_bytes 进行解密或其他操作
            }

            // 构造响应
            std::string extra = R"("key_size":)" + std::to_string(key_bytes.size());
            if (!lib_file.empty()) {
                extra += R"(,"lib_file":")" + lib_file + "\"";
                extra += R"(,"lib_size":)" + std::to_string(so_bytes.size());
                extra += R"(,"lib_type":")" + file_type + "\"";
                extra += R"(,"file_ext":")" + file_ext + "\"";
            }
            res->writeHeader("Content-Type", "application/json")
               ->end(json_resp(true, "ok", extra));
        });

        res->onAborted([body_buf]() {
            std::cerr << "[UPLOAD] aborted\n";
        });
    });

    // ── 404 fallback ──────────────────────────────────────────────────────────
    app.any("/*", [](auto* res, auto* /*req*/) {
        res->writeStatus("404 Not Found")
           ->writeHeader("Content-Type", "application/json")
           ->end(json_resp(false, "not found"));
    });

    app.listen(PORT, [PORT](auto* token) {
        if (token) {
            std::cout << "HTTPS server listening on https://0.0.0.0:" << PORT << "\n";
            std::cout << "  POST /upload  { \"key\":\"<b64>\", \"so\":\"<b64>\", \"name\":\"<opt>\" }\n";
            std::cout << "  GET  /health\n";
            std::cout << "  GET  /list    (list uploaded libraries)\n";
            std::cout << "\nSupported formats:\n";
            std::cout << "  .so  - Shared library (ELF ET_DYN)\n";
            std::cout << "  .a   - Static library (AR archive)\n";
            std::cout << "  .o   - Object file (ELF ET_REL)\n";
        } else {
            std::cerr << "Failed to listen on port " << PORT << "\n";
            std::cerr << "Possible reasons:\n";
            std::cerr << "  1. Port already in use (check: netstat -tuln | grep " << PORT << ")\n";
            std::cerr << "  2. Permission denied (ports < 1024 need root)\n";
            std::cerr << "  3. SSL certificate error\n";
            std::exit(1);
        }
    });

    app.run();
    return 0;
}
