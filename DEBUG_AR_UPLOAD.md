# 调试 AR 文件上传问题

## 问题描述

客户端显示编译成功静态库 (.a)，但服务器端保存为 .so 文件。

## 可能的原因

1. **服务器未重新编译** - 修改后的代码未生效
2. **Android 生成的不是真正的 AR 格式** - 格式有误
3. **format 参数未传递** - 已修复

## 调试步骤

### 1. 验证服务器端检测逻辑

```bash
# 编译测试程序
cd https-server
g++ -std=c++17 test_detection.cpp -o test_detection

# 运行测试
./test_detection

# 应该看到所有测试 PASS
```

### 2. 测试真实 AR 文件上传

```bash
# 运行测试脚本
./test_upload_ar.sh https://136.115.134.105:38443

# 检查输出
# 应该看到: ✓ Server correctly detected .a format
```

### 3. 重新编译服务器

```bash
# 在远程服务器上
cd ~/wallet-sdk/https-server
make clean
make

# 停止旧服务器
pkill -f "./server"

# 启动新服务器
./server 38443
```

### 4. 检查 Android 生成的文件

在 VaultUploader 中添加以下代码来保存文件到本地：

```kotlin
// 在编译成功后
if (result.success && result.soBytes != null) {
    // 保存到 Downloads 目录以便检查
    val downloads = Environment.getExternalStoragePublicDirectory(
        Environment.DIRECTORY_DOWNLOADS
    )
    val debugFile = File(downloads, "vault_debug$ext")
    debugFile.writeBytes(result.soBytes!!)
    Log.i(TAG, "Debug file saved: ${debugFile.absolutePath}")
}
```

然后：

```bash
# 从设备拉取文件
adb pull /sdcard/Download/vault_debug.a

# 检查文件头
hexdump -C vault_debug.a | head -n 5

# 应该看到: 21 3c 61 72 63 68 3e 0a (即 !<arch>\n)
```

### 5. 查看 Android 日志

```bash
# 查看编译日志
adb logcat | grep -E "KeySeal|MainActivity"

# 应该看到:
# KeySeal Archive: AR magic bytes: 21 3c 61 72 63 68 3e 0a
# MainActivity: First 16 bytes (hex): 21 3c 61 72 63 68 3e 0a ...
# MainActivity: First 16 bytes (ascii): !<arch>.
```

### 6. 查看服务器日志

在服务器端，上传时应该看到：

```
[DEBUG] File header (first 16 bytes): 21 3c 61 72 63 68 3e 0a ...
[DEBUG] As ASCII: !<arch>.
[DEBUG] Checking AR magic: data[0]=33 data[1]=60 ...
[DEBUG] Detected AR format
[UPLOAD] key=0B  so=8192B  type=static library
[LIB] saved 8192 bytes -> ./uploads/libs/test_timestamp.a (static library)
```

如果看到的是：

```
[DEBUG] File header (first 16 bytes): 7f 45 4c 46 ...
[DEBUG] As ASCII: .ELF...
[DEBUG] ELF e_type = 3
[UPLOAD] key=0B  so=8192B  type=shared library
[LIB] saved 8192 bytes -> ./uploads/libs/test_timestamp.so (shared library)
```

说明 Android 端生成的还是 ELF 格式，不是 AR 格式。

## 修复方案

### 如果是服务器未重新编译

```bash
# 远程服务器
cd ~/wallet-sdk/https-server
make clean
make
pkill -f "./server"
./server 38443
```

### 如果是 Android 生成格式错误

检查 `TccCompiler.kt` 中的调用：

```kotlin
// 确保传入了 format 参数
val result = compiler.compile(
    keyHex = keyHex,
    outputFile = outputFile,
    format = format  // ← 这个参数必须存在
)
```

检查 `KeySealCompiler.kt` 中的逻辑：

```kotlin
val soBytes = when (format) {
    OutputFormat.SHARED_LIBRARY -> TccCompiler.compileKeySealSo(...)
    OutputFormat.STATIC_LIBRARY -> TccCompiler.compileKeySealArchive(...)  // ← 确保调用这个
}
```

### 如果 C++ 层生成错误

检查 `tcc_bridge.cpp` 中的 `nativeCompileKeySealArchive` 函数：

```cpp
// 确保 AR 魔数正确
const char* AR_MAGIC = "!<arch>\n";
archiveBuf.insert(archiveBuf.end(), AR_MAGIC, AR_MAGIC + 8);

// 打印调试信息
LOGI("KeySeal Archive: AR magic bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
     (uint8_t)AR_MAGIC[0], (uint8_t)AR_MAGIC[1], ...);
```

## 快速验证

最快的验证方法：

```bash
# 1. 在本地创建标准 AR 文件
echo "int test() { return 42; }" > test.c
gcc -c test.c -o test.o
ar rcs test.a test.o

# 2. 上传到服务器
AR_B64=$(base64 < test.a)
curl -sk -X POST https://136.115.134.105:38443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test\"}"

# 3. 检查响应
# 应该包含: "file_ext":".a" 和 "lib_type":"static library"
```

如果标准 AR 文件能正确识别，说明服务器端没问题，问题在 Android 端。

## 检查清单

- [ ] 服务器已重新编译
- [ ] 服务器已重启
- [ ] Android 项目已重新编译
- [ ] App 已重新安装
- [ ] 选择了 `.a (静态库)` 选项
- [ ] 查看了 Android 日志中的文件头
- [ ] 查看了服务器日志中的检测结果
- [ ] 测试了标准 AR 文件上传

## 联系信息

如果以上步骤都无法解决问题，请提供：

1. Android logcat 输出（包含文件头信息）
2. 服务器日志输出（包含检测信息）
3. 从设备拉取的 vault_debug.a 文件的 hexdump 前 100 字节
