# VaultUploader 静态库支持指南

## 概述

VaultUploader 现在支持编译为两种格式：
- **共享库 (.so)** - 动态链接，需要 dlopen 加载
- **静态库 (.a)** - 静态链接，直接编译进目标程序

## 使用方法

### 1. 在 App 中选择格式

打开 VaultUploader，在界面上选择：
- **`.so (共享库)`** - 默认选项，生成 vault.so
- **`.a (静态库)`** - 生成 vault.a

### 2. 编译

1. 输入 32 字节密钥的 hex 编码（64 个字符）
2. 点击 **"① 编译 vault"** 按钮
3. 等待编译完成

编译成功后会显示：
- 文件格式（共享库或静态库）
- 文件大小
- 密封后的密文（可公开）

### 3. 上传到服务器

点击 **"② 上传"** 按钮，将编译好的库文件上传到 HTTPS 服务器。

## 两种格式的区别

### 共享库 (.so)

**优点**:
- 可以动态加载（dlopen）
- 不需要重新编译主程序
- 文件可以独立更新

**缺点**:
- 需要在运行时加载
- 依赖 libsodium.so

**使用示例**:
```cpp
#include <dlfcn.h>

// 加载库
void* handle = dlopen("vault.so", RTLD_NOW);
if (!handle) {
    fprintf(stderr, "dlopen failed: %s\n", dlerror());
    return -1;
}

// 获取函数指针
typedef int (*GetKeyFunc)(unsigned char*, unsigned long long*);
GetKeyFunc get_key = (GetKeyFunc)dlsym(handle, "get_key");

// 调用函数
unsigned char key[32];
unsigned long long key_len;
int ret = get_key(key, &key_len);

if (ret == 0) {
    printf("Key decrypted: %llu bytes\n", key_len);
    // 使用 key...
    memset(key, 0, sizeof(key)); // 清零
}

dlclose(handle);
```

### 静态库 (.a)

**优点**:
- 编译时链接，无需运行时加载
- 更快的启动速度
- 更好的安全性（代码内嵌）

**缺点**:
- 需要重新编译主程序才能更新
- 增加可执行文件大小

**使用示例**:
```cpp
// 直接声明函数（无需 dlopen）
extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

int main() {
    unsigned char key[32];
    unsigned long long key_len;
    
    // 直接调用
    int ret = get_key(key, &key_len);
    
    if (ret == 0) {
        printf("Key decrypted: %llu bytes\n", key_len);
        // 使用 key...
        memset(key, 0, sizeof(key)); // 清零
    }
    
    return 0;
}
```

**编译命令**:
```bash
# 链接静态库
g++ main.cpp vault.a -lsodium -o myapp

# 或者
g++ main.cpp -L. -lvault -lsodium -o myapp
```

## AR 文件格式

生成的 .a 文件使用标准的 Unix AR 格式：

```
文件结构:
┌─────────────────────┐
│ "!<arch>\n"         │  8 bytes - AR 魔数
├─────────────────────┤
│ 文件头 (60 bytes)   │  包含文件名、时间戳、权限等
├─────────────────────┤
│ key_vault.o         │  目标文件内容
│ (ELF 格式)          │
└─────────────────────┘
```

可以使用标准工具查看：
```bash
# 查看内容
ar -t vault.a

# 提取文件
ar -x vault.a

# 查看符号
nm vault.a
```

## 技术细节

### 编译流程

1. **生成密钥对**: 使用 libsodium 生成 X25519 密钥对
2. **密封密钥**: 使用 crypto_box_seal 加密输入密钥
3. **XOR 混淆**: 将 pk/sk/sealed 分块并 XOR 混淆
4. **生成 C 代码**: 构建包含混淆数据的 C 源码
5. **TCC 编译**: 
   - .so: 编译为 TCC_OUTPUT_DLL
   - .a: 编译为 TCC_OUTPUT_OBJ，然后打包为 AR 格式
6. **返回字节**: 将编译后的二进制返回给 Kotlin 层

### 安全特性

✅ **密钥密封**: 使用 libsodium 的 crypto_box_seal 加密
✅ **XOR 混淆**: pk/sk/sealed 分块存储并 XOR 加密
✅ **随机掩码**: 每次编译使用不同的 XOR 掩码
✅ **栈上解密**: 在栈上重组并解密，立即清零
✅ **无明文存储**: 二进制文件中不包含明文密钥

### 依赖关系

**共享库 (.so)**:
- 运行时依赖: libsodium.so
- 加载方式: dlopen

**静态库 (.a)**:
- 编译时依赖: libsodium.a 或 libsodium.so
- 链接方式: 静态链接

## 使用场景

### 推荐使用 .so 的场景

- 需要动态加载密钥库
- 密钥库需要独立更新
- 多个程序共享同一个密钥库
- 插件系统

### 推荐使用 .a 的场景

- 需要最佳性能
- 安全性要求高（避免 dlopen 被 hook）
- 单一可执行文件部署
- 嵌入式系统

## 示例项目

### C++ 项目使用静态库

```bash
# 1. 从 VaultUploader 编译并下载 vault.a
# 2. 创建项目
mkdir myproject && cd myproject

# 3. 创建 main.cpp
cat > main.cpp << 'EOF'
#include <stdio.h>
#include <string.h>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

int main() {
    unsigned char key[32];
    unsigned long long key_len;
    
    if (get_key(key, &key_len) == 0) {
        printf("Decrypted key (%llu bytes):\n", key_len);
        for (int i = 0; i < key_len; i++) {
            printf("%02x", key[i]);
        }
        printf("\n");
        memset(key, 0, sizeof(key));
    } else {
        printf("Failed to decrypt key\n");
    }
    
    return 0;
}
EOF

# 4. 编译
g++ main.cpp vault.a -lsodium -o myapp

# 5. 运行
./myapp
```

## 故障排查

### 问题: 链接时找不到 crypto_box_seal_open

**解决**: 确保链接了 libsodium
```bash
g++ ... -lsodium
```

### 问题: 运行时 Segmentation fault

**解决**: 检查 libsodium 是否正确安装
```bash
# Ubuntu
sudo apt-get install libsodium-dev

# macOS
brew install libsodium
```

### 问题: ar 命令无法识别 .a 文件

**解决**: 文件可能损坏，重新编译

## 性能对比

| 指标 | .so (共享库) | .a (静态库) |
|------|-------------|-------------|
| 加载时间 | ~1-5ms (dlopen) | 0ms (已链接) |
| 内存占用 | 共享 | 独立 |
| 文件大小 | 较小 | 较大 |
| 安全性 | 中等 | 较高 |
| 灵活性 | 高 | 低 |

## 下一步

- 查看完整 API 文档: [../TccCompiler/tcclib/README.md](../TccCompiler/tcclib/README.md)
- 了解服务器配置: [../https-server/README.md](../https-server/README.md)
- Ubuntu 版本: [../tcc-ubuntu/README.md](../tcc-ubuntu/README.md)
