#include <jni.h>
#include <android/log.h>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

// libtcc 公开头文件
#include "libtcc/libtcc.h"

#define LOG_TAG "TccBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ── 错误收集回调 ──────────────────────────────────────────────
static void tcc_error_callback(void* opaque, const char* msg) {
    std::string* errBuf = reinterpret_cast<std::string*>(opaque);
    if (errBuf) {
        errBuf->append(msg);
        errBuf->append("\n");
    }
    LOGE("TCC: %s", msg);
}

/**
 * 编译 C 源码，返回内存中的 ELF 二进制（ByteArray）。
 *
 * @param sourceCode  C 源代码字符串
 * @param cacheDir    app 可写的缓存目录（用于临时文件）
 * @return            编译后的 ELF 字节数组；失败时返回 null
 */
extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_example_tcclib_TccCompiler_compileToBytes(
        JNIEnv* env,
        jobject /* thiz */,
        jstring sourceCode,
        jstring cacheDir)
{
    const char* src = env->GetStringUTFChars(sourceCode, nullptr);
    const char* cacheDirStr = env->GetStringUTFChars(cacheDir, nullptr);
    if (!src || !cacheDirStr) return nullptr;

    std::string errorMsg;

    // 1. 创建 TCC 编译上下文
    TCCState* tcc = tcc_new();
    if (!tcc) {
        LOGE("tcc_new() failed");
        env->ReleaseStringUTFChars(sourceCode, src);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        return nullptr;
    }

    // 2. 注册错误回调
    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);

    // 禁用标准库链接（Android 环境下缺少 crt*.o 和 libc）
    tcc_set_options(tcc, "-nostdlib");

    // 3. 输出模式：内存（生成可重定位的 ELF）
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    // 4. 编译源码字符串
    if (tcc_compile_string(tcc, src) != 0) {
        LOGE("Compilation failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        return nullptr;
    }

    // 5. 重定位：将代码加载到内存并解析符号
    if (tcc_relocate(tcc) < 0) {
        LOGE("tcc_relocate() failed");
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        return nullptr;
    }

    // 6. 获取重定位后的代码大小（再次调用，传 nullptr 查询大小）
    //    注意：TCC_OUTPUT_MEMORY 模式下 tcc_relocate 完成后
    //    可通过 tcc_get_symbol 访问符号，但若需要原始字节，
    //    需在 relocate 前用 TCC_OUTPUT_OBJ 或自定义方式。
    //
    //    这里演示两种方案：
    //    方案A（推荐）：OUTPUT_MEMORY + 返回函数指针地址范围的字节
    //    方案B：OUTPUT_OBJ 输出到内存缓冲区，直接拿到 ELF 字节流
    //
    //    下面使用 方案B（OUTPUT_OBJ to memory buffer）更适合"保存二进制"需求

    tcc_delete(tcc);  // 先释放方案A的上下文，重新用方案B

    // ── 方案B：编译为目标文件字节流 ──────────────────────────
    tcc = tcc_new();
    if (!tcc) {
        env->ReleaseStringUTFChars(sourceCode, src);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        return nullptr;
    }
    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);
    errorMsg.clear();

    tcc_set_options(tcc, "-nostdlib");

    // 输出为目标文件（ELF relocatable）
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);

    if (tcc_compile_string(tcc, src) != 0) {
        LOGE("Compilation (obj) failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        return nullptr;
    }

    // 使用 app cache 目录作为临时文件路径（有写权限）
    std::string tmpPath = std::string(cacheDirStr) + "/tcc_out.o";
    env->ReleaseStringUTFChars(cacheDir, cacheDirStr);

    if (tcc_output_file(tcc, tmpPath.c_str()) != 0) {
        LOGE("tcc_output_file failed: %s", tmpPath.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        return nullptr;
    }

    // 读取临时文件到 vector
    FILE* f = fopen(tmpPath.c_str(), "rb");
    if (!f) {
        LOGE("Cannot open tmp file: %s", tmpPath.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
    size_t written = fread(buf.data(), 1, fileSize, f);
    fclose(f);
    remove(tmpPath.c_str());  // 清理临时文件

    if (static_cast<long>(written) != fileSize) {
        LOGE("File read mismatch: expected %ld, got %zu", fileSize, written);
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        return nullptr;
    }

    tcc_delete(tcc);
    env->ReleaseStringUTFChars(sourceCode, src);

    LOGI("Compiled successfully, ELF size = %zu bytes", written);

    // 7. 封装为 Java ByteArray 返回
    jbyteArray result = env->NewByteArray(static_cast<jsize>(written));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(written),
                            reinterpret_cast<const jbyte*>(buf.data()));
    return result;
}

/**
 * 编译并立即执行 main() 函数，返回退出码。
 * 用于快速验证编译结果是否可运行。
 */
extern "C"
JNIEXPORT jint JNICALL
Java_com_example_tcclib_TccCompiler_compileAndRun(
        JNIEnv* env,
        jobject /* thiz */,
        jstring sourceCode,
        jobjectArray args)
{
    const char* src = env->GetStringUTFChars(sourceCode, nullptr);
    if (!src) return -1;

    std::string errorMsg;
    TCCState* tcc = tcc_new();
    if (!tcc) {
        env->ReleaseStringUTFChars(sourceCode, src);
        return -1;
    }

    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);

    if (tcc_compile_string(tcc, src) != 0) {
        LOGE("compileAndRun failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        return -1;
    }

    if (tcc_relocate(tcc) < 0) {
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(sourceCode, src);
        return -1;
    }

    // 获取 main 符号并调用
    typedef int (*MainFunc)(int, char**);
    MainFunc mainFn = reinterpret_cast<MainFunc>(tcc_get_symbol(tcc, "main"));

    int ret = -1;
    if (mainFn) {
        ret = mainFn(0, nullptr);
        LOGI("main() returned %d", ret);
    } else {
        LOGE("Symbol 'main' not found");
    }

    tcc_delete(tcc);
    env->ReleaseStringUTFChars(sourceCode, src);
    return ret;
}

// ═══════════════════════════════════════════════════════════════
//  seal_key.so 动态编译
//
//  编译一段 C 代码，该代码将 libsodium 的 crypto_box_seal 声明为
//  外部符号（不链接进 so），由调用方在运行时提供。
//  产物是一个标准 ELF shared library，导出 seal_key 函数。
// ═══════════════════════════════════════════════════════════════

// C 模板源码：声明 sodium 符号为 extern，不依赖头文件
static const char* SEAL_KEY_C_SOURCE = R"C(
/* libsodium 常量 */
#define crypto_box_SEALBYTES 48ULL

/* 声明 libsodium 函数为外部符号，运行时由调用方的 libsodium 提供 */
extern int crypto_box_seal(
    unsigned char       *c,
    const unsigned char *m,
    unsigned long long   mlen,
    const unsigned char *pk
);

extern int crypto_box_seal_open(
    unsigned char       *m,
    const unsigned char *c,
    unsigned long long   clen,
    const unsigned char *pk,
    const unsigned char *sk
);

/**
 * 用接收方公钥对 key 进行密封（匿名加密）。
 *
 * @param recipient_pk  接收方 X25519 公钥，32 字节
 * @param key           待密封的密钥数据
 * @param key_len       密钥长度（字节）
 * @param out           输出缓冲区，需至少 key_len + 48 字节
 * @param out_len       [out] 实际写入的字节数
 * @return 0 成功，-1 失败
 */
int seal_key(
    const unsigned char *recipient_pk,
    const unsigned char *key,
    unsigned long long   key_len,
    unsigned char       *out,
    unsigned long long  *out_len)
{
    if (!recipient_pk || !key || !out || !out_len || key_len == 0)
        return -1;

    int ret = crypto_box_seal(out, key, key_len, recipient_pk);
    if (ret == 0) {
        *out_len = key_len + crypto_box_SEALBYTES;
    }
    return ret;
}

/**
 * 用接收方密钥对对密封数据进行解封。
 *
 * @param pk        接收方公钥，32 字节
 * @param sk        接收方私钥，32 字节
 * @param sealed    密封数据
 * @param sealed_len 密封数据长度
 * @param out       输出缓冲区，需至少 sealed_len - 48 字节
 * @param out_len   [out] 实际写入的字节数
 * @return 0 成功，-1 失败
 */
int unseal_key(
    const unsigned char *pk,
    const unsigned char *sk,
    const unsigned char *sealed,
    unsigned long long   sealed_len,
    unsigned char       *out,
    unsigned long long  *out_len)
{
    if (!pk || !sk || !sealed || !out || !out_len) return -1;
    if (sealed_len <= crypto_box_SEALBYTES) return -1;

    int ret = crypto_box_seal_open(out, sealed, sealed_len, pk, sk);
    if (ret == 0) {
        *out_len = sealed_len - crypto_box_SEALBYTES;
    }
    return ret;
}
)C";

extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_example_tcclib_TccCompiler_nativeCompileSodiumSealSo(
        JNIEnv* env,
        jclass  /* clazz */,
        jstring cacheDir,
        jstring sodiumLibDir)
{
    const char* cacheDirStr    = env->GetStringUTFChars(cacheDir, nullptr);
    const char* sodiumLibDirStr = env->GetStringUTFChars(sodiumLibDir, nullptr);
    if (!cacheDirStr || !sodiumLibDirStr) return nullptr;

    std::string errorMsg;
    std::string tmpSoPath = std::string(cacheDirStr) + "/seal_key_tmp.so";

    TCCState* tcc = tcc_new();
    if (!tcc) {
        LOGE("tcc_new() failed for sodium seal");
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        env->ReleaseStringUTFChars(sodiumLibDir, sodiumLibDirStr);
        return nullptr;
    }

    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);

    // ⚠️ 必须在 tcc_set_output_type 之前设置 nostdlib，
    // 因为 tcc_set_output_type 内部会立即调用 tccelf_add_crtbegin，
    // 届时才检查 nostdlib 标志。顺序错误就会找 crtbegin_so.o。
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_lib_path(tcc, "");

    // 输出为共享库（.so）
    tcc_set_output_type(tcc, TCC_OUTPUT_DLL);

    // sodium 的 crypto_box_seal 在 C 源码里声明为 extern，
    // TCC 会把它记录为动态未解析符号写入 so 的 .dynsym，
    // 不需要在编译期链接 libsodium，运行时由调用方进程提供。
    // 所以这里不调用 tcc_add_library("sodium")。

    // 编译内置 C 模板
    if (tcc_compile_string(tcc, SEAL_KEY_C_SOURCE) != 0) {
        LOGE("sodium seal compile failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        env->ReleaseStringUTFChars(sodiumLibDir, sodiumLibDirStr);
        return nullptr;
    }

    // 输出 .so 到临时文件
    if (tcc_output_file(tcc, tmpSoPath.c_str()) != 0) {
        LOGE("tcc_output_file (so) failed: %s", tmpSoPath.c_str());
        tcc_delete(tcc);
        env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
        env->ReleaseStringUTFChars(sodiumLibDir, sodiumLibDirStr);
        return nullptr;
    }

    tcc_delete(tcc);
    env->ReleaseStringUTFChars(cacheDir, cacheDirStr);
    env->ReleaseStringUTFChars(sodiumLibDir, sodiumLibDirStr);

    // 读回字节流
    FILE* f = fopen(tmpSoPath.c_str(), "rb");
    if (!f) {
        LOGE("Cannot open compiled so: %s", tmpSoPath.c_str());
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
    size_t nread = fread(buf.data(), 1, fileSize, f);
    fclose(f);
    remove(tmpSoPath.c_str());

    if (static_cast<long>(nread) != fileSize) {
        LOGE("Read mismatch for so: expected %ld got %zu", fileSize, nread);
        return nullptr;
    }

    LOGI("seal_key.so compiled: %zu bytes", nread);

    jbyteArray result = env->NewByteArray(static_cast<jsize>(nread));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(nread),
                            reinterpret_cast<const jbyte*>(buf.data()));
    return result;
}

// ═══════════════════════════════════════════════════════════════
//  KeySeal：接收 Kotlin 层已完成的 pkHex + sealedHex，
//  用 TCC 编译为 .so（keypair 生成和密封在 Kotlin/lazysodium 完成）
// ═══════════════════════════════════════════════════════════════

static const size_t PK_BYTES     = 32;
static const size_t SK_BYTES     = 32;
static const size_t SEAL_BYTES   = 48;
static const size_t KEY_BYTES    = 32;
static const size_t SEALED_LEN   = KEY_BYTES + SEAL_BYTES;  // 80

// 将字节数组转为 C 数组初始化字面量，如 {0x1a,0x2b,...}
static std::string bytes_to_c_array(const uint8_t* data, size_t len) {
    std::string s = "{";
    char buf[8];
    for (size_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "0x%02x", data[i]);
        s += buf;
        if (i + 1 < len) s += ",";
    }
    s += "}";
    return s;
}

