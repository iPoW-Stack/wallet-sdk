package com.example.vaultuploader

import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.RadioGroup
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.example.tcclib.KeySealCompiler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

class MainActivity : AppCompatActivity() {

    private val TAG = "MainActivity"

    // 编译好的字节，保存在内存变量中
    private var compiledBytes: ByteArray? = null
    private var isStaticLib: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val etKeyHex   = findViewById<EditText>(R.id.etKeyHex)
        val etLibName  = findViewById<EditText>(R.id.etLibName)
        val etServer   = findViewById<EditText>(R.id.etServer)
        val rgFormat   = findViewById<RadioGroup>(R.id.rgOutputFormat)
        val btnCompile = findViewById<Button>(R.id.btnCompile)
        val btnUpload  = findViewById<Button>(R.id.btnUpload)
        val tvStatus   = findViewById<TextView>(R.id.tvStatus)

        // 监听格式选择
        rgFormat.setOnCheckedChangeListener { _, checkedId ->
            isStaticLib = (checkedId == R.id.rbStaticLib)
            val ext = if (isStaticLib) ".a" else ".so"
            btnCompile.text = "① 编译 vault$ext"
        }

        // ── 编译 vault ─────────────────────────────────────
        btnCompile.setOnClickListener {
            val keyHex = etKeyHex.text.toString().trim().lowercase()

            if (keyHex.length != 64) {
                tvStatus.text = "❌ 请输入 64 位 hex（32字节密钥）"
                return@setOnClickListener
            }

            val ext = if (isStaticLib) ".a" else ".so"
            val outputFile = File(filesDir, "vault$ext")
            
            tvStatus.text = "⏳ 正在编译 vault$ext ..."

            CoroutineScope(Dispatchers.IO).launch {
                val compiler = KeySealCompiler(this@MainActivity)
                val format = if (isStaticLib) 
                    KeySealCompiler.OutputFormat.STATIC_LIBRARY 
                else 
                    KeySealCompiler.OutputFormat.SHARED_LIBRARY
                    
                val result = compiler.compile(
                    keyHex = keyHex,
                    outputFile = outputFile,
                    format = format
                )

                withContext(Dispatchers.Main) {
                    if (result.success && result.soBytes != null) {
                        // ✅ 字节保存在变量中
                        compiledBytes = result.soBytes

                        // 调试：打印前 16 字节
                        val hexStr = compiledBytes!!.take(16).joinToString(" ") { 
                            "%02x".format(it.toInt() and 0xFF) 
                        }
                        val asciiStr = compiledBytes!!.take(16).map { 
                            val c = it.toInt().toChar()
                            if (c in ' '..'~') c else '.'
                        }.joinToString("")
                        
                        Log.i(TAG, "First 16 bytes (hex): $hexStr")
                        Log.i(TAG, "First 16 bytes (ascii): $asciiStr")

                        tvStatus.text = buildString {
                            appendLine("✅ vault$ext 编译成功")
                            appendLine("格式: ${if (isStaticLib) "静态库 (.a)" else "共享库 (.so)"}")
                            appendLine("大小: ${compiledBytes!!.size} bytes")
                            appendLine("路径: ${result.soFile?.absolutePath}")
                            appendLine()
                            appendLine("文件头 (hex): $hexStr")
                            appendLine("文件头 (ascii): $asciiStr")
                            appendLine()
                            appendLine("密文(sealedKeyHex，可公开):")
                            appendLine(result.sealedKeyHex)
                            appendLine()
                            appendLine("点击「上传」发送到服务器")
                        }
                        Log.i(TAG, "vault$ext compiled: ${compiledBytes!!.size} bytes")
                    } else {
                        tvStatus.text = "❌ 编译失败\n${result.errorMessage}"
                    }
                }
            }
        }

        // ── 上传 vault ─────────────────────────────────────
        btnUpload.setOnClickListener {
            val bytes = compiledBytes
            if (bytes == null) {
                tvStatus.text = "❌ 请先编译 vault"
                return@setOnClickListener
            }

            val name   = etLibName.text.toString().trim().ifEmpty { "mylib" }
            val server = etServer.text.toString().trim().ifEmpty { "https://136.115.134.105:38443" }

            tvStatus.text = "⏳ 正在上传到 $server ..."

            CoroutineScope(Dispatchers.IO).launch {
                val client = VaultUploadClient(baseUrl = server)

                // key 传空值，密钥已内嵌
                val result = client.upload(soBytes = bytes, name = name)

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
