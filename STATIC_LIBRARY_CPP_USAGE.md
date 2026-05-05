# 静态库 (.a) 纯 C++ 使用指南

## 概述

本文档说明如何在纯 C++ 项目中使用 VaultUploader 生成的静态库 (vault.a) 来安全地获取加密密钥。

## 工作原理

1. **密钥生成**: 在 Android 端使用 VaultUploader 生成 vault.a
2. **密钥密封**: 使用 libsodium 的 crypto_box_seal 加密密钥
3. **XOR 混淆**: pk/sk/sealed 分块存储并 XOR 加密
4. **静态链接**: vault.a 可以直接链接到 C++ 项目
5. **运行时解密**: 调用 get_key() 函数获取解密后的密钥

## 前置要求

### 系统依赖

```bash
# Ubuntu/Debian
sudo apt-get install libsodium-dev g++ make

# macOS
brew install libsodium

# CentOS/RHEL
sudo yum install libsodium-devel gcc-c++ make
```

### 验证安装

```bash
# 检查 libsodium
pkg-config --modversion libsodium

# 检查头文件
ls /usr/include/sodium.h || ls /usr/local/include/sodium.h
```

## 快速开始

### 1. 获取 vault.a

从 VaultUploader 编译并下载 vault.a 文件：

1. 打开 VaultUploader App
2. 选择 `.a (静态库)` 选项
3. 输入 32 字节密钥 (hex 格式)
4. 点击编译
5. 上传到服务器或通过 adb 拉取

```bash
# 方法 1: 从服务器下载
curl -sk https://your-server:38443/list
# 找到文件路径，然后下载

# 方法 2: 从 Android 设备拉取
adb pull /data/data/com.example.vaultuploader/files/vault.a
```

### 2. 创建 C++ 项目

```bash
mkdir my_project
cd my_project
```

### 3. 创建主程序

创建 `main.cpp`:

```cpp
#include <iostream>
#include <cstring>
#include <iomanip>

// 声明 vault.a 中的函数
extern "C" {
    int get_key(unsigned char* out, unsigned long long* out_len);
}

int main() {
    // 准备接收密钥的缓冲区
    unsigned char key[32];
    unsigned long long key_len = 0;
    
    // 调用 vault.a 中的函数获取密钥
    int result = get_key(key, &key_len);
    
    if (result == 0) {
        std::cout << "✓ Key decrypted successfully!\n";
        std::cout << "Key length: " << key_len << " bytes\n";
        std::cout << "Key (hex): ";
        
        for (unsigned long long i = 0; i < key_len; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                      << (int)key[i];
        }
        std::cout << std::dec << "\n";
        
        // 使用密钥进行你的业务逻辑
        // ...
        
        // 使用完毕后立即清零
        std::memset(key, 0, sizeof(key));
        
        return 0;
    } else {
        std::cerr << "✗ Failed to decrypt key (error code: " << result << ")\n";
        return 1;
    }
}
```

### 4. 编译

```bash
# 基本编译
g++ main.cpp vault.a -lsodium -o myapp

# 或使用 pkg-config
g++ main.cpp vault.a $(pkg-config --cflags --libs libsodium) -o myapp

# 带优化
g++ -O2 main.cpp vault.a -lsodium -o myapp
```

### 5. 运行

```bash
./myapp
```

预期输出：
```
✓ Key decrypted successfully!
Key length: 32 bytes
Key (hex): deadbeefcafebabe0102030405060708aabbccddeeff00112233445566778899
```

## 完整示例

### 示例 1: 基础使用

```cpp
#include <iostream>
#include <cstring>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

int main() {
    unsigned char key[32];
    unsigned long long key_len;
    
    if (get_key(key, &key_len) == 0) {
        std::cout << "Got key: " << key_len << " bytes\n";
        
        // 使用密钥...
        
        // 清零
        std::memset(key, 0, sizeof(key));
    }
    
    return 0;
}
```

### 示例 2: 错误处理

