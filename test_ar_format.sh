#!/bin/bash

# 测试 AR 格式生成和检测

echo "=== AR Format Test ==="
echo ""

# 1. 创建一个简单的 .o 文件
echo "1. Creating test object file..."
cat > test.c << 'EOF'
int test_func() {
    return 42;
}
EOF

gcc -c test.c -o test.o
if [ ! -f test.o ]; then
    echo "❌ Failed to create test.o"
    exit 1
fi
echo "✓ test.o created"

# 2. 使用 ar 创建标准静态库
echo ""
echo "2. Creating standard .a file with ar..."
ar rcs test_standard.a test.o
if [ ! -f test_standard.a ]; then
    echo "❌ Failed to create test_standard.a"
    exit 1
fi
echo "✓ test_standard.a created"

# 3. 检查魔数
echo ""
echo "3. Checking AR magic bytes..."
echo "Standard .a file:"
hexdump -C test_standard.a | head -n 5

# 4. 手动创建 AR 文件（模拟 Android 代码）
echo ""
echo "4. Creating manual .a file (simulating Android)..."
python3 << 'PYEOF'
import struct
import time

# 读取 .o 文件
with open('test.o', 'rb') as f:
    obj_data = f.read()

obj_size = len(obj_data)

# 创建 AR 文件
ar_data = bytearray()

# AR 魔数
ar_data.extend(b'!<arch>\n')

# 文件头 (60 bytes)
header = bytearray(b' ' * 60)

# 文件名 (16 bytes)
name = b'test.o/'
header[0:len(name)] = name

# 时间戳 (12 bytes)
timestamp = str(int(time.time())).encode('ascii')
header[16:16+len(timestamp)] = timestamp

# UID (6 bytes)
header[28:29] = b'0'

# GID (6 bytes)
header[34:35] = b'0'

# 模式 (8 bytes)
mode = b'100644'
header[40:40+len(mode)] = mode

# 大小 (10 bytes)
size_str = str(obj_size).encode('ascii')
header[48:48+len(size_str)] = size_str

# 结束标记
header[58:60] = b'`\n'

ar_data.extend(header)
ar_data.extend(obj_data)

# 奇数填充
if obj_size % 2 == 1:
    ar_data.append(ord('\n'))

# 保存
with open('test_manual.a', 'wb') as f:
    f.write(ar_data)

print(f"✓ test_manual.a created ({len(ar_data)} bytes)")
PYEOF

# 5. 比较两个文件
echo ""
echo "5. Comparing files..."
echo "Manual .a file:"
hexdump -C test_manual.a | head -n 5

# 6. 测试两个文件是否都能被 ar 识别
echo ""
echo "6. Testing with ar command..."
echo "Standard:"
ar -t test_standard.a
echo "Manual:"
ar -t test_manual.a

# 7. 测试上传到服务器
if [ -n "$1" ]; then
    SERVER_URL="$1"
    echo ""
    echo "7. Testing upload to $SERVER_URL..."
    
    echo "Uploading standard .a:"
    STD_B64=$(base64 < test_standard.a)
    curl -sk -X POST "$SERVER_URL/upload" \
      -H "Content-Type: application/json" \
      -d "{\"key\":\"\",\"so\":\"${STD_B64}\",\"name\":\"test_standard\"}"
    echo ""
    
    echo "Uploading manual .a:"
    MAN_B64=$(base64 < test_manual.a)
    curl -sk -X POST "$SERVER_URL/upload" \
      -H "Content-Type: application/json" \
      -d "{\"key\":\"\",\"so\":\"${MAN_B64}\",\"name\":\"test_manual\"}"
    echo ""
fi

# 清理
rm -f test.c test.o test_standard.a test_manual.a

echo ""
echo "=== Test Complete ==="
