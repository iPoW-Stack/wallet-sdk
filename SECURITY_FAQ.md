# 安全常见问题 (FAQ)

## Q1: vault.so 暴露会被解析出私钥吗？

**A**: 不会**直接**暴露，但**可以被逆向工程提取**。

- ❌ 无法用 `strings` 或 `hexdump` 直接提取
- ❌ 无法通过静态分析直接获取明文
- ⚠️ 可以通过动态调试提取运行时数据
- ⚠️ 需要中高级逆向技能和 3-5 小时时间

**详细分析**: 查看 [SECURITY_ANALYSIS.md](SECURITY_ANALYSIS.md)

## Q2: vault.so 比明文硬编码安全多少？

**A**: 显著更安全。

| 方案 | 提取难度 | 所需技能 | 所需时间 |
|------|---------|---------|---------|
| 明文硬编码 | ❌ 极易 | 无 | < 1 分钟 |
| 环境变量 | ❌ 容易 | 基础 | < 5 分钟 |
| **vault.so** | ⚠️ **中等** | **中高级** | **3-5 小时** |
| Android Keystore | ✅ 困难 | 高级 | 数天 |
| HSM | ✅ 极难 | 专家 | 几乎不可能 |

## Q3: 什么情况下 vault.so 足够安全？

**A**: 当攻击成本 > 数据价值时。

**✅ 适合的场景**:
- API 密钥（非关键）
- 配置加密密钥
- 开发/测试环境
- 一般 SaaS 应用
- 防止意外泄露

**❌ 不适合的场景**:
- 银行/支付密钥
- 用户密码主密钥
- 加密货币私钥
- 医疗数据加密
- 需要合规认证

## Q4: 如何提高 vault.so 的安全性？

**A**: 多层防御。

### 基础防护（已实现）
- ✅ crypto_box_seal 加密
- ✅ XOR 混淆
- ✅ 分块存储
- ✅ 随机掩码
- ✅ 栈上清零

### 增强防护（可选）
```cpp
// 1. 反调试
if (ptrace(PTRACE_TRACEME, 0, 1, 0) < 0) {
    return -1;  // 被调试
}

// 2. 完整性检查
if (!verify_binary_hash()) {
    return -1;  // 被修改
}

// 3. 时间检查
if (execution_time > threshold) {
    return -1;  // 可能在单步调试
}

// 4. 环境检查
if (is_emulator() || is_rooted()) {
    return -1;  // 不安全环境
}
```

### 最强防护
- 使用 Android Keystore + TEE
- 使用 HSM (Hardware Security Module)
- 使用 Cloud KMS (AWS/GCP/Azure)

## Q5: .so 和 .a 哪个更安全？

**A**: 安全性相同，使用场景不同。

| 特性 | .so (共享库) | .a (静态库) |
|------|-------------|-------------|
| 安全性 | ⚠️ 相同 | ⚠️ 相同 |
| 加载方式 | dlopen (运行时) | 静态链接 (编译时) |
| 文件独立性 | ✅ 可独立更新 | ❌ 编译进可执行文件 |
| 逆向难度 | ⚠️ 相同 | ⚠️ 相同 |
| 部署复杂度 | ⚠️ 需要分发 .so | ✅ 单一可执行文件 |

**建议**:
- 需要独立更新 → 使用 .so
- 需要简化部署 → 使用 .a

## Q6: 攻击者需要什么才能提取密钥？

**A**: 本地访问 + 中高级技能。

### 必需条件
1. ✅ 物理或远程访问运行环境
2. ✅ 能够执行调试器（GDB/LLDB）
3. ✅ 理解汇编和逆向工程
4. ✅ 理解加密算法

### 无法远程攻击
- ❌ 无法通过网络嗅探
- ❌ 无法通过 SQL 注入
- ❌ 无法通过 XSS
- ❌ 无法通过 API 调用

### 时间成本
- 有经验的逆向工程师: 3-5 小时
- 一般开发者: 1-2 天
- 脚本小子: 无法完成

## Q7: 如何检测 vault.so 被逆向？

**A**: 实施监控和告警。

### 运行时检测
```cpp
// 检测调试器
bool is_debugged() {
    return ptrace(PTRACE_TRACEME, 0, 1, 0) < 0;
}

// 检测异常调用频率
static int call_count = 0;
static time_t last_call = 0;

int get_key(...) {
    time_t now = time(NULL);
    if (now - last_call < 1) {
        call_count++;
        if (call_count > 10) {
            // 异常：1秒内调用超过10次
            log_security_event("Possible attack");
            return -1;
        }
    } else {
        call_count = 0;
    }
    last_call = now;
    // ...
}
```

### 日志监控
```cpp
// 记录所有调用
void log_key_access(const char* caller) {
    syslog(LOG_WARNING, "Key accessed by %s", caller);
}
```

### 告警规则
- 短时间内多次调用
- 从异常进程调用
- 在调试器下运行
- 二进制文件被修改

## Q8: 密钥轮换怎么办？

**A**: 重新编译并部署新的 vault.so。

### 轮换流程
```bash
# 1. 生成新密钥
NEW_KEY=$(openssl rand -hex 32)

# 2. 在 VaultUploader 中编译
# 输入新密钥，生成 vault_v2.so

# 3. 部署新版本
cp vault_v2.so /path/to/app/vault.so

# 4. 重启应用
systemctl restart myapp
```

