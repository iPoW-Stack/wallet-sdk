# C++ 静态库使用示例

这是一个完整的示例，展示如何在纯 C++ 项目中使用 VaultUploader 生成的 vault.a 静态库。

## 前置要求

### 1. 安装 libsodium

```bash
# Ubuntu/Debian
sudo apt-get install libsodium-dev

# macOS
brew install libsodium

# CentOS/RHEL
sudo yum install libsodium-devel
```

### 2. 获取 vault.a

使用以下任一方法获取 vault.a 文件：

#### 方法 1: 使用 VaultUploader App

1. 打开 VaultUploader
2. 选择 `.a (静态库)` 选项
3. 输入 32 字节密钥 (hex 格式，64 个字符)
4. 点击编译
5. 上传到服务器或通过 adb 拉取

#### 方法 2: 从 Android 设备拉取

```bash
adb pull /data/data/com.example.vaultuploader/files/vault.a .
```

#### 方法 3: 从服务器下载

```bash
# 列出文件
curl -sk https://your-server:38443/list

# 下载（需要直接访问服务器文件系统）
scp user@server:~/wallet-sdk/https-server/uploads/libs/vault_xxx.a vault.a
```

## 快速开始

### 1. 将 vault.a 放到此目录

```bash
cp /path/to/vault.a .
```

### 2. 编译

```bash
make
```

### 3. 运行

```bash
make run
```

或直接运行：

```bash
./example
```

## 预期输出

```
╔════════════════════════════════════════════╗
║  Vault Static Library Example             ║
╚════════════════════════════════════════════╝

Calling get_key()...
✓ Success!

Key length: 32 bytes
Key (hex): deadbeefcafebabe0102030405060708aabbccddeeff00112233445566778899

Key successfully decrypted from vault.a
You can now use this key for encryption/decryption.

✓ Key cleared from memory
```

## 文件说明

- `main.cpp` - 示例程序源码
- `Makefile` - 构建配置
- `README.md` - 本文档
- `vault.a` - 静态库（需要自行获取）

## 手动编译

如果不使用 Makefile：

```bash
g++ -std=c++17 -O2 main.cpp vault.a -lsodium -o example
```

## 故障排查

### 问题: vault.a not found

```
Error: vault.a not found!
```

**解决**: 将 vault.a 文件复制到此目录

### 问题: libsodium 未安装

```
fatal error: sodium.h: No such file or directory
```

**解决**: 安装 libsodium-dev

```bash
sudo apt-get install libsodium-dev
```

### 问题: 运行时找不到 libsodium

```
error while loading shared libraries: libsodium.so.23
```

**解决**: 添加库路径或使用静态链接

```bash
# 方法 1: 添加库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 方法 2: 静态链接
g++ main.cpp vault.a -static -lsodium -lpthread -o example
```

### 问题: get_key 返回错误

```
✗ Failed to get key!
Error code: -1
```

**可能原因**:
1. vault.a 损坏或格式错误
2. libsodium 版本不兼容
3. 内存不足

**解决**: 重新生成 vault.a

## 集成到你的项目

### 1. 复制文件

```bash
cp vault.a /path/to/your/project/
```

### 2. 修改你的 Makefile

```makefile
# 添加 vault.a 和 libsodium
LDFLAGS += -lsodium

your_app: your_sources.cpp vault.a
	g++ your_sources.cpp vault.a $(LDFLAGS) -o your_app
```

### 3. 在代码中使用

```cpp
extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

// 在你的代码中
unsigned char key[32];
unsigned long long key_len;

if (get_key(key, &key_len) == 0) {
    // 使用 key...
    
    // 清零
    memset(key, 0, sizeof(key));
}
```

## 安全建议

1. **立即清零**: 使用完密钥后立即清零
2. **避免日志**: 不要将密钥写入日志
3. **内存锁定**: 考虑使用 mlock() 防止交换到磁盘
4. **RAII**: 使用 RAII 模式自动管理密钥生命周期

## 更多示例

查看 [STATIC_LIBRARY_CPP_USAGE.md](../../STATIC_LIBRARY_CPP_USAGE.md) 获取更多高级用法。

## 相关链接

- [VaultUploader 文档](../../VaultUploader/STATIC_LIBRARY_GUIDE.md)
- [libsodium 文档](https://doc.libsodium.org/)
- [服务器部署](../../https-server/README.md)
