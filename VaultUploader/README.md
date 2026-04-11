# VaultUploader: 密钥硬编码与安全上传方案

本项目提供了一套完整的 Android 密钥安全保护方案，通过在原生层（Native Layer）硬编码加密密钥并将其编译为 `.so` 库，结合安全上传机制，实现密钥的隐匿与分发。

## 1. 技术架构 (Technical Architecture)

- **Native Hardcoding (TCC/C)**: 使用 `tcclib` 将 256 位 (32字节) 的 16 进制密钥硬编码进 C 源码中。
- **Dynamic Compilation**: 在 Android 设备上动态生成 C 代码并调用 TCC 编译器将其编译为针对当前架构（如 `aarch64`）的 `vault.so` 文件。
- **JNA (Java Native Access)**: 使用 JNA AAR 库加载和执行原生代码。相比 JNI，JNA 提供了更简单的调用方式，同时我们通过 AAR 确保了 `libjnidispatch.so` 的正确打包。
- **Lazysodium**: 集成 Libsodium 的 Android 版本，用于底层的密码学支持。
- **HTTPS/TLS**: 使用 OkHttpClient 实现支持自签名证书的 HTTPS 上传，确保 `.so` 文件在传输过程中的加密。

## 2. 使用方法 (Usage)

1.  **输入密钥**: 在应用界面输入 64 位的 Hex 密钥（32 字节）。
2.  **点击「编译」**: 
    - 系统会生成包含密钥的 C 代码。
    - 调用内置编译器生成 `vault.so`。
    - 成功后显示 `sealedKeyHex`（混淆后的密文，可公开）。
3.  **配置服务器**: 默认地址已设置为 `https://136.115.134.105:38443`。
4.  **点击「上传」**:
    - 将生成的 `vault.so` 转换为 Base64。
    - 通过 HTTPS 接口上传。服务器仅接收 `.so` 文件，不再接收原始密钥。

## 3. 安全性分析 (Security Analysis)

- **密钥隐匿**: 密钥不以字符串形式存在于 Java 层，而是直接编译进 `.so` 二进制文件的 `.data` 或 `.text` 段中，增加了逆向工程的难度。
- **传输安全**: 强制使用 HTTPS 进行上传，防止中间人攻击（MITM）。
- **最小化泄露**: 上传接口中 `key` 字段为空字符串。原始密钥从未离开设备，服务器端通过解析 `.so` 来获取逻辑，而不是直接存储密钥字符串。
- **动态隔离**: 每次生成的 `.so` 都可以包含不同的混淆逻辑，提高了系统的整体韧性。

## 4. 维护与排除故障

- **Gradle 版本**: 项目适配 **AGP 8.2.0** 和 **Gradle 8.2**，请勿随意升级到 Gradle 9.0 以免出现配置冲突。
- **依赖冲突**: JNA 必须以 `@aar` 形式引入，以确保 Android 架构所需的 `libjnidispatch.so` 被正确包含。
- **Git 大文件**: 编译产生的 `.hprof` 或大型二进制文件已通过 `.gitignore` 过滤。若遇到推送失败，请参考历史记录清理命令。

---
*Created by VaultUploader Team*
