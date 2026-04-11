# 服务器端调试指南

## 1. 重新编译服务器

```bash
cd ~/wallet-sdk/https-server
make clean
make
```

## 2. 停止旧服务器

```bash
# 查找进程
ps aux | grep server

# 杀死进程
pkill -f "./server"

# 或使用 PID
kill -9 <PID>
```

## 3. 启动新服务器

```bash
# 前台运行（可以看到日志）
./server 38443

# 或后台运行
nohup ./server 38443 > server.log 2>&1 &

# 查看日志
tail -f server.log
```

## 4. 检查已上传的文件

### 方法 1: 使用 check_format 工具

```bash
# 编译工具
g++ -std=c++17 check_format.cpp -o check_format

# 检查文件
./check_format uploads/libs/mylib_20260411_104446_215.so

# 输出示例:
# Format: ELF
# Type: ET_DYN (Shared library / .so)
# Detected extension: .so
#
# 或
#
# Format: AR (Archive)
# Type: Static library
# Detected extension: .a
```

### 方法 2: 使用 check_file.sh 脚本

```bash
chmod +x check_file.sh
./check_file.sh uploads/libs/mylib_20260411_104446_215.so
```

### 方法 3: 使用 hexdump

```bash
# 查看文件头
hexdump -C uploads/libs/mylib_20260411_104446_215.so | head -n 5

# ELF 文件应该显示:
# 00000000  7f 45 4c 46 ...  |.ELF...|

# AR 文件应该显示:
# 00000000  21 3c 61 72 63 68 3e 0a  |!<arch>.|
```

### 方法 4: 使用 file 命令

```bash
file uploads/libs/mylib_20260411_104446_215.so

# ELF 共享库:
# ELF 64-bit LSB shared object, ...

# AR 静态库:
# current ar archive
```

## 5. 测试上传

### 测试标准 AR 文件

```bash
# 创建测试文件
echo "int test() { return 42; }" > test.c
gcc -c test.c -o test.o
ar rcs test.a test.o

# 检查格式
hexdump -C test.a | head -n 2
# 应该看到: 21 3c 61 72 63 68 3e 0a

# 上传
AR_B64=$(base64 -w 0 test.a)
curl -sk -X POST https://localhost:38443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test\"}"

# 检查响应
# 应该包含: "file_ext":".a"
```

### 测试 Android 上传的文件

从 Android 设备拉取文件：

```bash
# 在本地机器上
adb pull /data/data/com.example.vaultuploader/files/vault.a

# 上传到服务器
scp vault.a user@server:~/wallet-sdk/https-server/

# 在服务器上检查
./check_format vault.a
```

## 6. 查看服务器日志

上传时应该看到类似输出：

```
[DEBUG] File header (first 16 bytes): 21 3c 61 72 63 68 3e 0a ...
[DEBUG] As ASCII: !<arch>.
[DEBUG] Checking AR magic: data[0]=33 data[1]=60 data[2]=97 ...
[DEBUG] Detected AR format
[UPLOAD] key=0B  so=8192B  type=static library
[LIB] saved 8192 bytes -> ./uploads/libs/test_20260411_123456_789.a (static library)
```

如果看到的是：

```
[DEBUG] File header (first 16 bytes): 7f 45 4c 46 ...
[DEBUG] As ASCII: .ELF...
[DEBUG] ELF e_type = 3
[UPLOAD] key=0B  so=8192B  type=shared library
[LIB] saved 8192 bytes -> ./uploads/libs/test_20260411_123456_789.so (shared library)
```

说明上传的是 ELF 格式，不是 AR 格式。

## 7. 列出已上传的文件

```bash
# 使用 API
curl -sk https://localhost:38443/list | python3 -m json.tool

# 或直接查看目录
ls -lh uploads/libs/

# 查看文件类型
file uploads/libs/*
```

## 8. 清理测试文件

```bash
# 删除测试文件
rm -f test.c test.o test.a

# 清空上传目录
rm -f uploads/libs/*
```

## 9. 常见问题

### Q: 服务器显示 "Detected AR format" 但还是保存为 .so

**A**: 检查代码是否真的重新编译了

```bash
# 查看编译时间
ls -l server

# 应该是最近的时间
# 如果不是，重新编译
make clean && make
```

### Q: hexdump 显示 21 3c 61 72... 但服务器检测为 ELF

**A**: 可能是 base64 编码/解码问题

```bash
# 测试 base64 编解码
echo "!<arch>" | base64
echo "ITxhcmNoPgo=" | base64 -d | hexdump -C
```

### Q: 标准 AR 文件能正确识别，但 Android 的不行

**A**: Android 生成的格式有问题，检查 C++ 代码

```bash
# 在 Android 设备上
adb shell "cat /data/data/com.example.vaultuploader/files/vault.a" | hexdump -C | head -n 5

# 应该看到 AR 魔数
```

## 10. 完整测试流程

```bash
# 1. 重新编译
make clean && make

# 2. 重启服务器
pkill -f "./server"
./server 38443 &

# 3. 编译检查工具
g++ -std=c++17 check_format.cpp -o check_format

# 4. 创建并上传标准 AR 文件
echo "int test() { return 42; }" > test.c
gcc -c test.c && ar rcs test.a test.o
AR_B64=$(base64 -w 0 test.a)
curl -sk -X POST https://localhost:38443/upload \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test\"}"

# 5. 检查保存的文件
ls -l uploads/libs/test_*.a
./check_format uploads/libs/test_*.a

# 6. 清理
rm -f test.c test.o test.a
```

如果标准 AR 文件能正确保存为 `.a`，说明服务器端没问题。
如果还是保存为 `.so`，说明服务器代码未生效，需要重新编译。