// hex 字符串解码为字节数组
static size_t hex_to_bytes(const char* hex, uint8_t* out, size_t out_len) {
    size_t bytes = strlen(hex) / 2;
    if (bytes > out_len) bytes = out_len;
    for (size_t i = 0; i < bytes; i++) {
        unsigned int v = 0;
        sscanf(hex + i * 2, "%02x", &v);
        out[i] = (uint8_t)v;
    }
    return bytes;
}

// 构建混淆版 C 源码：pk/sk 不以连续明文数组存在
//
// 混淆策略：
//   1. XOR 掩码：每个字节与随机掩码 XOR，掩码和密文分开存储
//   2. 字节拆分：32字节拆成4个8字节片段，分散在不同静态变量
//   3. 运行时重组：get_key() 内部重组并在栈上还原，用完立即清零
//   4. 无符号名：内部变量全用无意义短名，不导出
//   5. sealed 同样 XOR 混淆
static std::string build_key_vault_source(
    const uint8_t* pk,
    const uint8_t* sk,
    const uint8_t* sealed_key,
    size_t sealed_len,
    const uint8_t* xor_mask_pk,   // 32字节随机掩码
    const uint8_t* xor_mask_sk,   // 32字节随机掩码
    const uint8_t* xor_mask_seal) // sealed_len 字节随机掩码
{
    // 对 pk/sk/sealed 做 XOR
    std::vector<uint8_t> pk_x(PK_BYTES), sk_x(SK_BYTES), seal_x(sealed_len);
    for (size_t i = 0; i < PK_BYTES;   i++) pk_x[i]   = pk[i]         ^ xor_mask_pk[i];
    for (size_t i = 0; i < SK_BYTES;   i++) sk_x[i]   = sk[i]         ^ xor_mask_sk[i];
    for (size_t i = 0; i < sealed_len; i++) seal_x[i] = sealed_key[i] ^ xor_mask_seal[i];

    // 生成片段变量名（无意义）
    // pk 拆成 4 段 × 8 字节：a0..a3，掩码：m0..m3
    // sk 拆成 4 段 × 8 字节：b0..b3，掩码：n0..n3
    // sealed 拆成 10 段 × 8 字节：c0..c9，掩码：p0..p9

    auto emit_chunk = [](const std::string& var, const std::string& mask_var,
                         const uint8_t* data, const uint8_t* mask, size_t len) -> std::string {
        std::string s;
        s += "static const unsigned char " + var + "[" + std::to_string(len) + "]={";
        for (size_t i = 0; i < len; i++) {
            char buf[8]; snprintf(buf, sizeof(buf), "0x%02x", data[i]);
            s += buf; if (i+1<len) s+=",";
        }
        s += "};\n";
        s += "static const unsigned char " + mask_var + "[" + std::to_string(len) + "]={";
        for (size_t i = 0; i < len; i++) {
            char buf[8]; snprintf(buf, sizeof(buf), "0x%02x", mask[i]);
            s += buf; if (i+1<len) s+=",";
        }
        s += "};\n";
        return s;
    };

    std::string src;
    src += "/* vault */\n";
    src += "#define _SB 48ULL\n";
    src += "#define _SL " + std::to_string(sealed_len) + "ULL\n\n";

    // pk 片段（XOR 后的密文 + 掩码）
    for (int i = 0; i < 4; i++) {
        src += emit_chunk("a"+std::to_string(i), "m"+std::to_string(i),
                          pk_x.data()+i*8, xor_mask_pk+i*8, 8);
    }
    // sk 片段
    for (int i = 0; i < 4; i++) {
        src += emit_chunk("b"+std::to_string(i), "n"+std::to_string(i),
                          sk_x.data()+i*8, xor_mask_sk+i*8, 8);
    }
    // sealed 片段（10 × 8 字节）
    size_t seal_chunks = (sealed_len + 7) / 8;
    for (size_t i = 0; i < seal_chunks; i++) {
        size_t off = i * 8;
        size_t chunk_len = (off + 8 <= sealed_len) ? 8 : (sealed_len - off);
        src += emit_chunk("c"+std::to_string(i), "p"+std::to_string(i),
                          seal_x.data()+off, xor_mask_seal+off, chunk_len);
    }

    src += "\nextern int crypto_box_seal_open(\n"
           "    unsigned char*,const unsigned char*,\n"
           "    unsigned long long,\n"
           "    const unsigned char*,const unsigned char*);\n\n";

    // get_key：栈上重组 pk/sk/sealed，解密后立即清零
    src += "int get_key(unsigned char* out, unsigned long long* out_len) {\n";
    src += "    if (!out || !out_len) return -1;\n";
    src += "    unsigned char pk[32], sk[32];\n";
    src += "    unsigned char sl[" + std::to_string(sealed_len) + "];\n";
    src += "    unsigned long long i;\n";

    // 重组 pk（XOR 还原）
    for (int i = 0; i < 4; i++) {
        src += "    for(i=0;i<8;i++) pk[" + std::to_string(i*8) +
               "+i]=a" + std::to_string(i) + "[i]^m" + std::to_string(i) + "[i];\n";
    }
    // 重组 sk
    for (int i = 0; i < 4; i++) {
        src += "    for(i=0;i<8;i++) sk[" + std::to_string(i*8) +
               "+i]=b" + std::to_string(i) + "[i]^n" + std::to_string(i) + "[i];\n";
    }
    // 重组 sealed
    for (size_t i = 0; i < seal_chunks; i++) {
        size_t off = i * 8;
        size_t chunk_len = (off + 8 <= sealed_len) ? 8 : (sealed_len - off);
        src += "    for(i=0;i<" + std::to_string(chunk_len) + ";i++) sl[" +
               std::to_string(off) + "+i]=c" + std::to_string(i) +
               "[i]^p" + std::to_string(i) + "[i];\n";
    }

    src += "    int r=crypto_box_seal_open(out,sl,_SL,pk,sk);\n";
    src += "    if(r==0) *out_len=_SL-_SB;\n";
    // 立即清零栈上的 pk/sk/sealed
    src += "    for(i=0;i<32;i++){pk[i]=0;sk[i]=0;}\n";
    src += "    for(i=0;i<" + std::to_string(sealed_len) + ";i++) sl[i]=0;\n";
    src += "    return r;\n";
    src += "}\n";

    return src;
}

