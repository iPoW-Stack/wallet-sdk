# vault.so/vault.a 安全性分析

## 核心问题

**vault.so 暴露会被解析出私钥吗？**

**简短回答**: 不会直接暴露明文私钥，但**不能完全防止逆向工程**。

## 安全机制

### 1. 密钥密封 (Sealed Box)

```
原始私钥 (32 bytes)
    ↓
crypto_box_seal(私钥, 公钥)  ← libsodium 加密
    ↓
密封密文 (48 bytes)
```

- 使用 libsodium 的 `crypto_box_seal` 加密
- 基于 X25519 + XSalsa20-Poly1305
- 只有对应的私钥才能解密

### 2. XOR 混淆

```c
// pk/sk/sealed 分块存储并 XOR 加密
static const unsigned char a0[8] = {0x12^0xAB, 0x34^0xCD, ...};  // pk 片段 1
static const unsigned char m0[8] = {0xAB, 0xCD, ...};            // XOR 掩码 1
static const unsigned char a1[8] = {0x56^0xEF, 0x78^0x12, ...};  // pk 片段 2
static const unsigned char m1[8] = {0xEF, 0x12, ...};            // XOR 掩码 2
// ... 更多片段

// 运行时重组
for (i=0; i<8; i++) pk[0+i] = a0[i] ^ m0[i];  // 还原 pk
for (i=0; i<8; i++) pk[8+i] = a1[i] ^ m1[i];
```

**目的**: 防止直接用 `strings` 或 `hexdump` 提取

### 3. 随机掩码

每次编译使用不同的 XOR 掩码（从 `/dev/urandom` 读取）：

```cpp
FILE* urandom = fopen("/dev/urandom", "rb");
fread(mask_pk, 1, 32, urandom);
fread(mask_sk, 1, 32, urandom);
fread(mask_seal, 1, 48, urandom);
```

**结果**: 相同密钥每次编译生成的 .so 二进制都不同

### 4. 栈上解密

```c
int get_key(unsigned char* out, unsigned long long* out_len) {
    unsigned char pk[32], sk[32], sealed[48];  // 栈上分配
    
    // 重组 pk/sk/sealed
    for (i=0; i<8; i++) pk[0+i] = a0[i] ^ m0[i];
    // ...
    
    // 解密
    int r = crypto_box_seal_open(out, sealed, 48, pk, sk);
    
    // 立即清零
    for (i=0; i<32; i++) { pk[i]=0; sk[i]=0; }
    for (i=0; i<48; i++) sealed[i]=0;
    
    return r;
}
```

**优点**: 
- 不在堆上分配
- 使用后立即清零
- 减少内存泄漏风险

## 攻击场景分析

### ❌ 场景 1: 静态分析 (strings/hexdump)

**攻击**: 使用 `strings` 或 `hexdump` 查看二进制

```bash
strings vault.so
hexdump -C vault.so
```

**结果**: ❌ **无法直接提取**

**原因**: 
- pk/sk/sealed 都经过 XOR 混淆
- 分块存储，无连续的密钥数据
- 变量名混淆（a0, m0 等无意义名称）

**示例**:
```bash
$ strings vault.so | grep -i key
# 找不到明文密钥
```

### ⚠️ 场景 2: 反汇编分析

**攻击**: 使用 IDA Pro / Ghidra 反汇编

```bash
objdump -d vault.so
```

**结果**: ⚠️ **可以看到算法逻辑**

**攻击者可以看到**:
```asm
; 可以看到 XOR 操作
mov al, byte ptr [a0+rdi]
xor al, byte ptr [m0+rdi]
mov byte ptr [rbp-0x20+rdi], al

; 可以看到 crypto_box_seal_open 调用
call crypto_box_seal_open
```

**攻击者可以做什么**:
1. 识别出 XOR 混淆模式
2. 提取混淆后的数据和掩码
3. 手动还原 pk/sk/sealed
4. 但**仍然无法直接得到原始私钥**（因为被 crypto_box_seal 加密）

### ⚠️ 场景 3: 动态调试

**攻击**: 使用 GDB/LLDB 调试运行时

```bash
gdb ./myapp
break get_key
run
x/32xb pk  # 查看 pk 内存
```

**结果**: ⚠️ **可以提取运行时的 pk/sk**

**攻击步骤**:
1. 在 `get_key` 函数设置断点
2. 运行到 XOR 还原后
3. 读取栈上的 pk/sk 内存
4. 使用 pk/sk 解密 sealed 得到原始私钥

**防御难度**: ⚠️ **很难防御**
- 需要反调试技术
- 需要代码混淆
- 需要完整性检查

### ⚠️ 场景 4: 内存转储

**攻击**: 在程序运行时转储内存

```bash
gcore <pid>  # 生成 core dump
strings core.<pid> | grep -E '[0-9a-f]{64}'
```

**结果**: ⚠️ **可能提取到密钥**

**条件**:
- 必须在 `get_key` 执行期间转储
- 密钥在栈上的时间很短
- 立即清零降低了风险

### ✅ 场景 5: 网络嗅探

**攻击**: 监听网络流量

**结果**: ✅ **无法获取密钥**

**原因**: 
- 密钥不通过网络传输
- 所有解密都在本地完成

### ✅ 场景 6: 文件系统访问

**攻击**: 只能访问 vault.so 文件

**结果**: ✅ **无法直接获取原始私钥**

**原因**:
- 原始私钥被 crypto_box_seal 加密
- 即使提取了 pk/sk/sealed，仍需要解密算法

## 安全等级评估

### 对不同攻击者的防护

