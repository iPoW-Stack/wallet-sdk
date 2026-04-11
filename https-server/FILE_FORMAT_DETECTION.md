# 文件格式自动检测

## 概述

HTTPS 服务器现在支持自动检测上传的库文件格式，并使用正确的扩展名保存。

## 支持的格式

### 1. 共享库 (.so)

**魔数**: `0x7f 'E' 'L' 'F'` (ELF 格式)  
**类型**: ELF ET_DYN (e_type = 3)

**检测逻辑**:
```cpp
// 检查 ELF 魔数
if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
    // 读取 e_type 字段（偏移 16）
    uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
    if (e_type == 3) return ".so"; // ET_DYN
}
```

**示例**:
```bash
# 上传共享库
SO_B64=$(base64 < libtest.so)
curl -sk -X POST https://localhost:8443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${SO_B64}\",\"name\":\"test\"}"

# 服务器自动检测为 .so 并保存为 test_timestamp.so
```

### 2. 静态库 (.a)

**魔数**: `!<arch>\n` (AR 格式)

**检测逻辑**:
```cpp
// 检查 AR 魔数（8 字节）
if (data[0] == '!' && data[1] == '<' && 
    data[2] == 'a' && data[3] == 'r' &&
    data[4] == 'c' && data[5] == 'h' &&
    data[6] == '>' && data[7] == '\n') {
    return ".a";
}
```

**AR 文件结构**:
```
┌─────────────────────┐
│ "!<arch>\n"         │  8 bytes - 魔数
├─────────────────────┤
│ 文件头 (60 bytes)   │  文件名、时间戳、权限等
├─────────────────────┤
│ 成员文件内容        │  通常是 .o 文件
├─────────────────────┤
│ 填充字节 (可选)     │  如果大小为奇数
└─────────────────────┘
```

**示例**:
```bash
# 上传静态库
AR_B64=$(base64 < libtest.a)
curl -sk -X POST https://localhost:8443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test\"}"

# 服务器自动检测为 .a 并保存为 test_timestamp.a
```

### 3. 目标文件 (.o)

**魔数**: `0x7f 'E' 'L' 'F'` (ELF 格式)  
**类型**: ELF ET_REL (e_type = 1)

**检测逻辑**:
```cpp
// 检查 ELF 魔数
if (data[0] == 0x7f && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
    // 读取 e_type 字段（偏移 16）
    uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
    if (e_type == 1) return ".o"; // ET_REL
}
```

**示例**:
```bash
# 上传目标文件
OBJ_B64=$(base64 < test.o)
curl -sk -X POST https://localhost:8443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${OBJ_B64}\",\"name\":\"test\"}"

# 服务器自动检测为 .o 并保存为 test_timestamp.o
```

### 4. 未知格式 (.bin)

如果文件不匹配以上任何格式，将保存为 `.bin`。

## ELF 文件类型

ELF 文件头中的 `e_type` 字段（偏移 16，2 字节）定义了文件类型：

| e_type | 名称 | 描述 | 扩展名 |
|--------|------|------|--------|
| 0 | ET_NONE | 未知类型 | .bin |
| 1 | ET_REL | 可重定位文件 | .o |
| 2 | ET_EXEC | 可执行文件 | (无) |
| 3 | ET_DYN | 共享库 | .so |
| 4 | ET_CORE | Core 文件 | .core |

## ELF 文件头结构

```c
// ELF64 文件头（简化）
struct Elf64_Ehdr {
    unsigned char e_ident[16];  // 魔数和标识
    uint16_t      e_type;       // 文件类型（偏移 16）
    uint16_t      e_machine;    // 架构
    uint32_t      e_version;    // 版本
    uint64_t      e_entry;      // 入口点
    // ... 其他字段
};

// e_ident[0..3] = 0x7f 'E' 'L' 'F'
// e_ident[4] = 1 (32位) 或 2 (64位)
// e_ident[5] = 1 (小端) 或 2 (大端)
```

## 实现细节

### detect_file_type() 函数

```cpp
static std::string detect_file_type(const std::string& data) {
    if (data.size() < 8) return ".bin";
    
    // ELF 魔数检测
    if (data[0] == 0x7f && data[1] == 'E' && 
        data[2] == 'L' && data[3] == 'F') {
        if (data.size() >= 18) {
            uint16_t e_type = *reinterpret_cast<const uint16_t*>(&data[16]);
            if (e_type == 3) return ".so";  // ET_DYN
            if (e_type == 1) return ".o";   // ET_REL
        }
        return ".so"; // 默认为共享库
    }
    
    // AR 魔数检测
    if (data.size() >= 8 && 
        data[0] == '!' && data[1] == '<' && 
        data[2] == 'a' && data[3] == 'r' &&
        data[4] == 'c' && data[5] == 'h' &&
        data[6] == '>' && data[7] == '\n') {
        return ".a";
    }
    
    return ".bin"; // 未知格式
}
```

### 使用示例

```cpp
// 在 /upload 端点中
std::string so_bytes = base64_decode(so_b64);
std::string file_ext = detect_file_type(so_bytes);
std::string file_type = (file_ext == ".a") ? "static library" :
                       (file_ext == ".so") ? "shared library" :
                       (file_ext == ".o") ? "object file" : "binary";

std::string lib_file = SO_DIR + "/" + timestamp_name(base, file_ext);
```

## 响应格式

服务器返回的 JSON 包含文件类型信息：

```json
{
  "ok": true,
  "msg": "ok",
  "key_size": 0,
  "lib_file": "./uploads/libs/mylib_20260411_113045_123.a",
  "lib_size": 8192,
  "lib_type": "static library",
  "file_ext": ".a"
}
```

字段说明：
- `lib_file`: 保存的文件路径
- `lib_size`: 文件大小（字节）
- `lib_type`: 人类可读的类型描述
- `file_ext`: 文件扩展名

## 测试

### 手动测试

```bash
# 1. 创建测试文件
echo "test" > test.txt

# 2. 创建 ELF 共享库头部
printf '\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x3e\x00' > test.so

# 3. 创建 AR 静态库
echo '!<arch>' > test.a

# 4. 上传并检查
for file in test.so test.a test.txt; do
    echo "Testing $file..."
    B64=$(base64 < $file)
    curl -sk -X POST https://localhost:8443/upload \
      -H "Content-Type: application/json" \
      -d "{\"key\":\"\",\"so\":\"${B64}\",\"name\":\"test\"}"
    echo ""
done
```

### 自动测试

使用提供的测试脚本：

```bash
./test_upload.sh https://localhost:8443
```

## 安全考虑

1. **魔数验证**: 只检查文件头，不解析完整文件结构
2. **大小限制**: 最大 1MB，防止内存溢出
3. **文件名清理**: 移除路径遍历字符（`.`, `..`）
4. **时间戳命名**: 避免文件名冲突

## 扩展支持

如需支持更多格式，在 `detect_file_type()` 中添加：

```cpp
// Mach-O (macOS)
if (data[0] == 0xfe && data[1] == 0xed && 
    data[2] == 0xfa && data[3] == 0xce) {
    return ".dylib";
}

// PE (Windows)
if (data[0] == 'M' && data[1] == 'Z') {
    return ".dll";
}
```

## 参考资料

- [ELF Format Specification](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [AR Archive Format](https://en.wikipedia.org/wiki/Ar_(Unix))
- [File Command Source](https://github.com/file/file)