/**
 * 接收 Kotlin 层传入的 pkHex（64字符）和 sealedHex（160字符），
 * 用 TCC 编译为 sealed_key_vault.so，返回 ELF 字节数组。
 * keypair 生成和 crypto_box_seal 在 Kotlin/lazysodium 层完成，
 * 此函数不依赖 libsodium。
 */
extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_example_tcclib_TccCompiler_nativeCompileKeySealSoImpl(
        JNIEnv* env,
        jclass  /* clazz */,
        jstring pkHexJ,
        jstring skHexJ,
        jstring sealedHexJ,
        jstring cacheDirJ)
{
    const char* pkHex     = env->GetStringUTFChars(pkHexJ, nullptr);
    const char* skHex     = env->GetStringUTFChars(skHexJ, nullptr);
    const char* sealedHex = env->GetStringUTFChars(sealedHexJ, nullptr);
    const char* cacheDir  = env->GetStringUTFChars(cacheDirJ, nullptr);
    if (!pkHex || !skHex || !sealedHex || !cacheDir) return nullptr;

    uint8_t pk[PK_BYTES]       = {};
    uint8_t sk[SK_BYTES]       = {};
    uint8_t sealed[SEALED_LEN] = {};
    size_t pk_len     = hex_to_bytes(pkHex, pk, PK_BYTES);
    size_t sk_len     = hex_to_bytes(skHex, sk, SK_BYTES);
    size_t sealed_len = hex_to_bytes(sealedHex, sealed, SEALED_LEN);

    env->ReleaseStringUTFChars(pkHexJ, pkHex);
    env->ReleaseStringUTFChars(skHexJ, skHex);
    env->ReleaseStringUTFChars(sealedHexJ, sealedHex);

    if (pk_len != PK_BYTES || sk_len != SK_BYTES || sealed_len != SEALED_LEN) {
        LOGE("KeySeal: bad hex lengths pk=%zu sk=%zu sealed=%zu", pk_len, sk_len, sealed_len);
        env->ReleaseStringUTFChars(cacheDirJ, cacheDir);
        return nullptr;
    }

    // 生成随机 XOR 掩码（每次编译不同，so 二进制每次都不一样）
    uint8_t mask_pk[PK_BYTES]       = {};
    uint8_t mask_sk[SK_BYTES]       = {};
    uint8_t mask_seal[SEALED_LEN]   = {};
    // 用 /dev/urandom 生成真随机掩码
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        fread(mask_pk,   1, PK_BYTES,   urandom);
        fread(mask_sk,   1, SK_BYTES,   urandom);
        fread(mask_seal, 1, SEALED_LEN, urandom);
        fclose(urandom);
    } else {
        // fallback：用时间戳混合索引（弱，但总比没有好）
        for (size_t i = 0; i < PK_BYTES;   i++) mask_pk[i]   = (uint8_t)(i * 0x6b + 0xa3);
        for (size_t i = 0; i < SK_BYTES;   i++) mask_sk[i]   = (uint8_t)(i * 0x7d + 0x5f);
        for (size_t i = 0; i < SEALED_LEN; i++) mask_seal[i] = (uint8_t)(i * 0x91 + 0x2e);
    }

    // 构建混淆 C 源码：pk/sk 以 XOR 片段形式分散存储
    std::string src = build_key_vault_source(
        pk, sk, sealed, SEALED_LEN,
        mask_pk, mask_sk, mask_seal);

    // 所有敏感数据用完立即清零
    memset(pk,        0, sizeof(pk));
    memset(sk,        0, sizeof(sk));
    memset(sealed,    0, sizeof(sealed));
    memset(mask_pk,   0, sizeof(mask_pk));
    memset(mask_sk,   0, sizeof(mask_sk));
    memset(mask_seal, 0, sizeof(mask_seal));

    LOGI("KeySeal: C source built (%zu bytes)", src.size());

    std::string errorMsg;
    std::string tmpSoPath = std::string(cacheDir) + "/key_vault_tmp.so";
    env->ReleaseStringUTFChars(cacheDirJ, cacheDir);

    TCCState* tcc = tcc_new();
    if (!tcc) { LOGE("KeySeal: tcc_new failed"); return nullptr; }

    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_lib_path(tcc, "");
    tcc_set_output_type(tcc, TCC_OUTPUT_DLL);

    if (tcc_compile_string(tcc, src.c_str()) != 0) {
        LOGE("KeySeal: compile failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        return nullptr;
    }
    if (tcc_output_file(tcc, tmpSoPath.c_str()) != 0) {
        LOGE("KeySeal: tcc_output_file failed");
        tcc_delete(tcc);
        return nullptr;
    }
    tcc_delete(tcc);

    FILE* f = fopen(tmpSoPath.c_str(), "rb");
    if (!f) { LOGE("KeySeal: cannot open so"); return nullptr; }
    fseek(f, 0, SEEK_END);
    long soSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> soBuf(static_cast<size_t>(soSize));
    fread(soBuf.data(), 1, soSize, f);
    fclose(f);
    remove(tmpSoPath.c_str());

    LOGI("KeySeal: vault.so = %ld bytes", soSize);

    jbyteArray result = env->NewByteArray(static_cast<jsize>(soSize));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(soSize),
                            reinterpret_cast<const jbyte*>(soBuf.data()));
    return result;
}

