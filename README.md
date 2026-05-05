# TCC Dynamic Compiler for Android - Secure Key Vault System

一个基于 TinyCC (TCC) 的 Android 动态编译系统，用于在运行时生成包含加密密钥的原生库（.so 或 .a），实现密钥的安全存储和分发。

## 🎯 核心功能

- **动态编译**：在 Android 设备上实时编译 C 代码为原生库
- **密钥密封**：使用 libsodium 的 X25519 + crypto_box_seal 加密密钥
- **XOR 混淆**：对 pk/sk/sealed 进行 XOR 混淆，防止简单的字符串提取
- **多格式输出**：支持生成共享库 (.so) 和静态库 (.a)
- **HTTPS 上传**：安全上传编译后的库文件到服务器
- **跨平台**：支持 Android (ARM64/x86_64) 和 Ubuntu x86_64

## 📁 项目结构

```
wallet-sdk/
├── TccCompiler/              # Android 库项目
│   ├── app/                  # 示例应用
│   └── tcclib/               # 可重用的 AAR 库模块
│       ├── src/main/cpp/     # JNI 桥接代码 + libtcc 源码
│       │   ├── tcc_bridge.cpp
│       │   ├── libtcc/       # TCC 编译器源码
│       │   └── CMakeLists.txt
│       └── src/main/java/com/example/tcclib/
│           ├── TccCompiler.kt      # JNI 入口
│           └── KeySealCompiler.kt  # 密钥密封编译器
│
├── VaultUploader/            # 独立的上传应用
│   └── app/src/main/java/com/example/vaultuploader/
│       ├── MainActivity.kt         # UI 界面
│       └── VaultUploadClient.kt    # HTTPS 客户端
│
├── https-server/             # C++ HTTPS 服务器
│   ├── server.cpp            # uWebSockets 服务器
│   ├── Makefile
│   └── test_*.sh             # 测试脚本
│
├── tcc-ubuntu/               # Ubuntu x86_64 版本
│   ├── tcc_wrapper.cpp       # C++ 包装库
│   └── test.c                # 测试程序
│
├── examples/                 # 使用示例
│   └── cpp_static_lib/       # C++ 静态库使用示例
│
└── docs/                     # 文档
    ├── SECURITY_ANALYSIS.md       # 安全性分析
    ├── REVERSE_ENGINEERING_DEMO.md # 逆向工程演示
    ├── SECURITY_FAQ.md            # 安全常见问题
    └── STATIC_LIBRARY_CPP_USAGE.md # C++ 使用指南
```

## 🚀 快速开始

### 1. 构建 AAR 库

```bash
cd TccCompiler
./gradlew :tcclib:assembleRelease

# 输出: tcclib/build/outputs/aar/tcclib-release.aar
```

### 2. 使用 VaultUploader 应用

```bash
cd VaultUploader
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

在应用中：
1. 输入 64 位 hex 密钥（32 字节）
2. 选择输出格式（.so 或 .a）
3. 点击「编译」生成 vault 库
4. 点击「上传」发送到服务器

### 3. 启动 HTTPS 服务器

```bash
cd https-server
make
./server 38443

# 测试上传
./test_upload.sh https://localhost:38443
```

## 🔐 技术原理

### 密钥密封流程

```
输入密钥 (32 bytes)
    ↓
生成 X25519 密钥对 (pk, sk)
    ↓
crypto_box_seal(key, pk) → sealed (48 bytes)
    ↓
XOR 混淆 pk/sk/sealed
    ↓
硬编码到 C 源码
    ↓
TCC 动态编译 → vault.so 或 vault.a
    ↓
导出函数: int get_key(unsigned char* out, unsigned long long* out_len)
```

### XOR 混淆机制

- 将 pk/sk/sealed 分割为 8 字节块
- 每个块使用随机 mask 进行 XOR
- 块和 mask 分别存储在不同的静态数组中
- 运行时在栈上重建，使用后立即清零

### 文件格式

**共享库 (.so)**
```
ELF Header: 7f 45 4c 46 ...
导出符号: get_key
链接方式: dlopen() + dlsym()
```

**静态库 (.a)**
```
AR Header: 21 3c 61 72 63 68 3e 0a (!<arch>\n)
包含: key_vault.o
链接方式: 静态链接到可执行文件
```

## 📚 使用场景

### ✅ 适用场景

- **API 密钥保护**：防止简单的字符串提取
- **配置加密**：加密应用配置文件
- **轻量级密钥管理**：不需要硬件支持的场景
- **开发/测试环境**：快速原型开发

### ❌ 不适用场景

- **高价值目标**：支付系统、加密货币钱包
- **合规要求**：需要 FIPS 140-2/3 认证
- **医疗/金融**：需要硬件安全模块 (HSM)
- **长期密钥存储**：建议使用 Android Keystore

## 🛡️ 安全性评估

### 防护能力

| 攻击方式 | 防护等级 | 说明 |
|---------|---------|------|
| strings 提取 | ✅ 完全防护 | XOR 混淆有效 |
| hexdump 搜索 | ✅ 完全防护 | 无明文密钥 |
| 静态分析 | ⚠️ 部分防护 | 需要逆向工程 |
| 动态调试 | ❌ 无防护 | 可在运行时提取 |
| 内存转储 | ❌ 无防护 | 可从内存读取 |

### 攻击成本

- **脚本小子**：无法攻破（需要专业知识）
- **普通开发者**：3-5 小时（使用 IDA/Ghidra）
- **安全专家**：30-60 分钟（熟练使用工具）

### 建议

- 适用于 **攻击成本 > 数据价值** 的场景
- 结合其他安全措施（代码混淆、完整性检查）
- 定期轮换密钥
- 监控异常访问

详见：[SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md)

## 🔧 API 参考

### Kotlin API

```kotlin
// 初始化编译器
val compiler = KeySealCompiler(context)