```cpp
#include <iostream>
#include <cstring>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

class KeyManager {
public:
    KeyManager() : key_loaded_(false) {
        std::memset(key_, 0, sizeof(key_));
    }
    
    ~KeyManager() {
        // 析构时自动清零
        std::memset(key_, 0, sizeof(key_));
    }
    
    bool load() {
        unsigned long long len;
        int result = get_key(key_, &len);
        
        if (result == 0 && len == 32) {
            key_loaded_ = true;
            return true;
        }
        
        std::cerr << "Failed to load key: ";
        switch (result) {
            case -1:
                std::cerr << "Invalid parameters\n";
                break;
            case -2:
                std::cerr << "Decryption failed\n";
                break;
            default:
                std::cerr << "Unknown error (" << result << ")\n";
        }
        
        return false;
    }
    
    const unsigned char* get() const {
        return key_loaded_ ? key_ : nullptr;
    }
    
private:
    unsigned char key_[32];
    bool key_loaded_;
};

int main() {
    KeyManager km;
    
    if (!km.load()) {
        return 1;
    }
    
    const unsigned char* key = km.get();
    if (key) {
        std::cout << "Key loaded successfully\n";
        // 使用 key...
    }
    
    // km 析构时自动清零
    return 0;
}
```

### 示例 3: 与加密库集成

```cpp
#include <iostream>
#include <cstring>
#include <sodium.h>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

// 使用密钥加密数据
bool encrypt_data(const unsigned char* plaintext, size_t plaintext_len,
                  unsigned char* ciphertext, size_t* ciphertext_len) {
    // 获取密钥
    unsigned char key[32];
    unsigned long long key_len;
    
    if (get_key(key, &key_len) != 0 || key_len != 32) {
        return false;
    }
    
    // 生成随机 nonce
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    
    // 加密
    *ciphertext_len = crypto_secretbox_NONCEBYTES + 
                      crypto_secretbox_MACBYTES + plaintext_len;
    
    std::memcpy(ciphertext, nonce, crypto_secretbox_NONCEBYTES);
    
    crypto_secretbox_easy(
        ciphertext + crypto_secretbox_NONCEBYTES,
        plaintext, plaintext_len,
        nonce, key
    );
    
    // 清零密钥
    std::memset(key, 0, sizeof(key));
    sodium_memzero(nonce, sizeof(nonce));
    
    return true;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium\n";
        return 1;
    }
    
    const char* message = "Hello, World!";
    unsigned char ciphertext[1024];
    size_t ciphertext_len;
    
    if (encrypt_data((const unsigned char*)message, strlen(message),
                     ciphertext, &ciphertext_len)) {
        std::cout << "Encrypted " << strlen(message) 
                  << " bytes -> " << ciphertext_len << " bytes\n";
    } else {
        std::cerr << "Encryption failed\n";
        return 1;
    }
    
    return 0;
}
```

编译：
```bash
g++ example3.cpp vault.a -lsodium -o example3
```

## Makefile 示例

创建 `Makefile`:

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsodium

# 静态库路径
VAULT_LIB = vault.a

# 目标
TARGET = myapp

# 源文件
SOURCES = main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS) $(VAULT_LIB)
	$(CXX) $(OBJECTS) $(VAULT_LIB) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
```

使用：
```bash
make          # 编译
make run      # 运行
make clean    # 清理
```

## CMake 示例

创建 `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 libsodium
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

# 添加可执行文件
add_executable(myapp main.cpp)

# 链接静态库和 libsodium
target_link_libraries(myapp
    ${CMAKE_CURRENT_SOURCE_DIR}/vault.a
    ${SODIUM_LIBRARIES}
)

target_include_directories(myapp PRIVATE
    ${SODIUM_INCLUDE_DIRS}
)
```

使用：
```bash
mkdir build
cd build
cmake ..
make
./myapp
```

## API 参考

### get_key()

```cpp
extern "C" int get_key(unsigned char* out, unsigned long long* out_len);
```

**功能**: 从 vault.a 中解密并获取密钥

**参数**:
- `out`: 输出缓冲区，至少 32 字节
- `out_len`: 输出密钥长度（通常是 32）

**返回值**:
- `0`: 成功
- `-1`: 参数错误（out 或 out_len 为 NULL）
- 其他负数: 解密失败

**注意事项**:
1. 缓冲区必须至少 32 字节
2. 使用完毕后立即清零: `memset(key, 0, 32)`
3. 不要将密钥写入日志或文件
4. 避免在栈上长时间保留密钥

## 安全最佳实践

### 1. 立即清零

```cpp
unsigned char key[32];
unsigned long long key_len;

get_key(key, &key_len);

// 使用密钥...

