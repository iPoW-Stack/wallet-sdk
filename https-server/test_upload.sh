#!/bin/bash

# HTTPS Server Upload 接口测试脚本
# 用法: ./test_upload.sh [server_url]

SERVER_URL=${1:-"https://136.115.134.105:38443"}

echo "=== Testing HTTPS Server: $SERVER_URL ==="
echo ""

# 1. 健康检查
echo "1. Health Check:"
curl -sk "$SERVER_URL/health"
echo -e "\n"

# 2. 测试上传共享库（小的 ELF 头部）
echo "2. Upload Test (shared library .so):"
# ELF 头部，ET_DYN (共享库)
SO_B64=$(printf '\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x3e\x00' | base64)
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${SO_B64}\",\"name\":\"test_shared\"}"
echo -e "\n"

# 3. 测试上传静态库（AR 格式）
echo "3. Upload Test (static library .a):"
# AR 魔数 + 简单头部
AR_B64=$(printf '!<arch>\ntest.o/         0           0     0     100644  0         `\n' | base64)
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${AR_B64}\",\"name\":\"test_static\"}"
echo -e "\n"

# 4. 测试上传目标文件（.o）
echo "4. Upload Test (object file .o):"
# ELF 头部，ET_REL (可重定位)
OBJ_B64=$(printf '\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x3e\x00' | base64)
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${OBJ_B64}\",\"name\":\"test_object\"}"
echo -e "\n"

# 5. 列出已上传的文件
echo "5. List Uploaded Files:"
curl -sk "$SERVER_URL/list" | python3 -m json.tool 2>/dev/null || curl -sk "$SERVER_URL/list"
echo -e "\n"

# 6. 测试错误情况（缺少字段）
echo "6. Error Test (missing 'so' field):"
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d '{"key":"test"}'
echo -e "\n"

# 7. 测试 404
echo "7. 404 Test:"
curl -sk "$SERVER_URL/notfound"
echo -e "\n"

echo "=== Test Complete ==="