/**
 * 编译 vault.a 静态库
 * 
 * 与 vault.so 功能相同，但输出为静态库格式 (.a)
 * 可以被其他 C/C++ 项目静态链接
 */
extern "C"
JNIEXPORT jbyteArray JNICALL
Java_com_example_tcclib_TccCompiler_nativeCompileKeySealArchive(
        JNIEnv* env,
        jclass  /* clazz */,
        jstring pkHexJ,
        jstring skHexJ,
        jstring sealedHexJ,
        jstring cacheDirJ)
{
    const char* pkHex     = env->GetStringUTFChars(pkHexJ, nullptr);
    const char* skHex     = env->GetStringUTFChars(skHexJ, nullptr);
    const char* sealedHex = env->GetStringUTFChars(sealedHexJ, nullptr);
    const char* cacheDir  = env->GetStringUTFChars(cacheDirJ, nullptr);
    if (!pkHex || !skHex || !sealedHex || !cacheDir) return nullptr;

    uint8_t pk[PK_BYTES]       = {};
    uint8_t sk[SK_BYTES]       = {};
    uint8_t sealed[SEALED_LEN] = {};
    size_t pk_len     = hex_to_bytes(pkHex, pk, PK_BYTES);
    size_t sk_len     = hex_to_bytes(skHex, sk, SK_BYTES);
    size_t sealed_len = hex_to_bytes(sealedHex, sealed, SEALED_LEN);

    env->ReleaseStringUTFChars(pkHexJ, pkHex);
    env->ReleaseStringUTFChars(skHexJ, skHex);
    env->ReleaseStringUTFChars(sealedHexJ, sealedHex);

    if (pk_len != PK_BYTES || sk_len != SK_BYTES || sealed_len != SEALED_LEN) {
        LOGE("KeySeal Archive: bad hex lengths pk=%zu sk=%zu sealed=%zu", pk_len, sk_len, sealed_len);
        env->ReleaseStringUTFChars(cacheDirJ, cacheDir);
        return nullptr;
    }

    // 生成随机 XOR 掩码
    uint8_t mask_pk[PK_BYTES]       = {};
    uint8_t mask_sk[SK_BYTES]       = {};
    uint8_t mask_seal[SEALED_LEN]   = {};
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        fread(mask_pk,   1, PK_BYTES,   urandom);
        fread(mask_sk,   1, SK_BYTES,   urandom);
        fread(mask_seal, 1, SEALED_LEN, urandom);
        fclose(urandom);
    } else {
        for (size_t i = 0; i < PK_BYTES;   i++) mask_pk[i]   = (uint8_t)(i * 0x6b + 0xa3);
        for (size_t i = 0; i < SK_BYTES;   i++) mask_sk[i]   = (uint8_t)(i * 0x7d + 0x5f);
        for (size_t i = 0; i < SEALED_LEN; i++) mask_seal[i] = (uint8_t)(i * 0x91 + 0x2e);
    }

    // 构建混淆 C 源码
    std::string src = build_key_vault_source(
        pk, sk, sealed, SEALED_LEN,
        mask_pk, mask_sk, mask_seal);

    // 清零敏感数据
    memset(pk,        0, sizeof(pk));
    memset(sk,        0, sizeof(sk));
    memset(sealed,    0, sizeof(sealed));
    memset(mask_pk,   0, sizeof(mask_pk));
    memset(mask_sk,   0, sizeof(mask_sk));
    memset(mask_seal, 0, sizeof(mask_seal));

    LOGI("KeySeal Archive: C source built (%zu bytes)", src.size());

    std::string errorMsg;
    std::string tmpObjPath = std::string(cacheDir) + "/key_vault_tmp.o";
    std::string tmpArchivePath = std::string(cacheDir) + "/key_vault_tmp.a";
    env->ReleaseStringUTFChars(cacheDirJ, cacheDir);

    TCCState* tcc = tcc_new();
    if (!tcc) {
        LOGE("KeySeal Archive: tcc_new() failed");
        return nullptr;
    }

    tcc_set_error_func(tcc, &errorMsg, tcc_error_callback);

    // 设置为目标文件输出（静态库需要先编译为 .o）
    tcc_set_options(tcc, "-nostdlib");
    tcc_set_output_type(tcc, TCC_OUTPUT_OBJ);

    if (tcc_compile_string(tcc, src.c_str()) != 0) {
        LOGE("KeySeal Archive: compile failed:\n%s", errorMsg.c_str());
        tcc_delete(tcc);
        return nullptr;
    }

    if (tcc_output_file(tcc, tmpObjPath.c_str()) != 0) {
        LOGE("KeySeal Archive: output .o failed");
        tcc_delete(tcc);
        return nullptr;
    }

    tcc_delete(tcc);
    LOGI("KeySeal Archive: .o file created");

    // 使用 ar 命令创建静态库
    // 注意：Android 上可能没有 ar 命令，需要使用 NDK 提供的工具链
    // 这里我们手动创建简单的 ar 格式
    
    // 读取 .o 文件
    FILE* objFile = fopen(tmpObjPath.c_str(), "rb");
    if (!objFile) {
        LOGE("KeySeal Archive: cannot open .o file");
        return nullptr;
    }

    fseek(objFile, 0, SEEK_END);
    long objSize = ftell(objFile);
    fseek(objFile, 0, SEEK_SET);

    std::vector<uint8_t> objBuf(objSize);
    size_t objRead = fread(objBuf.data(), 1, objSize, objFile);
    fclose(objFile);
    remove(tmpObjPath.c_str());

    if (static_cast<long>(objRead) != objSize) {
        LOGE("KeySeal Archive: .o file read mismatch");
        return nullptr;
    }

    // 创建简单的 ar 格式
    // AR 文件格式：
    // - 文件头: "!<arch>\n" (8 bytes)
    // - 每个成员:
    //   - 文件头 (60 bytes): 文件名、时间戳、UID、GID、模式、大小
    //   - 文件内容
    //   - 如果大小为奇数，添加一个 '\n' 填充字节
    
    std::vector<uint8_t> archiveBuf;
    
    // AR 文件魔数
    const char* AR_MAGIC = "!<arch>\n";
    archiveBuf.insert(archiveBuf.end(), AR_MAGIC, AR_MAGIC + 8);
    
    // 调试：打印前 16 字节
    LOGI("KeySeal Archive: AR magic bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
         (uint8_t)AR_MAGIC[0], (uint8_t)AR_MAGIC[1], (uint8_t)AR_MAGIC[2], (uint8_t)AR_MAGIC[3],
         (uint8_t)AR_MAGIC[4], (uint8_t)AR_MAGIC[5], (uint8_t)AR_MAGIC[6], (uint8_t)AR_MAGIC[7]);
    
    // 构建文件头 (60 bytes)
    char header[60];
    memset(header, ' ', 60);
    
    // 文件名 (16 bytes): "key_vault.o/"
    snprintf(header, 16, "key_vault.o/");
    
    // 时间戳 (12 bytes): 使用当前时间
    snprintf(header + 16, 12, "%ld", (long)time(nullptr));
    
    // UID (6 bytes): "0"
    snprintf(header + 28, 6, "0");
    
    // GID (6 bytes): "0"
    snprintf(header + 34, 6, "0");
    
    // 文件模式 (8 bytes): "100644"
    snprintf(header + 40, 8, "100644");
    
    // 文件大小 (10 bytes)
    snprintf(header + 48, 10, "%ld", objSize);
    
    // 结束标记 (2 bytes): "`\n"
    header[58] = '`';
    header[59] = '\n';
    
    // 添加文件头
    archiveBuf.insert(archiveBuf.end(), header, header + 60);
    
    // 添加文件内容
    archiveBuf.insert(archiveBuf.end(), objBuf.begin(), objBuf.end());
    
    // 如果大小为奇数，添加填充字节
    if (objSize % 2 == 1) {
        archiveBuf.push_back('\n');
    }

    LOGI("KeySeal Archive: vault.a = %zu bytes", archiveBuf.size());

    jbyteArray result = env->NewByteArray(static_cast<jsize>(archiveBuf.size()));
    env->SetByteArrayRegion(result, 0, static_cast<jsize>(archiveBuf.size()),
                            reinterpret_cast<const jbyte*>(archiveBuf.data()));
    return result;
}
