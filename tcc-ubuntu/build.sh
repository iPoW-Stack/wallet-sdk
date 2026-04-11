#!/bin/bash

# TCC Ubuntu 构建脚本

set -e

echo "╔════════════════════════════════════════════╗"
echo "║  TCC Compiler for Ubuntu x86_64 - Build   ║"
echo "╚════════════════════════════════════════════╝"
echo ""

# 检查 TCC 源码
TCC_SRC="../TccCompiler/tcclib/src/main/cpp/libtcc/libtcc.c"
if [ ! -f "$TCC_SRC" ]; then
    echo "❌ Error: TCC source not found at $TCC_SRC"
    echo ""
    echo "Please ensure the Android project is in the parent directory:"
    echo "  ../TccCompiler/tcclib/src/main/cpp/libtcc/"
    exit 1
fi
echo "✓ TCC source found"

# 清理旧文件
echo ""
echo "Cleaning old build files..."
make clean 2>/dev/null || true

# 编译库
echo ""
echo "Building libraries..."
make -j$(nproc)

echo ""
echo "═══════════════════════════════════════════"
echo "✓ Build complete!"
echo "═══════════════════════════════════════════"
echo ""
echo "Generated files:"
ls -lh libtcc_*.a libtcc_*.so 2>/dev/null || true
echo ""

# 询问是否运行测试
read -p "Run tests? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Building and running C API test..."
    make test
    echo ""
    ./tcc_test
    
    echo ""
    echo "Building and running C++ API test..."
    make wrapper-test
    echo ""
    ./wrapper_test
fi

echo ""
echo "═══════════════════════════════════════════"
echo "Next steps:"
echo "  1. Run tests:        make test && ./tcc_test"
echo "  2. Run wrapper test: make wrapper-test && ./wrapper_test"
echo "  3. Build example:    make example"
echo "  4. Install system:   sudo make install"
echo "═══════════════════════════════════════════"
