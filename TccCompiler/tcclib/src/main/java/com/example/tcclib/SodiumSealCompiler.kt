package com.example.tcclib

import android.content.Context
import android.util.Log
import java.io.File

/**
 * SodiumSealCompiler
 *
 * 用 TCC 编译通用 seal_key.so：
 * 导出 seal_key(pk, key, key_len, out, out_len) 和 unseal_key(pk, sk, ...)
 * 调用方自行提供 pk/sk，依赖 libsodium。
 */
class SodiumSealCompiler(private val context: Context) {

    data class CompileResult(
        val success: Boolean,
        val soBytes: ByteArray?,
        val soFile: File?,
        val errorMessage: String = ""
    )

    fun compile(outputFile: File = File(context.filesDir, "seal_key.so")): CompileResult {
        return try {
            val soBytes = TccCompiler.compileSodiumSealSo(
                context.cacheDir.absolutePath,
                context.applicationInfo.nativeLibraryDir
            )
            if (soBytes != null && soBytes.isNotEmpty()) {
                outputFile.parentFile?.mkdirs()
                outputFile.writeBytes(soBytes)
                Log.i(TAG, "seal_key.so: ${soBytes.size} bytes")
                CompileResult(success = true, soBytes = soBytes, soFile = outputFile)
            } else {
                CompileResult(success = false, soBytes = null, soFile = null,
                    errorMessage = "TCC returned empty output")
            }
        } catch (e: Exception) {
            Log.e(TAG, "compile failed", e)
            CompileResult(success = false, soBytes = null, soFile = null,
                errorMessage = e.message ?: "Unknown error")
        }
    }

    companion object { private const val TAG = "SodiumSealCompiler" }
}