| 攻击者类型 | 防护等级 | 说明 |
|-----------|---------|------|
| 普通用户 | ✅ 高 | 无法用简单工具提取 |
| 脚本小子 | ✅ 高 | strings/hexdump 无效 |
| 中级逆向 | ⚠️ 中 | 可以反汇编，但需要理解算法 |
| 高级逆向 | ❌ 低 | 可以动态调试提取 pk/sk |
| 内存攻击 | ⚠️ 中 | 时间窗口很短 |
| 物理访问 | ❌ 低 | 可以调试或修改二进制 |

### 与其他方案对比

| 方案 | 安全性 | 易用性 | 适用场景 |
|------|--------|--------|----------|
| **vault.so (本方案)** | ⚠️ 中 | ✅ 高 | 一般应用 |
| 明文硬编码 | ❌ 极低 | ✅ 极高 | ❌ 不推荐 |
| 环境变量 | ❌ 低 | ✅ 高 | 开发环境 |
| 配置文件加密 | ⚠️ 中低 | ⚠️ 中 | 需要密钥管理 |
| Android Keystore | ✅ 高 | ⚠️ 中 | Android 专用 |
| HSM/TPM | ✅ 极高 | ❌ 低 | 企业级 |
| AWS KMS/Secrets | ✅ 高 | ✅ 高 | 云环境 |

## 实际风险评估

### 低风险场景 ✅

1. **防止意外泄露**
   - 代码仓库泄露
   - 日志文件泄露
   - 错误消息泄露

2. **防止自动化扫描**
   - 密钥扫描工具
   - 静态分析工具
   - 合规性检查

3. **防止非技术人员**
   - 普通用户
   - 运维人员
   - 客服人员

### 中风险场景 ⚠️

1. **防止一般逆向**
   - 需要一定技能
   - 需要时间投入
   - 提高攻击成本

2. **防止批量攻击**
   - 每个 .so 都不同
   - 无法自动化提取
   - 需要逐个分析

### 高风险场景 ❌

1. **无法防止专业逆向**
   - 有足够时间和技能
   - 可以动态调试
   - 可以修改二进制

2. **无法防止物理访问**
   - 可以附加调试器
   - 可以修改内存
   - 可以 hook 函数

## 改进建议

### 1. 代码混淆

```cpp
// 添加无用代码
void dummy_func() {
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x += i;
}

// 控制流混淆
int get_key(...) {
    if (rand() % 2 == 0) {
        // 真实逻辑
    } else {
        // 永远不执行的分支
    }
}
```

### 2. 反调试

```cpp
#include <sys/ptrace.h>

int get_key(...) {
    // 检测调试器
    if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
        // 被调试，返回错误
        return -1;
    }
    
    // 正常逻辑...
}
```

### 3. 完整性检查

```cpp
// 计算自身哈希
bool verify_integrity() {
    // 读取自身二进制
    // 计算 SHA256
    // 与预期值比较
    return hash == expected_hash;
}

int get_key(...) {
    if (!verify_integrity()) {
        return -1;  // 被修改
    }
    // ...
}
```

### 4. 时间检查

```cpp
int get_key(...) {
    auto start = std::chrono::steady_clock::now();
    
    // 执行解密
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 如果执行时间异常（可能是单步调试）
    if (duration.count() > 100) {
        return -1;
    }
}
```

### 5. 使用硬件安全

**Android**:
```kotlin
// 使用 Android Keystore
val keyStore = KeyStore.getInstance("AndroidKeyStore")
val key = keyStore.getKey("my_key", null)
```

**Linux**:
```cpp
// 使用 TPM
#include <tss2/tss2_esys.h>
// TPM 操作...
```

## 最佳实践建议

### 对于不同安全需求

#### 低安全需求（开发/测试）
```
✅ vault.so 足够
- 防止意外泄露
- 简单易用
- 无额外依赖
```

#### 中等安全需求（一般生产）
```
✅ vault.so + 代码混淆
- 提高逆向难度
- 增加攻击成本
- 仍然易于部署
```

#### 高安全需求（金融/医疗）
```
❌ vault.so 不够
✅ 使用硬件安全方案:
- Android Keystore + TEE
- HSM (Hardware Security Module)
- TPM (Trusted Platform Module)
- Cloud KMS (AWS/GCP/Azure)
```

## 结论

### vault.so 的定位

**适合**:
- ✅ 防止意外泄露
- ✅ 防止自动化扫描
- ✅ 提高攻击门槛
- ✅ 简单易用的场景

**不适合**:
- ❌ 防止专业逆向工程
- ❌ 防止物理访问攻击
- ❌ 高安全性要求场景
- ❌ 需要合规认证的场景

### 安全建议

1. **理解威胁模型**: 明确你要防御什么
2. **分层防御**: 不要只依赖一种方案
3. **定期轮换**: 定期更换密钥
4. **监控异常**: 检测异常访问模式
5. **最小权限**: 限制 .so 文件访问权限

### 最终建议

```
如果你的威胁模型是:
  - 防止代码泄露到 GitHub
  - 防止配置文件被误读
  - 防止日志中出现密钥
  - 防止非技术人员访问
  
  ✅ vault.so 是一个好选择

如果你的威胁模型是:
  - 防止专业黑客攻击
  - 防止恶意内部人员
  - 需要通过安全审计
  - 处理高价值数据
  
  ❌ 需要使用硬件安全方案
     (Android Keystore, HSM, TPM, KMS)
```

## 参考资料

- [libsodium 文档](https://doc.libsodium.org/)
- [OWASP Mobile Security](https://owasp.org/www-project-mobile-security/)
- [Android Keystore](https://developer.android.com/training/articles/keystore)
- [Reverse Engineering 101](https://github.com/wtsxDev/reverse-engineering)
