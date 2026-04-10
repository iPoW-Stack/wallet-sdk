package com.example.tcccompiler

import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.example.tcclib.KeySealCompiler
import com.example.tcclib.SodiumSealCompiler
import com.example.tcclib.TccCompiler
import java.io.File

class MainActivity : AppCompatActivity() {

    private val TAG = "MainActivity"

    // 编译后的 ELF 字节保存在这个变量中
    private var compiledBytes: ByteArray? = null
    // seal_key.so 字节
    private var sealSoBytes: ByteArray? = null

    private val compiler = TccCompiler()
    private lateinit var sodiumSealCompiler: SodiumSealCompiler
    private lateinit var keySealCompiler: KeySealCompiler

    // 示例 C 代码（不依赖标准库头文件）
    private val sampleCode = """
        int add(int a, int b) {
            return a + b;
        }

        int factorial(int n) {
            if (n <= 1) return 1;
            return n * factorial(n - 1);
        }

        int main() {
            return factorial(5); // 120
        }
    """.trimIndent()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        sodiumSealCompiler = SodiumSealCompiler(this)
        keySealCompiler = KeySealCompiler(this)

        val tvResult = findViewById<TextView>(R.id.tvResult)
        val btnCompile = findViewById<Button>(R.id.btnCompile)
        val btnRun = findViewById<Button>(R.id.btnRun)
        val btnSodiumSeal = findViewById<Button>(R.id.btnSodiumSeal)
        val btnKeySeal = findViewById<Button>(R.id.btnKeySeal)

        btnCompile.setOnClickListener {
            val result = compiler.compile(sampleCode, cacheDir)

            if (result.success) {
                val bytes = result.bytes
                if (bytes != null) {
                    compiledBytes = bytes
                    tvResult.text = buildString {
                        appendLine("✅ 编译成功")
                        appendLine("ELF 大小: ${bytes.size} bytes")
                        appendLine("前16字节 (hex):")
                        appendLine(bytes.take(16).joinToString(" ") { "%02X".format(it) })
                        appendLine()
                        appendLine("变量 compiledBytes 已保存编译结果")
                    }
                    Log.i(TAG, "Compiled ELF bytes: ${bytes.size}")
                }
            } else {
                tvResult.text = "❌ 编译失败\n${result.errorMessage}"
                Log.e(TAG, "Compile error: ${result.errorMessage}")
            }
        }

        btnRun.setOnClickListener {
            val exitCode = compiler.runCode(sampleCode)
            tvResult.text = "▶ 运行完成，main() 返回: $exitCode\n(查看 Logcat 获取 printf 输出)"
        }

        btnSodiumSeal.setOnClickListener {
            tvResult.text = "⏳ 正在编译 seal_key.so ..."
            val outFile = File(filesDir, "seal_key.so")
            val result = sodiumSealCompiler.compile(outFile)

            if (result.success) {
                val soBytes = result.soBytes
                if (soBytes != null) {
                    sealSoBytes = soBytes
                    tvResult.text = buildString {
                        appendLine("✅ seal_key.so 编译成功")
                        appendLine("大小: ${soBytes.size} bytes")
                        appendLine("路径: ${result.soFile?.absolutePath}")
                        appendLine()
                        appendLine("ELF magic: " +
                            soBytes.take(4).joinToString(" ") { "%02X".format(it) })
                        appendLine("导出函数: seal_key / unseal_key")
                    }
                }
            } else {
                tvResult.text = "❌ seal_key.so 编译失败\n${result.errorMessage}"
            }
        }

        btnKeySeal.setOnClickListener {
            // 演示用：32字节随机密钥的 hex
            val demoKeyHex = "deadbeefcafebabe" + "0102030405060708" +
                             "aabbccddeeff0011" + "2233445566778899"
            tvResult.text = "⏳ 正在密封密钥并编译 vault.so ..."

            val result = keySealCompiler.compile(demoKeyHex)

            if (result.success) {
                val soBytes = result.soBytes
                if (soBytes != null) {
                    sealSoBytes = soBytes
                    tvResult.text = buildString {
                        appendLine("✅ sealed_key_vault.so 编译成功")
                        appendLine("so 大小: ${soBytes.size} bytes")
                        appendLine("路径: ${result.soFile?.absolutePath}")
                        appendLine()
                        appendLine("密文(sealedKeyHex):")
                        appendLine(result.sealedKeyHex)
                        appendLine()
                        appendLine("so 导出: get_key()")
                        appendLine("so 自包含，调用方直接 get_key() 即可解密")
                    }
                    Log.i(TAG, "vault.so size=${soBytes.size}")
                }
            } else {
                tvResult.text = "❌ vault.so 编译失败\n${result.errorMessage}"
            }
        }
    }
}