// 编译为共享库
val result = compiler.compile(
    keyHex = "0123456789abcdef...",  // 64 字符 hex
    outputFile = File(filesDir, "vault.so"),
    format = KeySealCompiler.OutputFormat.SHARED_LIBRARY
)

// 编译为静态库
val result = compiler.compile(
    keyHex = "0123456789abcdef...",
    outputFile = File(filesDir, "vault.a"),
    format = KeySealCompiler.OutputFormat.STATIC_LIBRARY
)

// 检查结果
if (result.success) {
    val bytes = result.soBytes  // 编译后的字节
    val file = result.soFile    // 输出文件
    val sealed = result.sealedKeyHex  // 密封后的密文
}
```

### C/C++ API

```c
// 加载共享库
void* handle = dlopen("vault.so", RTLD_NOW);
int (*get_key)(unsigned char*, unsigned long long*) = 
    dlsym(handle, "get_key");

// 获取密钥
unsigned char key[32];
unsigned long long key_len;
int ret = get_key(key, &key_len);

// 使用密钥...

// 清零
memset(key, 0, sizeof(key));
dlclose(handle);
```

详见：[STATIC_LIBRARY_CPP_USAGE.md](STATIC_LIBRARY_CPP_USAGE.md)

## 🌐 HTTPS 服务器 API

### POST /upload

上传编译后的库文件

**请求**
```json
{
  "key": "",                    // 空字符串（密钥已内嵌）
  "so": "<base64_encoded>",     // Base64 编码的库文件
  "name": "mylib"               // 库名称
}
```

**响应**
```json
{
  "ok": true,
  "msg": "ok",
  "key_size": 0,
  "lib_file": "./uploads/libs/mylib_20260411_123456.so",
  "lib_size": 7300,
  "lib_type": "shared library",
  "file_ext": ".so"
}
```

### GET /list

列出已上传的文件

**响应**
```json
{
  "ok": true,
  "files": [
    {
      "name": "mylib_20260411_123456.so",
      "size": 7300,
      "ext": ".so",
      "path": "./uploads/libs/mylib_20260411_123456.so"
    }
  ]
}
```

## 🧪 测试

### Android 测试

```bash
# 重新编译并安装
cd VaultUploader
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk

# 查看日志
adb logcat | grep -E "KeySeal|MainActivity"
```

### 服务器测试

```bash
cd https-server

# 运行完整测试套件
./test_server.sh

# 测试特定格式
./test_upload.sh https://localhost:38443
```

### 格式验证

```bash
# 检查文件格式
./check_format uploads/libs/mylib_*.so

# 或使用 hexdump
hexdump -C vault.a | head -n 3
# 应该看到: 21 3c 61 72 63 68 3e 0a (!<arch>)
```

## 📖 文档

- [安全性分析](SECURITY_ANALYSIS.md) - 详细的安全评估和风险分析
- [逆向工程演示](REVERSE_ENGINEERING_DEMO.md) - 如何逆向 vault.so（教育目的）
- [安全常见问题](SECURITY_FAQ.md) - 常见安全问题解答
- [C++ 使用指南](STATIC_LIBRARY_CPP_USAGE.md) - 如何在 C++ 项目中使用
- [调试指南](DEBUG_AR_UPLOAD.md) - 故障排查步骤

## 🔨 构建要求

### Android

- Android Studio Arctic Fox+
- NDK 21+
- Gradle 7.0+
- CMake 3.18+

### Ubuntu

- GCC 9+
- CMake 3.16+
- libsodium-dev
- uWebSockets (服务器)

## 📝 许可证

本项目使用 TinyCC (LGPL) 和 libsodium (ISC License)。

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## ⚠️ 免责声明

本项目仅供学习和研究使用。在生产环境中使用前，请进行充分的安全评估。作者不对因使用本项目造成的任何损失负责。

## 📞 联系方式

如有问题，请提交 GitHub Issue。

---

**最后更新**: 2026-04-11
