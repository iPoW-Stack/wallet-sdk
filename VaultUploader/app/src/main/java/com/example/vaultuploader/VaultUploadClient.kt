package com.example.vaultuploader

import android.util.Base64
import android.util.Log
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Protocol
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.security.SecureRandom
import java.security.cert.X509Certificate
import java.util.concurrent.TimeUnit
import javax.net.ssl.SSLContext
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

/**
 * 上传 vault.so 到服务端。
 *
 * 接口格式（curl 等价）：
 *   curl -sk -X POST https://localhost:8443/upload \
 *     -H "Content-Type: application/json" \
 *     -d '{"key":"","so":"<base64>","name":"mylib"}'
 *
 * key 字段传空字符串（密钥已内嵌 so，无需上传）。
 */
class VaultUploadClient(
    private val baseUrl: String = "https://localhost:8443"
) {

    data class UploadResult(
        val success: Boolean,
        val responseCode: Int = 0,
        val responseBody: String = "",
        val errorMessage: String = ""
    )

    private val client: OkHttpClient = buildTrustAllClient()

    /**
     * 上传 so 二进制。
     *
     * @param soBytes  vault.so 的原始字节
     * @param name     库名称，如 "mylib"
     */
    fun upload(soBytes: ByteArray, name: String): UploadResult {
        return try {
            // so 转 Base64
            val soB64 = Base64.encodeToString(soBytes, Base64.NO_WRAP)

            // key 传空值（密钥已内嵌 so）
            val body = JSONObject().apply {
                put("key", "")       // 空值
                put("so",  soB64)    // so 二进制的 base64
                put("name", name)
            }.toString()

            Log.d(TAG, "Uploading so: ${soBytes.size} bytes, name=$name")

            val request = Request.Builder()
                .url("$baseUrl/upload")
                .post(body.toRequestBody("application/json".toMediaType()))
                .build()

            val response = client.newCall(request).execute()
            val respBody = response.body?.string() ?: ""

            Log.i(TAG, "Upload response: ${response.code} $respBody")

            UploadResult(
                success = response.isSuccessful,
                responseCode = response.code,
                responseBody = respBody
            )
        } catch (e: Exception) {
            Log.e(TAG, "Upload failed", e)
            UploadResult(success = false, errorMessage = e.message ?: "Unknown error")
        }
    }

    /**
     * 信任所有证书的 OkHttpClient（用于 localhost 自签名证书）。
     * 生产环境应替换为固定证书 pinning。
     */
    private fun buildTrustAllClient(): OkHttpClient {
        val trustAll = object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) {}
            override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {}
            override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
        }
        val sslContext = SSLContext.getInstance("TLS").apply {
            init(null, arrayOf<TrustManager>(trustAll), SecureRandom())
        }
        return OkHttpClient.Builder()
            .sslSocketFactory(sslContext.socketFactory, trustAll)
            .hostnameVerifier { _, _ -> true }
            .protocols(listOf(Protocol.HTTP_1_1)) // 强制使用 HTTP/1.1，避免 HTTP/2 兼容性问题
            .connectTimeout(15, TimeUnit.SECONDS)
            .readTimeout(15, TimeUnit.SECONDS)
            .retryOnConnectionFailure(true)
            .build()
    }

    /**
     * 更新私钥。
     * 接口格式：POST /update_private_key?private_key=<hex>
     */
    fun updatePrivateKey(privateKeyHex: String): UploadResult {
        return try {
            // 使用当前实例的 baseUrl，而不是硬编码
            val url = "$baseUrl/update_private_key?private_key=$privateKeyHex"
            
            Log.d(TAG, "Updating private key: $url")

            val request = Request.Builder()
                .url(url)
                .header("Connection", "close") 
                .header("Accept", "application/json")
                .header("User-Agent", "VaultUploader/1.0")
                .post("".toRequestBody("application/json".toMediaType()))
                .build()

            val response = client.newCall(request).execute()
            val respBody = response.body?.string() ?: ""

            Log.i(TAG, "Update response: ${response.code} $respBody")

            UploadResult(
                success = response.isSuccessful,
                responseCode = response.code,
                responseBody = respBody
            )
        } catch (e: Exception) {
            Log.e(TAG, "Update private key failed", e)
            UploadResult(success = false, errorMessage = e.message ?: "Unknown error")
        }
    }

    companion object { private const val TAG = "VaultUploadClient" }
}
