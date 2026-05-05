# 快速开始指南

## 🚀 5 分钟上手

### 前置条件

- Android Studio 已安装
- Android 设备或模拟器
- HTTPS 服务器已部署（可选）

### 步骤 1: 构建 AAR 库（可选）

如果你想在自己的项目中使用 TCC 编译器：

```bash
cd TccCompiler
./gradlew :tcclib:assembleRelease

# 输出位置
# tcclib/build/outputs/aar/tcclib-release.aar
```

### 步骤 2: 安装 VaultUploader 应用

```bash
cd VaultUploader

# 清理并构建
./gradlew clean assembleDebug

# 安装到设备
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### 步骤 3: 使用应用

1. **打开应用**

2. **输入密钥**
   - 输入 64 位 hex 字符串（32 字节）
   - 例如：`0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef`

3. **选择输出格式**
   - `.so (共享库)` - 用于 dlopen() 动态加载
   - `.a (静态库)` - 用于静态链接到 C++ 项目

4. **编译**
   - 点击「① 编译 vault.so」或「① 编译 vault.a」
   - 等待编译完成（通常 1-2 秒）
   - 查看编译结果和文件头信息

5. **上传（可选）**
   - 输入服务器地址（默认：`https://136.115.134.105:38443`）
   - 输入库名称（默认：`mylib`）
   - 点击「② 上传到服务器」
   - 查看上传结果

### 步骤 4: 使用生成的库

#### 方式 A: 在 Android 中使用 .so

```kotlin
// 加载库
val vaultFile = File(filesDir, "vault.so")
System.load(vaultFile.absolutePath)

// 声明 native 方法
external fun get_key(out: ByteArray, outLen: LongArray): Int

// 调用
val key = ByteArray(32)
val keyLen = LongArray(1)
val ret = get_key(key, keyLen)

if (ret == 0) {
    // 使用密钥
    println("Key: ${key.joinToString("") { "%02x".format(it) }}")
    
    // 清零
    key.fill(0)
}
```

#### 方式 B: 在 C++ 中使用 .so

```cpp
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main() {
    // 加载库
    void* handle = dlopen("./vault.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }
    
    // 获取函数
    typedef int (*get_key_fn)(unsigned char*, unsigned long long*);
    get_key_fn get_key = (get_key_fn)dlsym(handle, "get_key");
    
    // 调用
    unsigned char key[32];
    unsigned long long key_len;
    int ret = get_key(key, &key_len);
    
    if (ret == 0) {
        printf("Key (%llu bytes): ", key_len);
        for (int i = 0; i < key_len; i++) {
            printf("%02x", key[i]);
        }
        printf("\n");
    }
    
    // 清零并关闭
    memset(key, 0, sizeof(key));
    dlclose(handle);
    return 0;
}
```

#### 方式 C: 在 C++ 中使用 .a

```cpp
#include <stdio.h>
#include <string.h>

// 声明外部函数
extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

int main() {
    unsigned char key[32];
    unsigned long long key_len;
    
    int ret = get_key(key, &key_len);
    
    if (ret == 0) {
        printf("Key (%llu bytes): ", key_len);
        for (int i = 0; i < key_len; i++) {
            printf("%02x", key[i]);
        }
        printf("\n");
    }
    
    // 清零
    memset(key, 0, sizeof(key));
    return 0;
}
```

编译：
```bash
# 链接静态库
g++ -o myapp main.cpp vault.a -lsodium

# 运行
./myapp
```

## 🧪 验证安装

### 检查应用是否正确安装

```bash
adb shell pm list packages | grep vaultuploader
# 应该看到: package:com.example.vaultuploader
```

### 查看应用日志

```bash
adb logcat | grep -E "KeySeal|MainActivity|VaultUpload"
```

### 测试编译功能

在应用中：
1. 输入测试密钥：`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
2. 选择 `.so` 格式
3. 点击编译
4. 应该看到「✅ vault.so 编译成功」

### 检查文件头

编译成功后，日志应显示：

**共享库 (.so)**：
```
First 16 bytes (hex): 7f 45 4c 46 02 01 01 00 ...
First 16 bytes (ascii): .ELF...
```

**静态库 (.a)**：
```
First 16 bytes (hex): 21 3c 61 72 63 68 3e 0a ...
First 16 bytes (ascii): !<arch>.
```

## 🐛 常见问题

### Q1: 编译失败，提示「密钥长度错误」

**A**: 确保输入的是 64 位 hex 字符串（0-9, a-f），不包含空格或其他字符。

### Q2: 上传失败，提示「网络错误」

**A**: 检查：
1. 服务器是否运行
2. 服务器地址是否正确
3. 设备是否能访问服务器（ping 测试）

### Q3: 应用闪退

**A**: 查看崩溃日志：
```bash
adb logcat | grep -E "AndroidRuntime|FATAL"
```

### Q4: 选择 .a 格式但生成的是 .so

**A**: 确保应用已更新到最新版本：
```bash
cd VaultUploader
./gradlew clean assembleDebug
adb uninstall com.example.vaultuploader
adb install app/build/outputs/apk/debug/app-debug.apk
```

### Q5: 如何验证密钥是否正确？

**A**: 使用测试程序：
```bash
# 拉取生成的库
adb pull /data/data/com.example.vaultuploader/files/vault.so

# 编写测试程序
cat > test.cpp << 'EOF'
#include <dlfcn.h>
#include <stdio.h>
int main() {
    void* h = dlopen("./vault.so", RTLD_NOW);
    int (*get_key)(unsigned char*, unsigned long long*) = 
        (int(*)(unsigned char*, unsigned long long*))dlsym(h, "get_key");
    unsigned char key[32];
    unsigned long long len;
    get_key(key, &len);
    for (int i = 0; i < len; i++) printf("%02x", key[i]);
    printf("\n");
    dlclose(h);
    return 0;
}
EOF

# 编译并运行
g++ test.cpp -ldl -o test
./test
```

## 📚 下一步

- 阅读 [完整文档](README.md)
- 查看 [安全性分析](SECURITY_ANALYSIS.md)
- 学习 [C++ 使用指南](STATIC_LIBRARY_CPP_USAGE.md)
- 了解 [逆向工程演示](REVERSE_ENGINEERING_DEMO.md)（教育目的）

## 🔗 相关链接

- [TinyCC 官网](https://bellard.org/tcc/)
- [libsodium 文档](https://doc.libsodium.org/)
- [Android NDK 指南](https://developer.android.com/ndk/guides)

## 💡 提示

- 首次编译可能需要几秒钟，后续会更快
- 密钥会在内存中立即清零，无需担心泄露
- 生成的库文件很小（7-9 KB），适合网络传输
- 可以离线使用，不依赖服务器

## ⚠️ 安全提醒

- 不要在生产环境中使用硬编码的测试密钥
- 定期轮换密钥
- 结合其他安全措施（代码混淆、完整性检查）
- 了解安全限制（详见 [SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md)）

---

**祝你使用愉快！** 🎉

如有问题，请查看 [DEBUG_AR_UPLOAD.md](DEBUG_AR_UPLOAD.md) 或提交 GitHub Issue。
