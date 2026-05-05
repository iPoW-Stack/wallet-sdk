# vault.so 逆向工程演示

**⚠️ 警告**: 本文档仅用于教育目的，帮助理解安全机制的局限性。

## 目的

展示 vault.so 可以被逆向工程，但**仍然无法直接获取原始私钥**。

## 工具准备

```bash
# 安装工具
sudo apt-get install binutils gdb hexdump

# 可选：高级工具
# - IDA Pro / Ghidra (反汇编)
# - radare2 (逆向框架)
```

## 步骤 1: 静态分析

### 1.1 查看符号表

```bash
nm vault.so

# 输出:
# 0000000000001234 T get_key
# 0000000000002000 d a0
# 0000000000002008 d m0
# 0000000000002010 d a1
# ...
```

**发现**: 可以看到 `get_key` 函数和数据符号

### 1.2 查看字符串

```bash
strings vault.so

# 输出:
# (没有明文密钥)
```

**结果**: ❌ 无法直接提取密钥

### 1.3 十六进制转储

```bash
hexdump -C vault.so | head -n 100

# 输出:
# 00001000  12 34 56 78 9a bc de f0  ...
# (混淆后的数据)
```

**结果**: ❌ 看到的是 XOR 后的数据

### 1.4 反汇编

```bash
objdump -d vault.so > vault.asm

# 查看 get_key 函数
grep -A 50 "<get_key>:" vault.asm
```

**输出示例**:
```asm
0000000000001234 <get_key>:
    1234:   push   rbp
    1235:   mov    rbp,rsp
    1238:   sub    rsp,0x60
    
    ; 检查参数
    123c:   cmp    QWORD PTR [rbp+0x10],0x0
    1241:   je     1300 <get_key+0xcc>
    
    ; XOR 操作 (还原 pk)
    1246:   mov    rdi,0x0
    124d:   movzx  eax,BYTE PTR [rip+0xe00]  ; a0[i]
    1254:   movzx  edx,BYTE PTR [rip+0xe08]  ; m0[i]
    125b:   xor    eax,edx
    125d:   mov    BYTE PTR [rbp-0x20+rdi],al
    
    ; 循环...
    
    ; 调用 crypto_box_seal_open
    1280:   lea    rdi,[rbp-0x20]  ; pk
    1284:   lea    rsi,[rbp-0x40]  ; sk
    1288:   call   crypto_box_seal_open@PLT
```

**发现**: 
- ✅ 可以看到 XOR 操作
- ✅ 可以看到数据地址
- ✅ 可以看到算法流程
- ❌ 但仍需要运行时才能得到真实数据

## 步骤 2: 提取混淆数据

### 2.1 定位数据段

```bash
readelf -S vault.so | grep -E "\.data|\.rodata"

# 输出:
# [15] .rodata  PROGBITS  0000000000002000  00002000  000001a0  00  A  0  0  8
# [24] .data    PROGBITS  0000000000004000  00003000  00000100  00  WA 0  0  8
```

### 2.2 提取数据

```bash
# 提取 .rodata 段
objcopy --dump-section .rodata=rodata.bin vault.so

# 查看内容
hexdump -C rodata.bin | head -n 20
```

**输出**:
```
00000000  a3 7f 2e 91 5c 44 e8 b2  |.....\D..|  <- a0 (XOR 后)
00000008  6b 9d 3a f7 1c 88 d4 5e  |k.:....^|  <- m0 (掩码)
00000010  f1 23 c7 89 4d 6a b5 e0  |.#..Mj..|  <- a1 (XOR 后)
...
```

### 2.3 手动还原

```python
#!/usr/bin/env python3

# 从 hexdump 提取的数据
a0 = bytes.fromhex("a3 7f 2e 91 5c 44 e8 b2")
m0 = bytes.fromhex("6b 9d 3a f7 1c 88 d4 5e")

# XOR 还原
pk_part0 = bytes([a ^ m for a, m in zip(a0, m0)])
print("pk[0:8] =", pk_part0.hex())

# 输出: pk[0:8] = c8e214665ccc3cec
```

**结果**: ✅ 可以还原出 pk/sk/sealed

**但是**: ❌ 这些数据仍然是加密的！

## 步骤 3: 动态调试

### 3.1 创建测试程序

```cpp
// test.cpp
#include <stdio.h>

extern "C" int get_key(unsigned char* out, unsigned long long* out_len);

int main() {
    unsigned char key[32];
    unsigned long long len;
    
    int result = get_key(key, &len);
    
    printf("Result: %d\n", result);
    printf("Length: %llu\n", len);
    
    if (result == 0) {
        printf("Key: ");
        for (int i = 0; i < len; i++) {
            printf("%02x", key[i]);
        }
        printf("\n");
    }
    
    return 0;
}
```

编译:
```bash
g++ test.cpp vault.so -lsodium -o test
```

### 3.2 使用 GDB 调试

```bash
gdb ./test

# 在 get_key 设置断点
(gdb) break get_key
(gdb) run

# 运行到断点
Breakpoint 1, get_key (out=0x7fffffffe000, out_len=0x7fffffffe008)

# 单步执行到 XOR 还原后
(gdb) next
...

# 查看 pk (栈上，rbp-0x20)
(gdb) x/32xb $rbp-0x20
0x7fffffffdfe0: 0xc8 0xe2 0x14 0x66 0x5c 0xcc 0x3c 0xec
0x7fffffffdfe8: 0x9a 0x2f 0x67 0x0f 0x30 0x14 0x1c 0xec
...

# 查看 sk
(gdb) x/32xb $rbp-0x40
0x7fffffffdfc0: 0x12 0x34 0x56 0x78 0x9a 0xbc 0xde 0xf0
...

# 查看 sealed
(gdb) x/48xb $rbp-0x60
0x7fffffffdfa0: 0xaa 0xbb 0xcc 0xdd ...
```