// 立即清零
std::memset(key, 0, sizeof(key));
// 或使用 libsodium 的安全清零
sodium_memzero(key, sizeof(key));
```

### 2. 使用 RAII

```cpp
class SecureKey {
public:
    SecureKey() {
        std::memset(data_, 0, sizeof(data_));
        valid_ = (get_key(data_, &len_) == 0);
    }
    
    ~SecureKey() {
        sodium_memzero(data_, sizeof(data_));
    }
    
    // 禁止拷贝
    SecureKey(const SecureKey&) = delete;
    SecureKey& operator=(const SecureKey&) = delete;
    
    const unsigned char* data() const { return valid_ ? data_ : nullptr; }
    size_t size() const { return valid_ ? len_ : 0; }
    bool valid() const { return valid_; }
    
private:
    unsigned char data_[32];
    unsigned long long len_;
    bool valid_;
};
```

### 3. 避免内存泄漏

```cpp
// ❌ 错误：堆分配可能泄漏
unsigned char* key = new unsigned char[32];
get_key(key, &len);
// 如果这里抛出异常，key 不会被清零和释放

// ✅ 正确：栈分配 + RAII
{
    unsigned char key[32];
    unsigned long long len;
    get_key(key, &len);
    
    // 使用 key...
    
    std::memset(key, 0, sizeof(key));
} // 离开作用域自动清理
```

### 4. 防止调试器查看

```cpp
#include <sys/mman.h>

// 锁定内存页，防止交换到磁盘
void lock_memory(void* addr, size_t len) {
    mlock(addr, len);
}

void unlock_memory(void* addr, size_t len) {
    munlock(addr, len);
}

// 使用
unsigned char key[32];
lock_memory(key, sizeof(key));

get_key(key, &len);
// 使用 key...

std::memset(key, 0, sizeof(key));
unlock_memory(key, sizeof(key));
```

## 故障排查

### 问题 1: 链接错误

```
undefined reference to `get_key'
```

**解决**:
```bash
# 确保 vault.a 在命令行中
g++ main.cpp vault.a -lsodium -o myapp

# 检查 vault.a 中的符号
nm vault.a | grep get_key
```

### 问题 2: libsodium 未找到

```
fatal error: sodium.h: No such file or directory
```

**解决**:
```bash
# 安装 libsodium
sudo apt-get install libsodium-dev

# 或指定路径
g++ main.cpp vault.a -I/usr/local/include -L/usr/local/lib -lsodium -o myapp
```

### 问题 3: 运行时错误

```
error while loading shared libraries: libsodium.so.23
```

**解决**:
```bash
# 添加库路径
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 或使用静态链接
g++ main.cpp vault.a -static -lsodium -lpthread -o myapp
```

### 问题 4: get_key 返回 -1

**原因**: 参数为 NULL

**解决**:
```cpp
unsigned char key[32];
unsigned long long key_len;  // 必须初始化

// ❌ 错误
get_key(nullptr, &key_len);  // out 为 NULL
get_key(key, nullptr);       // out_len 为 NULL

// ✅ 正确
get_key(key, &key_len);
```

## 性能考虑

### 缓存密钥

如果需要频繁使用密钥，可以缓存：

```cpp
class KeyCache {
public:
    static KeyCache& instance() {
        static KeyCache cache;
        return cache;
    }
    
    const unsigned char* get() {
        if (!loaded_) {
            load();
        }
        return key_;
    }
    
    ~KeyCache() {
        sodium_memzero(key_, sizeof(key_));
    }
    
private:
    KeyCache() : loaded_(false) {
        std::memset(key_, 0, sizeof(key_));
    }
    
    void load() {
        unsigned long long len;
        if (get_key(key_, &len) == 0) {
            loaded_ = true;
        }
    }
    
    unsigned char key_[32];
    bool loaded_;
};

// 使用
const unsigned char* key = KeyCache::instance().get();
```

### 基准测试

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

unsigned char key[32];
unsigned long long len;
get_key(key, &len);

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

std::cout << "get_key() took " << duration.count() << " μs\n";
```

典型性能：< 100 μs

## 完整项目示例

查看 `examples/cpp_static_lib/` 目录获取完整的可运行示例项目。

## 相关文档

- [VaultUploader 使用指南](VaultUploader/STATIC_LIBRARY_GUIDE.md)
- [服务器部署文档](https-server/README.md)
- [Ubuntu 编译指南](tcc-ubuntu/README.md)
- [libsodium 文档](https://doc.libsodium.org/)

## 许可证

本项目使用的 libsodium 采用 ISC 许可证。
