package com.example.wechathelper.util

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.GestureDescription
import android.graphics.Path
import android.graphics.Rect
import android.os.Bundle
import android.util.Log
import android.view.accessibility.AccessibilityNodeInfo
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume

private const val TAG = "NodeHelper"

// ── 节点查找 ───────────────────────────────────────────────

/** 通过 resource-id 查找第一个匹配节点 */
fun AccessibilityNodeInfo.findById(id: String): AccessibilityNodeInfo? {
    val nodes = findAccessibilityNodeInfosByViewId(id)
    return nodes?.firstOrNull()
}

/** 通过文本内容查找节点 */
fun AccessibilityNodeInfo.findByText(text: String): AccessibilityNodeInfo? {
    val nodes = findAccessibilityNodeInfosByText(text)
    return nodes?.firstOrNull()
}

/** 通过文本精确匹配查找节点 */
fun AccessibilityNodeInfo.findByExactText(text: String): AccessibilityNodeInfo? {
    val nodes = findAccessibilityNodeInfosByText(text)
    return nodes?.firstOrNull { it.text?.toString() == text }
}

/** 递归查找所有 TextView 节点的文本 */
fun AccessibilityNodeInfo.collectAllText(): List<String> {
    val result = mutableListOf<String>()
    collectTextRecursive(this, result)
    return result
}

private fun collectTextRecursive(node: AccessibilityNodeInfo, result: MutableList<String>) {
    val text = node.text?.toString()
    if (!text.isNullOrBlank()) {
        result.add(text)
    }
    for (i in 0 until node.childCount) {
        val child = node.getChild(i) ?: continue
        collectTextRecursive(child, result)
    }
}

/** 递归查找满足条件的第一个节点 */
fun AccessibilityNodeInfo.findFirst(predicate: (AccessibilityNodeInfo) -> Boolean): AccessibilityNodeInfo? {
    if (predicate(this)) return this
    for (i in 0 until childCount) {
        val child = getChild(i) ?: continue
        val found = child.findFirst(predicate)
        if (found != null) return found
    }
    return null
}

/** 递归查找所有满足条件的节点 */
fun AccessibilityNodeInfo.findAll(predicate: (AccessibilityNodeInfo) -> Boolean): List<AccessibilityNodeInfo> {
    val result = mutableListOf<AccessibilityNodeInfo>()
    findAllRecursive(this, predicate, result)
    return result
}

private fun findAllRecursive(
    node: AccessibilityNodeInfo,
    predicate: (AccessibilityNodeInfo) -> Boolean,
    result: MutableList<AccessibilityNodeInfo>
) {
    if (predicate(node)) result.add(node)
    for (i in 0 until node.childCount) {
        val child = node.getChild(i) ?: continue
        findAllRecursive(child, predicate, result)
    }
}

/** 获取可滚动的父节点 */
fun AccessibilityNodeInfo.findScrollableParent(): AccessibilityNodeInfo? {
    var current: AccessibilityNodeInfo? = this
    while (current != null) {
        if (current.isScrollable) return current
        current = current.parent
    }
    return null
}

// ── 节点操作 ───────────────────────────────────────────────

/** 点击节点 */
fun AccessibilityNodeInfo.click(): Boolean {
    return if (isClickable) {
        performAction(AccessibilityNodeInfo.ACTION_CLICK)
    } else {
        // 向上查找可点击的父节点
        var parent = parent
        while (parent != null) {
            if (parent.isClickable) {
                return parent.performAction(AccessibilityNodeInfo.ACTION_CLICK)
            }
            parent = parent.parent
        }
        false
    }
}

/** 向输入框设置文本 */
fun AccessibilityNodeInfo.setText(text: String): Boolean {
    val args = Bundle().apply {
        putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text)
    }
    return performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args)
}

/** 向下滚动 */
fun AccessibilityNodeInfo.scrollForward(): Boolean {
    return performAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD)
}

/** 向上滚动 */
fun AccessibilityNodeInfo.scrollBackward(): Boolean {
    return performAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)
}

// ── 手势操作 ───────────────────────────────────────────────

