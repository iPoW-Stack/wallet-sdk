# 调试 AR 文件上传问题

## ✅ 问题已修复

**根本原因**：MainActivity.kt 未将 `format` 参数传递给编译器，导致始终使用默认的 SHARED_LIBRARY 格式。

**修复内容**：
```kotlin
// 修复前
val result = compiler.compile(
    keyHex = keyHex,
    outputFile = outputFile
)

// 修复后
val format = if (isStaticLib) {
    KeySealCompiler.OutputFormat.STATIC_LIBRARY
} else {
    KeySealCompiler.OutputFormat.SHARED_LIBRARY
}

val result = compiler.compile(
    keyHex = keyHex,
    outputFile = outputFile,
    format = format  // ← 添加此参数
)
```

## 🔄 验证步骤

### 1. 重新编译并安装应用

```bash
cd VaultUploader
./gradlew clean assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### 2. 测试静态库生成

在应用中：
1. 输入 64 位 hex 密钥
2. 选择 `.a (静态库)` 选项
3. 点击「编译」
4. 查看日志输出

### 3. 检查 Android 日志

```bash
adb logcat | grep -E "KeySeal|MainActivity"
```

**期望输出**：
```
KeySeal Archive: AR magic bytes: 21 3c 61 72 63 68 3e 0a
MainActivity: First 16 bytes (hex): 21 3c 61 72 63 68 3e 0a ...
MainActivity: First 16 bytes (ascii): !<arch>.
```

**如果看到 ELF 头**：
```
MainActivity: First 16 bytes (hex): 7f 45 4c 46 ...
MainActivity: First 16 bytes (ascii): .ELF...
```
说明应用未重新编译或未重新安装。

### 4. 测试上传到服务器

点击「上传」按钮，检查服务器响应：

**期望响应**：
```json
{
  "ok": true,
  "lib_type": "static library",
  "file_ext": ".a",
  "lib_file": "./uploads/libs/mylib_20260411_123456.a"
}
```

### 5. 验证服务器端文件

```bash
# 在服务器上
cd ~/wallet-sdk/https-server
ls -lh uploads/libs/*.a

# 检查文件格式
./check_format uploads/libs/mylib_*.a
```

**期望输出**：
```
Format: AR (Archive)
Type: Static library
Detected extension: .a
```

## 🧪 完整测试流程

## 🧪 完整测试流程

### 测试 1: 共享库 (.so)

```bash
# 1. 在应用中选择 .so 选项
# 2. 编译并上传
# 3. 检查日志
adb logcat | grep "First 16 bytes"
# 应该看到: 7f 45 4c 46 (.ELF)

# 4. 检查服务器响应
# 应该包含: "file_ext": ".so", "lib_type": "shared library"
```

### 测试 2: 静态库 (.a)

```bash
# 1. 在应用中选择 .a 选项
# 2. 编译并上传
# 3. 检查日志
adb logcat | grep "First 16 bytes"
# 应该看到: 21 3c 61 72 63 68 3e 0a (!<arch>)

# 4. 检查服务器响应
# 应该包含: "file_ext": ".a", "lib_type": "static library"
```

### 测试 3: 服务器端验证

```bash
cd https-server

# 运行完整测试套件
./test_server.sh

# 应该看到所有格式正确检测：
# ✓ .o file correctly detected
# ✓ .a file correctly detected
# ✓ .so file correctly detected
```

## 🐛 故障排查

### 问题 1: 应用仍生成 .so 文件

**症状**：选择 .a 选项，但日志显示 `7f 45 4c 46`

**解决方案**：
```bash
# 1. 清理构建缓存
cd VaultUploader
./gradlew clean

# 2. 重新编译
./gradlew assembleDebug

# 3. 卸载旧版本
adb uninstall com.example.vaultuploader

# 4. 安装新版本
adb install app/build/outputs/apk/debug/app-debug.apk
```

### 问题 2: 服务器检测错误

**症状**：Android 生成正确的 AR 文件，但服务器保存为 .so

**解决方案**：
```bash
# 1. 重新编译服务器
cd https-server
make clean
make

# 2. 停止旧进程
pkill -f "./server"

# 3. 启动新服务器
./server 38443

# 4. 测试检测逻辑
./test_server.sh
```

### 问题 3: 编译失败

**症状**：点击编译按钮后显示错误

**检查项**：
1. 密钥长度是否为 64 字符
2. 密钥是否为有效的 hex 字符串
3. 查看详细错误日志：`adb logcat | grep -E "KeySeal|TCC"`

### 问题 4: 上传失败

**症状**：编译成功但上传失败

**检查项**：
1. 服务器是否运行：`curl -k https://136.115.134.105:38443/health`
2. 网络连接是否正常
3. 证书是否信任（应用已配置信任 localhost）
4. 查看详细错误：`adb logcat | grep VaultUpload`

## 📊 预期结果对比

### 共享库 (.so)

| 项目 | 值 |
|-----|---|
| 文件头 (hex) | `7f 45 4c 46 02 01 01 00` |
| 文件头 (ascii) | `.ELF....` |
| 文件大小 | ~7-8 KB |
| 服务器检测 | `shared library` |
| 文件扩展名 | `.so` |

### 静态库 (.a)

| 项目 | 值 |
|-----|---|
| 文件头 (hex) | `21 3c 61 72 63 68 3e 0a` |
| 文件头 (ascii) | `!<arch>.` |
| 文件大小 | ~8-9 KB |
| 服务器检测 | `static library` |
| 文件扩展名 | `.a` |

## 🔍 调试工具

### 1. 检查文件格式

```bash
# 编译检查工具
cd https-server
g++ -std=c++17 check_format.cpp -o check_format

# 检查文件
./check_format path/to/file
```

### 2. 查看文件头

```bash
# 使用 hexdump
hexdump -C vault.a | head -n 3

# 使用 xxd
xxd vault.a | head -n 3

# 使用 od
od -A x -t x1z -N 64 vault.a
```

### 3. 提取文件从设备

```bash
# 查找文件
adb shell "find /data/data/com.example.vaultuploader -name 'vault.*'"

# 拉取文件
adb pull /data/data/com.example.vaultuploader/files/vault.a

# 检查格式
file vault.a
# 应该显示: current ar archive
```

## ✅ 验证清单

完成以下检查以确认修复成功：

- [ ] MainActivity.kt 包含 `format` 参数传递
- [ ] 应用已重新编译（`./gradlew clean assembleDebug`）
- [ ] 应用已重新安装（`adb install -r`）
- [ ] 选择 .a 选项时，日志显示 `!<arch>`
- [ ] 选择 .so 选项时，日志显示 `.ELF`
- [ ] 服务器正确检测 .a 文件为 `static library`
- [ ] 服务器正确检测 .so 文件为 `shared library`
- [ ] 上传的文件扩展名与选择的格式匹配

## 📝 相关文件

- `VaultUploader/app/src/main/java/com/example/vaultuploader/MainActivity.kt` - 修复位置
- `TccCompiler/tcclib/src/main/java/com/example/tcclib/KeySealCompiler.kt` - 格式处理逻辑
- `TccCompiler/tcclib/src/main/cpp/tcc_bridge.cpp` - AR 生成实现
- `https-server/server.cpp` - 服务器端检测逻辑

## 🎓 技术细节

### AR 文件格式

```
Offset  Content
------  -------
0x00    "!<arch>\n"           (8 bytes, AR 魔数)
0x08    文件头                 (60 bytes)
        - 文件名 (16 bytes)
        - 时间戳 (12 bytes)
        - UID (6 bytes)
        - GID (6 bytes)
        - 模式 (8 bytes)
        - 大小 (10 bytes)
        - 结束标记 "`\n" (2 bytes)
0x44    文件内容 (.o 文件)
        如果大小为奇数，添加 '\n' 填充
```

### ELF 文件格式

```
Offset  Content
------  -------
0x00    0x7f 'E' 'L' 'F'     (4 bytes, ELF 魔数)
0x04    类别 (1=32位, 2=64位)
0x05    字节序 (1=小端, 2=大端)
0x06    版本 (1)
0x10    类型 (1=.o, 3=.so)
...
```

## 🔗 相关文档

- [README.md](README.md) - 项目总览
- [SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md) - 安全性分析
- [STATIC_LIBRARY_CPP_USAGE.md](STATIC_LIBRARY_CPP_USAGE.md) - C++ 使用指南

---

**最后更新**: 2026-04-11 (修复已应用)
