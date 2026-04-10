package com.example.tcclib

import java.io.File

/**
 * vault.so 编译结果。
 *
 * @param soBytes      编译好的 .so ELF 字节流
 * @param soFile       .so 写入路径
 * @param sealedKeyHex 密文 hex（可公开存储，用于审计）
 * @param errorMessage 失败时的错误信息
 *
 * 注：sk 已混淆内嵌进 so，不对外暴露。
 */
data class KeySealResult(
    val success: Boolean,
    val soBytes: ByteArray?,
    val soFile: File?,
    val sealedKeyHex: String,
    val errorMessage: String = ""
) {
    override fun equals(other: Any?) = false
    override fun hashCode() = System.identityHashCode(this)
}
