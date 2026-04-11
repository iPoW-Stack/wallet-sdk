# TCC Compiler for Ubuntu x86_64

基于 TinyCC (TCC) 的 C 编译器库，支持在 Ubuntu x86_64 系统上动态编译 C 代码并生成静态库 (.a)、共享库 (.so) 等格式。

## 特性

- ✅ 编译 C 代码为目标文件 (.o)
- ✅ 编译为静态库 (.a)
- ✅ 编译为共享库 (.so)
- ✅ 编译为可执行文件
- ✅ 内存中编译和执行
- ✅ C 和 C++ API
- ✅ 零依赖（除了标准 C/C++ 库）

## 快速开始

### 1. 编译

```bash
cd tcc-ubuntu
make
```

这将生成：
- `libtcc_x86_64.a` - TCC 静态库
- `libtcc_x86_64.so` - TCC 共享库
- `libtcc_wrapper.a` - C++ 封装静态库
- `libtcc_wrapper.so` - C++ 封装共享库

### 2. 运行测试

**C API 测试**:
```bash
make test
./tcc_test
```

**C++ API 测试**:
```bash
make wrapper-test
./wrapper_test
```

### 3. 安装到系统

```bash
sudo make install
```

这将安装库文件到 `/usr/local/lib/` 和头文件到 `/usr/local/include/`。

## 使用示例

### C API

```c
#include "libtcc.h"

int main() {
    const char* source = 
        "int add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n";
    
    // 创建编译器
    TCCState* tcc = tcc_new();
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);
    
    // 编译
    tcc_compile_string(tcc, source);
    tcc_output_file(tcc, "output.o");
    
    // 清理
    tcc_delete(tcc);
    
    return 0;
}
```

编译：
```bash
gcc example.c -I../TccCompiler/tcclib/src/main/cpp/libtcc \
    libtcc_x86_64.a -o example -ldl -lpthread
```

### C++ API

```cpp
#include "tcc_wrapper.h"

int main() {
    std::string source = R"(
        int multiply(int a, int b) {
            return a * b;
        }
    )";
    
    tcc::TccCompiler compiler;
    
    // 编译为静态库
    bool success = compiler.compile_to_file(
        source,
        "libmath.a",
        tcc::OutputType::ARCHIVE
    );
    
    if (!success) {
        std::cerr << "Error: " << compiler.get_last_error() << "\n";
        return 1;
    }
    
    std::cout << "Compiled successfully!\n";
    return 0;
}
```

编译：
```bash
g++ example.cpp -I../TccCompiler/tcclib/src/main/cpp/libtcc \
    libtcc_wrapper.a libtcc_x86_64.a -o example -ldl -lpthread
```

## API 文档

### C++ API (tcc_wrapper.h)

#### OutputType 枚举

```cpp
enum class OutputType {
    OBJECT,      // .o 目标文件
    ARCHIVE,     // .a 静态库
    SHARED,      // .so 共享库
    EXECUTABLE,  // 可执行文件
    MEMORY       // 内存中执行
};
```

#### TccCompiler 类

**编译到内存**:
```cpp
CompileResult compile(
    const std::string& source_code,
    OutputType output_type = OutputType::OBJECT,
    const std::string& options = ""
);
```

**编译到文件**:
```cpp
bool compile_to_file(
    const std::string& source_code,
    const std::string& output_file,
    OutputType output_type = OutputType::OBJECT,
    const std::string& options = ""
);
```

**编译多个文件为静态库**:
```cpp
bool compile_archive(
    const std::vector<std::string>& source_files,
    const std::string& output_file
);
```

**获取错误信息**:
```cpp
std::string get_last_error() const;
```

### C API (libtcc.h)

详见 TCC 官方文档：https://bellard.org/tcc/tcc-doc.html

主要函数：
- `TCCState* tcc_new()` - 创建编译器实例
- `void tcc_delete(TCCState*)` - 销毁实例
- `void tcc_set_output_type(TCCState*, int)` - 设置输出类型
- `int tcc_compile_string(TCCState*, const char*)` - 编译源码字符串
- `int tcc_output_file(TCCState*, const char*)` - 输出到文件
- `void tcc_set_options(TCCState*, const char*)` - 设置编译选项

## 编译选项

常用选项：
- `-O2` - 优化级别 2
- `-Wall` - 显示所有警告
- `-shared -fPIC` - 生成共享库
- `-nostdlib` - 不链接标准库
- `-I<path>` - 添加头文件搜索路径
- `-L<path>` - 添加库文件搜索路径
- `-l<lib>` - 链接库

## 与 Android 版本的区别

| 特性 | Android 版本 | Ubuntu 版本 |
|------|-------------|-------------|
| 目标平台 | ARM64/x86_64 (Android) | x86_64 (Linux) |
| 构建系统 | CMake + Gradle | Makefile |
| JNI 绑定 | ✅ | ❌ |
| C++ API | ❌ | ✅ |
| 静态库支持 | ✅ | ✅ |
| 共享库支持 | ✅ | ✅ |

## 故障排查

### 编译错误

**问题**: `libtcc.c: No such file or directory`

**解决**: 确保 TCC 源码路径正确：
```bash
ls -la ../TccCompiler/tcclib/src/main/cpp/libtcc/libtcc.c
```

### 链接错误

**问题**: `undefined reference to tcc_new`

**解决**: 确保链接了 libtcc 库和必要的系统库：
```bash
gcc ... libtcc_x86_64.a -ldl -lpthread
```

### 运行时错误

**问题**: `error while loading shared libraries: libtcc_x86_64.so`

**解决**: 添加库路径到 LD_LIBRARY_PATH：
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)
```

或安装到系统：
```bash
sudo make install
```

## 性能对比

TCC 编译速度非常快，但生成的代码性能略低于 GCC/Clang：

| 编译器 | 编译时间 | 运行时性能 |
|--------|---------|-----------|
| TCC    | 极快 (~10x) | 中等 (~70%) |
| GCC -O0 | 快 | 中等 |
| GCC -O2 | 中等 | 快 |
| Clang -O2 | 中等 | 快 |

TCC 适用于：
- 动态代码生成
- JIT 编译
- 快速原型开发
- 插件系统
- 脚本化 C 代码

## 许可证

TCC 使用 LGPL 许可证。详见 TCC 官方仓库。

## 相关链接

- TCC 官网: https://bellard.org/tcc/
- TCC 源码: https://repo.or.cz/tinycc.git
- TCC 文档: https://bellard.org/tcc/tcc-doc.html