**结果**: ✅ 可以提取运行时的 pk/sk/sealed

### 3.3 手动解密

现在有了 pk/sk/sealed，可以手动解密：

```python
#!/usr/bin/env python3
from nacl.public import PrivateKey, SealedBox

# 从 GDB 提取的数据
pk_bytes = bytes.fromhex("c8e214665ccc3cec...")  # 32 bytes
sk_bytes = bytes.fromhex("123456789abcdef0...")  # 32 bytes
sealed = bytes.fromhex("aabbccdd...")            # 48 bytes

# 创建密钥对
private_key = PrivateKey(sk_bytes)
public_key = private_key.public_key

# 解密
sealed_box = SealedBox(private_key)
original_key = sealed_box.decrypt(sealed)

print("Original key:", original_key.hex())
```

**结果**: ✅ 成功提取原始私钥！

## 步骤 4: 自动化提取

### 4.1 使用 GDB 脚本

```python
# extract_key.py
import gdb

class ExtractKey(gdb.Command):
    def __init__(self):
        super(ExtractKey, self).__init__("extract-key", gdb.COMMAND_USER)
    
    def invoke(self, arg, from_tty):
        # 在 get_key 设置断点
        gdb.execute("break get_key")
        gdb.execute("run")
        
        # 单步到 crypto_box_seal_open 之前
        for _ in range(100):
            gdb.execute("next")
        
        # 读取 pk/sk/sealed
        rbp = int(gdb.parse_and_eval("$rbp"))
        
        pk = gdb.selected_inferior().read_memory(rbp - 0x20, 32)
        sk = gdb.selected_inferior().read_memory(rbp - 0x40, 32)
        sealed = gdb.selected_inferior().read_memory(rbp - 0x60, 48)
        
        print("PK:", pk.hex())
        print("SK:", sk.hex())
        print("Sealed:", sealed.hex())

ExtractKey()
```

使用:
```bash
gdb -x extract_key.py ./test
```

### 4.2 使用 Frida (动态插桩)

```javascript
// hook.js
Interceptor.attach(Module.findExportByName(null, "get_key"), {
    onEnter: function(args) {
        console.log("get_key called");
        this.out = args[0];
        this.out_len = args[1];
    },
    onLeave: function(retval) {
        if (retval.toInt32() === 0) {
            var len = this.out_len.readU64();
            var key = this.out.readByteArray(len);
            console.log("Key extracted:", hexdump(key));
        }
    }
});
```

使用:
```bash
frida -l hook.js ./test
```

## 防御措施效果

| 防御措施 | 对静态分析 | 对动态调试 | 实施难度 |
|---------|-----------|-----------|---------|
| XOR 混淆 | ✅ 有效 | ❌ 无效 | 简单 |
| 分块存储 | ✅ 有效 | ❌ 无效 | 简单 |
| 随机掩码 | ✅ 有效 | ❌ 无效 | 简单 |
| 栈上清零 | ⚠️ 部分 | ⚠️ 部分 | 简单 |
| 反调试 | ❌ 无效 | ⚠️ 部分 | 中等 |
| 代码混淆 | ⚠️ 部分 | ⚠️ 部分 | 困难 |
| 完整性检查 | ❌ 无效 | ⚠️ 部分 | 中等 |
| 硬件安全 | ✅ 有效 | ✅ 有效 | 困难 |

## 关键发现

### 可以提取的内容

1. ✅ **算法逻辑**: 通过反汇编
2. ✅ **混淆数据**: 通过静态分析
3. ✅ **运行时数据**: 通过动态调试
4. ✅ **原始私钥**: 通过调试 + 手动解密

### 无法提取的内容

1. ❌ **直接的明文密钥**: 需要多步骤
2. ❌ **批量自动化**: 每个 .so 都不同
3. ❌ **远程提取**: 需要本地访问

## 实际攻击成本

### 时间成本

- 静态分析: 1-2 小时
- 动态调试: 30 分钟
- 编写脚本: 1-2 小时
- **总计**: 3-5 小时（有经验的逆向工程师）

### 技能要求

- ✅ 熟悉 GDB/LLDB
- ✅ 理解 ELF 格式
- ✅ 理解汇编语言
- ✅ 理解加密算法
- ⚠️ 中高级逆向技能

### 工具要求

- ✅ 免费工具即可（GDB, objdump）
- ⚠️ 高级工具更方便（IDA Pro, Ghidra）

## 结论

### vault.so 的安全性

**防护等级**: ⚠️ **中等**

**可以防御**:
- ✅ 意外泄露
- ✅ 自动化扫描
- ✅ 非技术人员
- ✅ 脚本小子

**无法防御**:
- ❌ 专业逆向工程师
- ❌ 有物理访问权限的攻击者
- ❌ 有足够时间和资源的攻击者

### 适用场景

**✅ 适合**:
- 防止配置文件泄露
- 防止代码仓库泄露
- 提高攻击门槛
- 一般应用场景

**❌ 不适合**:
- 高价值目标
- 需要合规认证
- 防御专业攻击
- 关键基础设施

### 最终建议

```
如果你的数据价值 < 攻击成本:
  ✅ vault.so 足够

如果你的数据价值 > 攻击成本:
  ❌ 需要硬件安全方案
     (Keystore, HSM, TPM, KMS)
```

## 参考资料

- [Reverse Engineering for Beginners](https://beginners.re/)
- [GDB Tutorial](https://www.gnu.org/software/gdb/documentation/)
- [Frida Documentation](https://frida.re/docs/home/)
- [IDA Pro](https://www.hex-rays.com/products/ida/)
- [Ghidra](https://ghidra-sre.org/)
