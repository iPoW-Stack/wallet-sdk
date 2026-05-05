# 更新日志

## [2026-04-11] - 修复静态库生成问题

### 🐛 Bug 修复

#### MainActivity.kt 未传递 format 参数

**问题描述**：
- 用户在 VaultUploader 中选择 `.a (静态库)` 选项
- 应用显示编译成功
- 但实际生成的仍是 `.so (共享库)` 格式
- 服务器端检测为 ELF 格式而非 AR 格式

**根本原因**：
`MainActivity.kt` 中调用编译器时未传递 `format` 参数，导致始终使用默认的 `SHARED_LIBRARY` 格式。

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

**影响范围**：
- ✅ 修复了静态库生成功能
- ✅ 用户选择 .a 选项时正确生成 AR 格式文件
- ✅ 服务器端正确检测并保存为 .a 文件
- ✅ 不影响共享库 (.so) 生成功能

**测试验证**：
1. 重新编译并安装应用
2. 选择 `.a (静态库)` 选项
3. 编译后检查日志，应显示 `21 3c 61 72 63 68 3e 0a` (AR 魔数)
4. 上传到服务器，响应应包含 `"file_ext": ".a"`

### 📚 文档更新

#### README.md - 完整项目文档

新增内容：
- 🎯 核心功能介绍
- 📁 项目结构说明
- 🚀 快速开始指南
- 🔐 技术原理详解
- 📚 使用场景分析
- 🛡️ 安全性评估
- 🔧 完整 API 参考
- 🌐 HTTPS 服务器 API 文档
- 🧪 测试指南
- 📖 相关文档链接

#### DEBUG_AR_UPLOAD.md - 调试指南更新

更新内容：
- ✅ 标记问题已修复
- 🔄 添加验证步骤
- 🧪 完整测试流程
- 🐛 故障排查指南
- 📊 预期结果对比表
- 🔍 调试工具说明
- ✅ 验证清单
- 🎓 技术细节说明

### 🔍 技术细节

#### AR 文件格式生成

C++ 层 (`tcc_bridge.cpp`) 已正确实现：
```cpp
// AR 魔数
const char* AR_MAGIC = "!<arch>\n";
archiveBuf.insert(archiveBuf.end(), AR_MAGIC, AR_MAGIC + 8);

// 60 字节文件头
char header[60];
// 文件名、时间戳、UID、GID、模式、大小、结束标记

// 文件内容 + 填充
archiveBuf.insert(archiveBuf.end(), objBuf.begin(), objBuf.end());
if (objSize % 2 == 1) {
    archiveBuf.push_back('\n');
}
```

#### 服务器端检测逻辑

服务器 (`server.cpp`) 已正确实现：
```cpp
// 检测 AR 魔数
if (data.size() >= 8 && 
    data[0] == '!' && data[1] == '<' && 
    data[2] == 'a' && data[3] == 'r' &&
    data[4] == 'c' && data[5] == 'h' && 
    data[6] == '>' && data[7] == '\n') {
    return ".a";  // static library
}

// 检测 ELF 魔数
if (data.size() >= 4 && 
    data[0] == 0x7f && data[1] == 'E' && 
    data[2] == 'L' && data[3] == 'F') {
    // 检查 e_type 区分 .o 和 .so
}
```

### ✅ 验证结果

#### 服务器端测试

```bash
$ ./test_server.sh
✓ .o file correctly detected
✓ .a file correctly detected
✓ .so file correctly detected
```

所有格式检测正常。

#### Android 端测试

修复后预期日志：
```
KeySeal Archive: AR magic bytes: 21 3c 61 72 63 68 3e 0a
MainActivity: First 16 bytes (hex): 21 3c 61 72 63 68 3e 0a ...
MainActivity: First 16 bytes (ascii): !<arch>.
```

### 📝 待办事项

- [ ] 用户重新编译 VaultUploader 应用
- [ ] 用户重新安装应用到设备
- [ ] 用户测试静态库生成功能
- [ ] 用户验证上传到服务器的文件格式

### 🔗 相关文件

修改的文件：
- `VaultUploader/app/src/main/java/com/example/vaultuploader/MainActivity.kt`
- `README.md`
- `DEBUG_AR_UPLOAD.md`

新增的文件：
- `CHANGELOG.md` (本文件)

未修改但相关的文件：
- `TccCompiler/tcclib/src/main/java/com/example/tcclib/KeySealCompiler.kt` (已正确实现)
- `TccCompiler/tcclib/src/main/cpp/tcc_bridge.cpp` (已正确实现)
- `https-server/server.cpp` (已正确实现)

---

## 历史版本

### [2026-04-10] - 初始版本

- ✅ 创建 TccCompiler Android 项目
- ✅ 实现 libsodium 密钥密封
- ✅ 实现 XOR 混淆
- ✅ 重构为 AAR 库模块
- ✅ 创建 VaultUploader 应用
- ✅ 创建 HTTPS 服务器
- ✅ 添加静态库支持
- ✅ 创建 Ubuntu x86_64 版本
- ✅ 编写安全性分析文档
- ✅ 编写使用指南

### [2026-04-11] - Bug 修复和文档完善

- 🐛 修复静态库生成问题
- 📚 完善 README.md
- 📚 更新 DEBUG_AR_UPLOAD.md
- 📝 创建 CHANGELOG.md