/** 在指定坐标执行点击手势 */
suspend fun AccessibilityService.clickAt(x: Float, y: Float) {
    val path = Path().apply { moveTo(x, y) }
    val gesture = GestureDescription.Builder()
        .addStroke(GestureDescription.StrokeDescription(path, 0, 100))
        .build()

    suspendCancellableCoroutine { cont ->
        val dispatched = dispatchGesture(gesture, object : AccessibilityService.GestureResultCallback() {
            override fun onCompleted(gestureDescription: GestureDescription?) {
                if (cont.isActive) cont.resume(true)
            }
            override fun onCancelled(gestureDescription: GestureDescription?) {
                if (cont.isActive) cont.resume(false)
            }
        }, null)
        if (!dispatched && cont.isActive) {
            cont.resume(false)
        }
    }
}

/** 点击节点中心坐标（当 performAction(CLICK) 不生效时使用） */
suspend fun AccessibilityService.clickNode(node: AccessibilityNodeInfo) {
    val rect = Rect()
    node.getBoundsInScreen(rect)
    clickAt(rect.centerX().toFloat(), rect.centerY().toFloat())
}

/** 执行滑动手势 */
suspend fun AccessibilityService.swipe(
    startX: Float, startY: Float,
    endX: Float, endY: Float,
    durationMs: Long = 300
) {
    val path = Path().apply {
        moveTo(startX, startY)
        lineTo(endX, endY)
    }
    val gesture = GestureDescription.Builder()
        .addStroke(GestureDescription.StrokeDescription(path, 0, durationMs))
        .build()

    suspendCancellableCoroutine { cont ->
        val dispatched = dispatchGesture(gesture, object : AccessibilityService.GestureResultCallback() {
            override fun onCompleted(gestureDescription: GestureDescription?) {
                if (cont.isActive) cont.resume(true)
            }
            override fun onCancelled(gestureDescription: GestureDescription?) {
                if (cont.isActive) cont.resume(false)
            }
        }, null)
        if (!dispatched && cont.isActive) {
            cont.resume(false)
        }
    }
}

// ── 等待工具 ───────────────────────────────────────────────

/** 等待指定 Activity 出现 */
suspend fun waitForActivity(
    activityName: String,
    getCurrentActivity: () -> String?,
    timeoutMs: Long = 10_000,
    pollMs: Long = 300
): Boolean {
    val deadline = System.currentTimeMillis() + timeoutMs
    while (System.currentTimeMillis() < deadline) {
        if (getCurrentActivity()?.contains(activityName) == true) return true
        delay(pollMs)
    }
    Log.w(TAG, "waitForActivity timeout: $activityName")
    return false
}

/** 等待节点出现 */
suspend fun AccessibilityService.waitForNode(
    id: String,
    timeoutMs: Long = 8_000,
    pollMs: Long = 300
): AccessibilityNodeInfo? {
    val deadline = System.currentTimeMillis() + timeoutMs
    while (System.currentTimeMillis() < deadline) {
        val root = rootInActiveWindow
        if (root == null) {
            delay(pollMs)
        } else {
            val node = root.findById(id)
            if (node != null) return node
            delay(pollMs)
        }
    }
    Log.w(TAG, "waitForNode timeout: $id")
    return null
}

/** 等待包含指定文本的节点出现 */
suspend fun AccessibilityService.waitForText(
    text: String,
    timeoutMs: Long = 8_000,
    pollMs: Long = 300
): AccessibilityNodeInfo? {
    val deadline = System.currentTimeMillis() + timeoutMs
    while (System.currentTimeMillis() < deadline) {
        val root = rootInActiveWindow
        if (root == null) {
            delay(pollMs)
        } else {
            val node = root.findByText(text)
            if (node != null) return node
            delay(pollMs)
        }
    }
    Log.w(TAG, "waitForText timeout: $text")
    return null
}

// ── 调试工具 ───────────────────────────────────────────────

/** 打印节点树（调试用） */
fun AccessibilityNodeInfo.dumpTree(indent: Int = 0) {
    val prefix = "  ".repeat(indent)
    val cls = className?.toString()?.substringAfterLast('.') ?: "?"
    val id = viewIdResourceName ?: ""
    val txt = text?.toString()?.take(30) ?: ""
    val desc = contentDescription?.toString()?.take(30) ?: ""
    val rect = Rect()
    getBoundsInScreen(rect)
    Log.d(TAG, "$prefix[$cls] id=$id text=\"$txt\" desc=\"$desc\" click=$isClickable scroll=$isScrollable rect=$rect")
    for (i in 0 until childCount) {
        getChild(i)?.dumpTree(indent + 1)
    }
}
