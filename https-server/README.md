# HTTPS Server for SO Upload

基于 uWebSockets 的 HTTPS 服务器，用于接收和存储 .so 二进制文件。

## 快速启动

### 1. 编译服务器

```bash
cd https-server
make clean
make
```

**注意**: 如果修改了 `server.cpp`，必须重新编译才能生效。

### 2. 启动服务器

```bash
# 使用默认端口 8443
./start.sh

# 或指定其他端口
./start.sh 9443
```

启动脚本会自动：
- 检查可执行文件是否存在
- 生成自签名 SSL 证书（如果不存在）
- 检查端口是否被占用
- 创建上传目录

### 3. 手动启动（不使用脚本）

```bash
# 生成证书（首次运行）
mkdir -p certs
openssl req -x509 -newkey rsa:4096 -nodes \
    -keyout certs/key.pem \
    -out certs/cert.pem \
    -days 365 \
    -subj "/CN=localhost"

# 启动服务器
./server          # 默认端口 8443
./server 9443     # 指定端口 9443
```

## 故障排查

### 端口 8443 监听失败

**问题**: `Failed to listen on port 8443`

**可能原因**:

1. **端口已被占用**
   ```bash
   # Linux
   netstat -tuln | grep 8443
   lsof -i :8443
   
   # 杀死占用进程
   kill -9 <PID>
   ```

2. **权限不足**（端口 < 1024 需要 root）
   ```bash
   # 使用更高端口
   ./server 8443
   
   # 或使用 sudo（不推荐）
   sudo ./server 443
   ```

3. **SSL 证书文件缺失或损坏**
   ```bash
   # 重新生成证书
   rm -rf certs
   mkdir -p certs
   openssl req -x509 -newkey rsa:4096 -nodes \
       -keyout certs/key.pem \
       -out certs/cert.pem \
       -days 365 \
       -subj "/CN=localhost"
   ```

4. **防火墙阻止**
   ```bash
   # Ubuntu/Debian
   sudo ufw allow 8443/tcp
   
   # CentOS/RHEL
   sudo firewall-cmd --add-port=8443/tcp --permanent
   sudo firewall-cmd --reload
   ```

### 更换端口

如果 8443 端口不可用，使用其他端口：

```bash
./server 9443
```

或修改 VaultUploader 客户端代码中的端口号。

## API 接口

### POST /upload

上传 .so 文件和密钥

**请求**:
```bash
curl -sk -X POST https://localhost:8443/upload \
  -H "Content-Type: application/json" \
  -d '{
    "key": "",
    "so": "<base64_encoded_so_binary>",
    "name": "mylib"
  }'
```

**字段说明**:
- `key`: 密钥的 base64 编码（可以为空字符串 `""`）
- `so`: .so 文件的 base64 编码（必填）
- `name`: 文件名前缀（可选）

**响应**:
```json
{
  "ok": true,
  "msg": "ok",
  "key_size": 0,
  "so_file": "./uploads/libs/mylib_20260411_113045_123.so",
  "so_size": 8192
}
```

### 测试脚本

使用提供的测试脚本验证服务器：

```bash
# 测试本地服务器
./test_upload.sh https://localhost:8443

# 测试远程服务器
./test_upload.sh https://136.115.134.105:38443
```

### GET /health

健康检查

```bash
curl -sk https://localhost:8443/health
```

**响应**:
```json
{
  "ok": true,
  "msg": "server is running"
}
```

## 目录结构

```
https-server/
├── server.cpp              # 服务器源码
├── server                  # 编译后的可执行文件
├── Makefile               # 编译配置
├── start.sh               # 启动脚本
├── certs/                 # SSL 证书
│   ├── cert.pem          # 公钥证书
│   └── key.pem           # 私钥
├── uploads/              # 上传文件存储
│   ├── libs/            # .so 文件
│   └── keys/            # 密钥文件（预留）
├── uWebSockets-20.62.0/  # uWebSockets 库
└── uSockets-0.8.8/       # uSockets 库
```

## 安全注意事项

1. **自签名证书**: 默认生成的是自签名证书，客户端需要使用 `-k` 参数跳过证书验证
2. **生产环境**: 使用 Let's Encrypt 或其他 CA 签发的证书
3. **文件大小限制**: 当前限制为 1MB，可在 `server.cpp` 中修改 `MAX_BODY`
4. **访问控制**: 当前无认证机制，建议添加 API key 或 JWT 认证
