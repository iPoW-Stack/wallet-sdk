# 获取 libtcc 源码

libtcc 不能直接打包在此仓库中，需要手动获取。

## 方法一：从官方仓库克隆（推荐）

```bash
git clone https://repo.or.cz/tinycc.git /tmp/tinycc
cp /tmp/tinycc/libtcc.c  ./libtcc.c
cp /tmp/tinycc/libtcc.h  ./libtcc.h   # 覆盖占位文件
# 还需要 tcc 内部依赖的头文件：
cp /tmp/tinycc/tcc.h     ./tcc.h
cp /tmp/tinycc/tcctok.h  ./tcctok.h
cp /tmp/tinycc/tccgen.c  ./tccgen.c
cp /tmp/tinycc/tccelf.c  ./tccelf.c
cp /tmp/tinycc/tccasm.c  ./tccasm.c
cp /tmp/tinycc/tccpp.c   ./tccpp.c
cp /tmp/tinycc/tccrun.c  ./tccrun.c
cp /tmp/tinycc/tccmacho.c ./tccmacho.c
# ARM64 后端
cp /tmp/tinycc/arm64-gen.c  ./arm64-gen.c
cp /tmp/tinycc/arm64-link.c ./arm64-link.c
cp /tmp/tinycc/arm64-asm.c  ./arm64-asm.c
```

## 方法二：使用 mob-tcc（专为 Android 移植的分支）

```bash
git clone https://github.com/nicowillis/mob-tcc.git
```
此分支已针对 Android NDK 做了适配，推荐使用。

## CMakeLists.txt 中的编译定义说明

| 定义                    | 说明                          |
|-------------------------|-------------------------------|
| `TCC_TARGET_ARM64`      | arm64-v8a 目标架构            |
| `TCC_TARGET_X86_64`     | x86_64 目标架构               |
| `CONFIG_TCC_STATIC=1`   | 静态链接模式                  |
| `ONE_SOURCE=0`          | 多文件编译（非单文件模式）    |

## tcc_output_to_memory 说明

标准 TCC 没有 `tcc_output_to_memory`，有两种解决方案：

1. **patch 方案**：在 `tccelf.c` 中添加此函数，将 `tcc_output_file` 的文件写入重定向到内存缓冲区。
2. **临时文件方案**：用 `tcc_output_file` 写到 `/data/data/<pkg>/cache/tmp.o`，再读回 ByteArray。

临时文件方案更简单，见 `tcc_bridge.cpp` 中的备用实现注释。
