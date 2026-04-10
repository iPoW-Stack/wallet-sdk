package com.example.vaultuploader

import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.example.tcclib.KeySealCompiler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    private val TAG = "MainActivity"

    // 编译好的 so 字节，保存在内存变量中
    private var compiledSoBytes: ByteArray? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val etKeyHex   = findViewById<EditText>(R.id.etKeyHex)
        val etLibName  = findViewById<EditText>(R.id.etLibName)
        val etServer   = findViewById<EditText>(R.id.etServer)
        val btnCompile = findViewById<Button>(R.id.btnCompile)
        val btnUpload  = findViewById<Button>(R.id.btnUpload)
        val tvStatus   = findViewById<TextView>(R.id.tvStatus)

        // ── 编译 vault.so ─────────────────────────────────────
        btnCompile.setOnClickListener {
            val keyHex = etKeyHex.text.toString().trim().lowercase()

            if (keyHex.length != 64) {
                tvStatus.text = "❌ 请输入 64 位 hex（32字节密钥）"
                return@setOnClickListener
            }

            tvStatus.text = "⏳ 正在编译 vault.so ..."

            CoroutineScope(Dispatchers.IO).launch {
                val compiler = KeySealCompiler(this@MainActivity)
                val result = compiler.compile(keyHex = keyHex)

                withContext(Dispatchers.Main) {
                    if (result.success && result.soBytes != null) {
                        // ✅ so 字节保存在变量中
                        compiledSoBytes = result.soBytes

                        tvStatus.text = buildString {
                            appendLine("✅ vault.so 编译成功")
                            appendLine("大小: ${compiledSoBytes!!.size} bytes")
                            appendLine("路径: ${result.soFile?.absolutePath}")
                            appendLine()
                            appendLine("密文(sealedKeyHex，可公开):")
                            appendLine(result.sealedKeyHex)
                            appendLine()
                            appendLine("点击「上传」发送到服务器")
                        }
                        Log.i(TAG, "vault.so compiled: ${compiledSoBytes!!.size} bytes")
                    } else {
                        tvStatus.text = "❌ 编译失败\n${result.errorMessage}"
                    }
                }
            }
        }

        // ── 上传 vault.so ─────────────────────────────────────
        btnUpload.setOnClickListener {
            val soBytes = compiledSoBytes
            if (soBytes == null) {
                tvStatus.text = "❌ 请先编译 vault.so"
                return@setOnClickListener
            }

            val name   = etLibName.text.toString().trim().ifEmpty { "mylib" }
            val server = etServer.text.toString().trim().ifEmpty { "https://localhost:8443" }

            tvStatus.text = "⏳ 正在上传到 $server ..."

            CoroutineScope(Dispatchers.IO).launch {
                val client = VaultUploadClient(baseUrl = server)

                // key 传空值，密钥已内嵌 so
                val result = client.upload(soBytes = soBytes, name = name)

                withContext(Dispatchers.Main) {
                    tvStatus.text = if (result.success) {
                        buildString {
                            appendLine("✅ 上传成功")
                            appendLine("HTTP ${result.responseCode}")
                            appendLine(result.responseBody)
                        }
                    } else {
                        "❌ 上传失败\n${result.errorMessage}\nHTTP ${result.responseCode}"
                    }
                }
            }
        }
    }
}