### 自动化
```python
#!/usr/bin/env python3
import subprocess
import datetime

def rotate_key():
    # 生成新密钥
    new_key = subprocess.check_output(
        ["openssl", "rand", "-hex", "32"]
    ).decode().strip()
    
    # 编译新 vault.so
    # (需要调用 VaultUploader API 或使用 tcc-ubuntu)
    
    # 部署
    timestamp = datetime.datetime.now().strftime("%Y%m%d")
    subprocess.run([
        "cp", f"vault_{timestamp}.so", "/path/to/vault.so"
    ])
    
    # 重启
    subprocess.run(["systemctl", "restart", "myapp"])

# 定期执行（如每月）
rotate_key()
```

## Q9: 可以在生产环境使用吗？

**A**: 取决于你的威胁模型。

### ✅ 可以使用的场景

**低风险应用**:
- 内部工具
- 开发/测试环境
- 非关键 API 密钥
- 配置文件加密

**中风险应用**:
- 一般 SaaS 应用
- 非金融数据
- 日志加密
- 备份加密

### ❌ 不应使用的场景

**高风险应用**:
- 银行/支付系统
- 加密货币钱包
- 医疗记录系统
- 政府/军事系统

**合规要求**:
- PCI DSS Level 1
- HIPAA (医疗)
- SOC 2 Type II
- ISO 27001

## Q10: 有更好的替代方案吗？

**A**: 有，但复杂度更高。

### Android 平台
```kotlin
// Android Keystore + TEE
val keyStore = KeyStore.getInstance("AndroidKeyStore")
val keyGenerator = KeyGenerator.getInstance(
    KeyProperties.KEY_ALGORITHM_AES,
    "AndroidKeyStore"
)
val keyGenSpec = KeyGenParameterSpec.Builder(
    "my_key",
    KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
)
    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
    .setUserAuthenticationRequired(true)  // 需要生物识别
    .build()

keyGenerator.init(keyGenSpec)
val key = keyGenerator.generateKey()
```

### Linux 服务器
```cpp
// 使用 TPM 2.0
#include <tss2/tss2_esys.h>

ESYS_CONTEXT *esys_ctx;
Esys_Initialize(&esys_ctx, NULL, NULL);

// 创建密钥
TPM2B_SENSITIVE_CREATE inSensitive = {...};
TPM2B_PUBLIC inPublic = {...};
TPM2_HANDLE objectHandle;

Esys_Create(esys_ctx, primaryHandle,
            &inSensitive, &inPublic,
            &outPrivate, &outPublic, ...);
```

### 云环境
```python
# AWS KMS
import boto3

kms = boto3.client('kms')

# 加密
response = kms.encrypt(
    KeyId='alias/my-key',
    Plaintext=b'my-secret-data'
)
ciphertext = response['CiphertextBlob']

# 解密
response = kms.decrypt(
    CiphertextBlob=ciphertext
)
plaintext = response['Plaintext']
```

## Q11: 如何评估是否需要升级安全方案？

**A**: 使用风险评估矩阵。

### 评估维度

1. **数据价值**
   - 低: 配置、日志
   - 中: API 密钥、用户数据
   - 高: 支付信息、密码

2. **攻击者能力**
   - 低: 脚本小子
   - 中: 一般黑客
   - 高: 专业团队、国家级

3. **攻击成本**
   - 低: < 1 天
   - 中: 1-7 天
   - 高: > 1 周

4. **影响范围**
   - 低: 单个用户
   - 中: 部分用户
   - 高: 所有用户

### 决策矩阵

| 数据价值 | 攻击者能力 | 推荐方案 |
|---------|-----------|---------|
| 低 | 低 | ✅ vault.so |
| 低 | 中 | ✅ vault.so |
| 低 | 高 | ⚠️ vault.so + 监控 |
| 中 | 低 | ✅ vault.so |
| 中 | 中 | ⚠️ vault.so + 增强 |
| 中 | 高 | ❌ 硬件安全 |
| 高 | 低 | ⚠️ vault.so + 增强 |
| 高 | 中 | ❌ 硬件安全 |
| 高 | 高 | ❌ 硬件安全 + 审计 |

## Q12: 总结建议

### 快速决策树

```
你的数据如果泄露会导致:
├─ 用户不满 → ✅ vault.so 足够
├─ 财务损失 < $10,000 → ✅ vault.so + 监控
├─ 财务损失 $10,000-$100,000 → ⚠️ vault.so + 增强防护
├─ 财务损失 > $100,000 → ❌ 使用硬件安全方案
└─ 法律责任/合规要求 → ❌ 使用硬件安全方案
```

### 最佳实践

1. **理解威胁**: 明确你要防御什么
2. **分层防御**: 不要只依赖一种方案
3. **定期评估**: 威胁模型会变化
4. **监控告警**: 检测异常访问
5. **应急预案**: 准备密钥泄露响应流程

## 相关文档

- [完整安全分析](SECURITY_ANALYSIS.md)
- [逆向工程演示](REVERSE_ENGINEERING_DEMO.md)
- [C++ 使用指南](STATIC_LIBRARY_CPP_USAGE.md)
- [VaultUploader 指南](VaultUploader/STATIC_LIBRARY_GUIDE.md)
