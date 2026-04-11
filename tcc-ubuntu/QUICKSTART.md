# TCC Ubuntu 快速入门

## 一键构建

```bash
cd tcc-ubuntu
./build.sh
```

## 手动构建

### 1. 编译所有库

```bash
make
```

生成文件：
- `libtcc_x86_64.a` - TCC 核心静态库
- `libtcc_x86_64.so` - TCC 核心共享库
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

### 3. 编译示例程序

```bash
make example
```

使用示例：
```bash
# 创建测试源文件
cat > test.c << 'EOF'
int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}
EOF

# 编译为静态库
./tcc_example test.c libtest.a

# 查看内容
ar -t libtest.a
```

## 在你的项目中使用

### 方法 1: 直接链接静态库

```bash
# 编译你的程序
g++ your_program.cpp \
    -I/path/to/tcc-ubuntu/../TccCompiler/tcclib/src/main/cpp/libtcc \
    /path/to/tcc-ubuntu/libtcc_wrapper.a \
    /path/to/tcc-ubuntu/libtcc_x86_64.a \
    -o your_program \
    -ldl -lpthread
```

### 方法 2: 安装到系统

```bash
sudo make install
```

然后在你的项目中：
```bash
g++ your_program.cpp \
    -ltcc_wrapper -ltcc_x86_64 \
    -o your_program \
    -ldl -lpthread
```

## 代码示例

### 最简示例

```cpp
#include "tcc_wrapper.h"
#include <iostream>

int main() {
    std::string code = R"(
        int factorial(int n) {
            if (n <= 1) return 1;
            return n * factorial(n - 1);
        }
    )";
    
    tcc::TccCompiler compiler;
    
    // 编译为静态库
    if (compiler.compile_to_file(code, "libmath.a", tcc::OutputType::ARCHIVE)) {
        std::cout << "Success!\n";
    } else {
        std::cerr << "Error: " << compiler.get_last_error() << "\n";
    }
    
    return 0;
}
```

### 编译到内存

```cpp
#include "tcc_wrapper.h"
#include <iostream>

int main() {
    std::string code = "int add(int a, int b) { return a + b; }";
    
    tcc::TccCompiler compiler;
    auto result = compiler.compile(code, tcc::OutputType::OBJECT);
    
    if (result.success) {
        std::cout << "Compiled " << result.size << " bytes\n";
        // result.binary 包含编译后的二进制数据
    }
    
    return 0;
}
```

### 编译多个文件

```cpp
#include "tcc_wrapper.h"
#include <vector>
#include <string>

int main() {
    tcc::TccCompiler compiler;
    
    std::vector<std::string> sources = {
        "module1.c",
        "module2.c",
        "module3.c"
    };
    
    if (compiler.compile_archive(sources, "libmylib.a")) {
        std::cout << "Archive created!\n";
    }
    
    return 0;
}
```

## 常见问题

### Q: 编译时找不到 libtcc.h

A: 确保包含正确的头文件路径：
```bash
-I../TccCompiler/tcclib/src/main/cpp/libtcc
```

### Q: 链接时出现 undefined reference

A: 确保链接了所有必要的库：
```bash
libtcc_wrapper.a libtcc_x86_64.a -ldl -lpthread
```

注意顺序：wrapper 在前，tcc 在后。

### Q: 运行时找不到 .so 文件

A: 添加库路径：
```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/tcc-ubuntu
```

或使用静态库（.a）避免此问题。

### Q: 如何启用优化？

A: 在 compile 时传入选项：
```cpp
compiler.compile(code, OutputType::OBJECT, "-O2 -Wall");
```

## 性能提示

1. **使用静态库** - 避免运行时动态链接开销
2. **启用优化** - 使用 `-O2` 选项
3. **批量编译** - 使用 `compile_archive` 一次编译多个文件
4. **缓存结果** - 保存编译后的二进制，避免重复编译

## 下一步

- 阅读完整文档: [README.md](README.md)
- 查看测试代码: [wrapper_test.cpp](wrapper_test.cpp)
- 了解 TCC API: https://bellard.org/tcc/tcc-doc.html
