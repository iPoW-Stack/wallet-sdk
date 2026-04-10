package com.example.tcclib

import java.io.File

/**
 * TccCompiler — JNI 入口
 *
 * 通过 JNI 调用 libtcc，将 C 源码编译为 ELF 字节流，
 * 或生成自包含的 vault.so（密钥密封）。
 */
class TccCompiler {

    data class CompileResult(
        val success: Boolean,
        val bytes: ByteArray?,
        val errorMessage: String = ""
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is CompileResult) return false
            return success == other.success &&
                    (bytes ?: byteArrayOf()).contentEquals(other.bytes ?: byteArrayOf()) &&
                    errorMessage == other.errorMessage
        }
        override fun hashCode(): Int {
            var r = success.hashCode()
            r = 31 * r + (bytes?.contentHashCode() ?: 0)
            r = 31 * r + errorMessage.hashCode()
            return r
        }
    }

    /** 编译 C 源码，返回 ELF 目标文件字节流 */
    fun compile(sourceCode: String, cacheDir: File): CompileResult = try {
        val bytes = compileToBytes(sourceCode, cacheDir.absolutePath)
        if (bytes != null && bytes.isNotEmpty())
            CompileResult(success = true, bytes = bytes)
        else
            CompileResult(success = false, bytes = null, errorMessage = "Empty result")
    } catch (e: Exception) {
        CompileResult(success = false, bytes = null, errorMessage = e.message ?: "Unknown error")
    }

    /** 编译并执行 main()，返回退出码 */
    fun runCode(sourceCode: String): Int = compileAndRun(sourceCode, emptyArray())

    private external fun compileToBytes(sourceCode: String, cacheDir: String): ByteArray?
    private external fun compileAndRun(sourceCode: String, args: Array<String>): Int

    companion object {
        init { System.loadLibrary("tcc_bridge") }

        /** 编译通用 seal_key.so（seal/unseal 函数，调用方传 pk/sk） */
        fun compileSodiumSealSo(cacheDir: String, sodiumLibDir: String): ByteArray? =
            nativeCompileSodiumSealSo(cacheDir, sodiumLibDir)

        /**
         * 编译 vault.so：pk/sk/sealed 全部硬编码（XOR混淆），
         * 调用方只需 get_key(out, out_len) 即可解密。
         */
        fun compileKeySealSo(
            pkHex: String, skHex: String, sealedHex: String, cacheDir: String
        ): ByteArray? = nativeCompileKeySealSoImpl(pkHex, skHex, sealedHex, cacheDir)

        @JvmStatic private external fun nativeCompileSodiumSealSo(
            cacheDir: String, sodiumLibDir: String): ByteArray?

        @JvmStatic private external fun nativeCompileKeySealSoImpl(
            pkHex: String, skHex: String, sealedHex: String, cacheDir: String): ByteArray?
    }
}
