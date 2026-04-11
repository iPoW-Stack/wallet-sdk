#!/bin/bash

# HTTPS Server 启动脚本
# 用法: ./start.sh [port]
# 默认端口: 8443

PORT=${1:-8443}

echo "=== HTTPS Server Startup Check ==="
echo ""

# 1. 检查可执行文件
if [ ! -f "./server" ]; then
    echo "❌ Error: ./server not found"
    echo "   Run: make"
    exit 1
fi
echo "✓ Server binary exists"

# 2. 检查证书文件
if [ ! -f "certs/key.pem" ] || [ ! -f "certs/cert.pem" ]; then
    echo "❌ Error: SSL certificates not found"
    echo "   Generating self-signed certificates..."
    mkdir -p certs
    openssl req -x509 -newkey rsa:4096 -nodes \
        -keyout certs/key.pem \
        -out certs/cert.pem \
        -days 365 \
        -subj "/CN=localhost"
    if [ $? -eq 0 ]; then
        echo "✓ Certificates generated"
    else
        echo "❌ Failed to generate certificates"
        exit 1
    fi
else
    echo "✓ SSL certificates exist"
fi

# 3. 检查端口占用
if command -v lsof &> /dev/null; then
    if lsof -i :$PORT &> /dev/null; then
        echo "❌ Error: Port $PORT is already in use"
        echo "   Processes using port $PORT:"
        lsof -i :$PORT
        echo ""
        echo "   Kill with: kill -9 <PID>"
        exit 1
    fi
    echo "✓ Port $PORT is available"
elif command -v netstat &> /dev/null; then
    if netstat -tuln 2>/dev/null | grep -q ":$PORT "; then
        echo "⚠️  Warning: Port $PORT may be in use"
        netstat -tuln | grep ":$PORT "
    else
        echo "✓ Port $PORT appears available"
    fi
fi

# 4. 创建上传目录
mkdir -p uploads/libs uploads/keys
echo "✓ Upload directories ready"

echo ""
echo "=== Starting Server on Port $PORT ==="
echo ""

# 启动服务器
./server $PORT
