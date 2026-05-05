package com.example.wechathelper.service

import android.app.Service
import android.content.Intent
import android.graphics.Color
import android.graphics.PixelFormat
import android.os.Build
import android.os.IBinder
import android.util.TypedValue
import android.view.Gravity
import android.view.WindowManager
import android.widget.TextView

/**
 * 悬浮窗状态服务
 *
 * 在微信操作过程中显示一个小的悬浮窗，
 * 让用户知道当前进度，不需要切回本应用。
 */
class FloatingStatusService : Service() {

    companion object {
        var instance: FloatingStatusService? = null
            private set
        const val EXTRA_TEXT = "text"
    }

    private var windowManager: WindowManager? = null
    private var statusView: TextView? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        instance = this
        windowManager = getSystemService(WINDOW_SERVICE) as WindowManager
        createFloatingView()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val text = intent?.getStringExtra(EXTRA_TEXT)
        if (text != null) {
            updateText(text)
        }
        return START_STICKY
    }

    override fun onDestroy() {
        instance = null
        removeFloatingView()
        super.onDestroy()
    }

    fun updateText(text: String) {
        statusView?.text = text
    }

    private fun createFloatingView() {
        val tv = TextView(this).apply {
            text = "微信群助手运行中..."
            setTextColor(Color.WHITE)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            setBackgroundColor(0xCC333333.toInt())
            setPadding(24, 12, 24, 12)
        }

        val layoutType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            layoutType,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.TOP or Gravity.START
            x = 20
            y = 100
        }

        try {
            windowManager?.addView(tv, params)
            statusView = tv
        } catch (e: Exception) {
            // 没有悬浮窗权限，忽略
        }
    }

    private fun removeFloatingView() {
        try {
            statusView?.let { windowManager?.removeView(it) }
        } catch (_: Exception) {}
        statusView = null
    }
}
