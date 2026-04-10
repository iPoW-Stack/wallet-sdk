package com.example.tcclib

import android.content.Context
import android.util.Log
import com.goterl.lazysodium.SodiumAndroid
import java.io.File

/**
 * KeySealCompiler
 *
 * 输入 32字节 hex 密钥，输出自包含的 vault.so：
 *   - 内部随机生成 X25519 keypair
 *   - 用 pk 密封输入密钥
 *   - pk / sk / 密文以 XOR 混淆形式硬编码进 so
 *   - 导出唯一函数：int get_key(unsigned char* out, unsigned long long* out_len)
 *
 * 调用方只需 libsodium + dlopen，无需传入任何密钥参数。
 */
class KeySealCompiler(private val context: Context) {

    private val sodium = SodiumAndroid()

    companion object {
        private const val TAG        = "KeySealCompiler"
        private const val KEY_BYTES  = 32
        private const val PK_BYTES   = 32
        private const val SK_BYTES   = 32
        private const val SEAL_BYTES = 48
    }

    /**
     * @param keyHex     待保护密钥的 hex 字符串（64字符 = 32字节）
     * @param outputFile vault.so 输出路径
     */
    fun compile(
        keyHex: String,
        outputFile: File = File(context.filesDir, "vault.so")
    ): KeySealResult {
        require(keyHex.length == 64) { "keyHex must be 64 hex chars (32 bytes)" }

        val pk     = ByteArray(PK_BYTES)
        val sk     = ByteArray(SK_BYTES)
        val sealed = ByteArray(KEY_BYTES + SEAL_BYTES)

        return try {
            // 1. 生成 X25519 keypair
            check(sodium.crypto_box_keypair(pk, sk) == 0) { "crypto_box_keypair failed" }

            // 2. 密封输入密钥
            val inputKey = hexToBytes(keyHex.lowercase())
            check(sodium.crypto_box_seal(sealed, inputKey, inputKey.size.toLong(), pk) == 0) {
                "crypto_box_seal failed"
            }
            inputKey.fill(0)

            // 3. 转 hex 传给 JNI（原始字节立即清零）
            val pkHex     = bytesToHex(pk)
            val skHex     = bytesToHex(sk)
            val sealedHex = bytesToHex(sealed)
            pk.fill(0); sk.fill(0); sealed.fill(0)

            // 4. TCC 编译：pk/sk/sealed XOR混淆后硬编码进 so
            val soBytes = TccCompiler.compileKeySealSo(
                pkHex, skHex, sealedHex, context.cacheDir.absolutePath
            ) ?: return KeySealResult(
                success = false, soBytes = null, soFile = null,
                sealedKeyHex = sealedHex, errorMessage = "TCC compile returned null"
            )

            outputFile.parentFile?.mkdirs()
            outputFile.writeBytes(soBytes)
            Log.i(TAG, "vault.so: ${soBytes.size} bytes -> ${outputFile.absolutePath}")

            KeySealResult(
                success = true,
                soBytes = soBytes,
                soFile = outputFile,
                sealedKeyHex = sealedHex
            )
        } catch (e: Exception) {
            pk.fill(0); sk.fill(0); sealed.fill(0)
            Log.e(TAG, "compile failed", e)
            KeySealResult(
                success = false, soBytes = null, soFile = null,
                sealedKeyHex = "", errorMessage = e.message ?: "Unknown error"
            )
        }
    }

    private fun hexToBytes(hex: String) = ByteArray(hex.length / 2) { i ->
        hex.substring(i * 2, i * 2 + 2).toInt(16).toByte()
    }
    private fun bytesToHex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }
}
