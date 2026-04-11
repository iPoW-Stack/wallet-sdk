#!/bin/bash

# 测试上传 AR 文件到服务器

SERVER_URL=${1:-"https://136.115.134.105:38443"}

echo "=== Testing AR Upload to $SERVER_URL ==="
echo ""

# 创建一个真实的 AR 文件
echo "1. Creating test AR file..."
cat > test.c << 'EOF'
int test_func() {
    return 42;
}
EOF

gcc -c test.c -o test.o
ar rcs test.a test.o

echo "✓ test.a created"
echo ""

# 检查文件头
echo "2. Checking AR file header..."
hexdump -C test.a | head -n 3
echo ""

# 上传到服务器
echo "3. Uploading to server..."
AR_B64=$(base64 < test.a)
RESPONSE=$(curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test_ar\"}")

echo "Response:"
echo "$RESPONSE" | python3 -m json.tool 2>/dev/null || echo "$RESPONSE"
echo ""

# 检查响应中的文件类型
if echo "$RESPONSE" | grep -q '"file_ext":".a"'; then
    echo "✓ Server correctly detected .a format"
else
    echo "✗ Server did NOT detect .a format"
    echo "Response details:"
    echo "$RESPONSE"
fi

# 清理
rm -f test.c test.o test.a

echo ""
echo "=== Test Complete ==="
