#!/bin/bash

# 服务器端一键测试脚本

set -e

echo "╔════════════════════════════════════════════╗"
echo "║  HTTPS Server Test Suite                  ║"
echo "╚════════════════════════════════════════════╝"
echo ""

SERVER_URL="https://localhost:38443"

# 1. 编译检查工具
echo "1. Compiling check tool..."
g++ -std=c++17 check_format.cpp -o check_format
echo "✓ check_format compiled"
echo ""

# 2. 创建测试文件
echo "2. Creating test files..."

# 创建 .o 文件
cat > test.c << 'EOF'
int test_func() {
    return 42;
}
EOF
gcc -c test.c -o test.o
echo "✓ test.o created"

# 创建 .a 文件
ar rcs test.a test.o
echo "✓ test.a created"

# 创建 .so 文件
gcc -shared -fPIC test.c -o test.so
echo "✓ test.so created"

echo ""

# 3. 检查文件格式
echo "3. Checking file formats..."
echo ""
echo "--- test.o ---"
./check_format test.o | grep -E "Format:|Type:|Detected"
echo ""
echo "--- test.a ---"
./check_format test.a | grep -E "Format:|Type:|Detected"
echo ""
echo "--- test.so ---"
./check_format test.so | grep -E "Format:|Type:|Detected"
echo ""

# 4. 测试上传
echo "4. Testing uploads to $SERVER_URL..."
echo ""

# 测试 .o 上传
echo "Uploading test.o..."
OBJ_B64=$(base64 -w 0 test.o)
RESP_O=$(curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${OBJ_B64}\",\"name\":\"test_obj\"}")
echo "$RESP_O" | python3 -m json.tool 2>/dev/null || echo "$RESP_O"

if echo "$RESP_O" | grep -q '"file_ext":".o"'; then
    echo "✓ .o file correctly detected"
else
    echo "✗ .o file NOT correctly detected"
fi
echo ""

# 测试 .a 上传
echo "Uploading test.a..."
AR_B64=$(base64 -w 0 test.a)
RESP_A=$(curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test_static\"}")
echo "$RESP_A" | python3 -m json.tool 2>/dev/null || echo "$RESP_A"

if echo "$RESP_A" | grep -q '"file_ext":".a"'; then
    echo "✓ .a file correctly detected"
else
    echo "✗ .a file NOT correctly detected"
fi
echo ""

# 测试 .so 上传
echo "Uploading test.so..."
SO_B64=$(base64 -w 0 test.so)
RESP_SO=$(curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${SO_B64}\",\"name\":\"test_shared\"}")
echo "$RESP_SO" | python3 -m json.tool 2>/dev/null || echo "$RESP_SO"

if echo "$RESP_SO" | grep -q '"file_ext":".so"'; then
    echo "✓ .so file correctly detected"
else
    echo "✗ .so file NOT correctly detected"
fi
echo ""

# 5. 列出上传的文件
echo "5. Listing uploaded files..."
curl -sk "$SERVER_URL/list" | python3 -m json.tool 2>/dev/null || curl -sk "$SERVER_URL/list"
echo ""

# 6. 验证保存的文件
echo "6. Verifying saved files..."
echo ""

for file in uploads/libs/test_*; do
    if [ -f "$file" ]; then
        echo "Checking: $file"
        ./check_format "$file" | grep -E "Format:|Type:|Detected"
        echo ""
    fi
done

# 清理
echo "7. Cleaning up..."
rm -f test.c test.o test.a test.so
echo "✓ Test files removed"
echo ""

echo "╔════════════════════════════════════════════╗"
echo "║  Test Complete                             ║"
echo "╚════════════════════════════════════════════╝"
echo ""
echo "Summary:"
echo "  - If all formats are correctly detected, server is working properly"
echo "  - If .a files are saved as .so, server needs recompilation"
echo "  - Check server logs for detailed debug output"
