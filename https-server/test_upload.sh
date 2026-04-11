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

# 2. 测试上传（小的 ELF 头部）
echo "2. Upload Test (small ELF header):"
SO_B64=$(printf '\x7fELF\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00' | base64)
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"\",\"so\":\"${SO_B64}\",\"name\":\"test_lib\"}"
echo -e "\n"

# 3. 测试上传（带密钥）
echo "3. Upload Test (with key):"
KEY_B64=$(printf '\x01\x02\x03\x04\x05\x06\x07\x08' | base64)
SO_B64=$(printf '\x7fELF\x02\x01\x01\x00' | base64)
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d "{\"key\":\"${KEY_B64}\",\"so\":\"${SO_B64}\",\"name\":\"mylib\"}"
echo -e "\n"

# 4. 测试错误情况（缺少字段）
echo "4. Error Test (missing 'so' field):"
curl -sk -X POST "$SERVER_URL/upload" \
  -H "Content-Type: application/json" \
  -d '{"key":"test"}'
echo -e "\n"

# 5. 测试 404
echo "5. 404 Test:"
curl -sk "$SERVER_URL/notfound"
echo -e "\n"

echo "=== Test Complete ==="
