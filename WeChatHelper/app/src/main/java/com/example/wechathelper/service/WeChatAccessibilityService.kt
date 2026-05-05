package com.example.wechathelper.service

import android.accessibilityservice.AccessibilityService
import android.content.Intent
import android.util.Log
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import com.example.wechathelper.WeChatIds
import com.example.wechathelper.model.GroupMember
import com.example.wechathelper.model.TaskConfig
import com.example.wechathelper.util.*
import kotlinx.coroutines.*

/**
 * 微信无障碍服务
 *
 * 核心功能：
 * 1. 获取指定群的所有成员列表（含备注）
 * 2. 逐个私聊发送消息
 *
 * 工作流程：
 *   打开微信 → 搜索群名 → 进入群聊 → 打开群信息 → 查看全部成员
 *   → 遍历成员列表 → 点击每个成员获取备注 → 返回
 *   → 逐个搜索成员 → 打开私聊 → 发送消息
 */
class WeChatAccessibilityService : AccessibilityService() {

    companion object {
        private const val TAG = "WeChatA11y"

        /** 单例引用，供 Activity 调用 */
        var instance: WeChatAccessibilityService? = null
            private set
    }

    /** 当前 Activity 类名 */
    @Volatile
    var currentActivity: String? = null
        private set

    /** 任务协程作用域 */
    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    /** 当前运行的任务 */
    private var currentJob: Job? = null

    /** 状态回调 */
    var onStatusUpdate: ((String) -> Unit)? = null

    /** 成员列表回调 */
    var onMembersCollected: ((List<GroupMember>) -> Unit)? = null

    /** 任务完成回调 */
    var onTaskComplete: ((success: Boolean, message: String) -> Unit)? = null

    override fun onServiceConnected() {
        super.onServiceConnected()
        instance = this
        Log.i(TAG, "无障碍服务已连接")
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        event ?: return
        // 跟踪当前 Activity
        if (event.eventType == AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED) {
            val cls = event.className?.toString() ?: return
            if (cls.startsWith("com.tencent.mm")) {
                currentActivity = cls
                Log.d(TAG, "Activity: $cls")
            }
        }
    }

    override fun onInterrupt() {
        Log.w(TAG, "无障碍服务被中断")
    }

    override fun onDestroy() {
        instance = null
        serviceScope.cancel()
        super.onDestroy()
        Log.i(TAG, "无障碍服务已销毁")
    }

    // ════════════════════════════════════════════════════════
    //  公开 API
    // ════════════════════════════════════════════════════════

    /** 是否有任务在运行 */
    val isRunning: Boolean get() = currentJob?.isActive == true

    /** 取消当前任务 */
    fun cancelTask() {
        currentJob?.cancel()
        currentJob = null
        updateStatus("⏹ 任务已取消")
    }

    /**
     * 步骤一：获取群成员列表
     * 前提：微信已打开且在主界面
     */
    fun startCollectMembers(groupName: String) {
        if (isRunning) {
            updateStatus("⚠️ 有任务正在运行")
            return
        }
        currentJob = serviceScope.launch {
            try {
                val members = collectGroupMembers(groupName)
                onMembersCollected?.invoke(members)
                onTaskComplete?.invoke(true, "获取到 ${members.size} 个成员")
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                Log.e(TAG, "获取成员失败", e)
                onTaskComplete?.invoke(false, "失败: ${e.message}")
            }
        }
    }

    /**
     * 步骤二：逐个发送私聊消息
     * @param members 要发送的成员列表
     * @param config  任务配置
     */
    fun startSendMessages(members: List<GroupMember>, config: TaskConfig) {
        if (isRunning) {
            updateStatus("⚠️ 有任务正在运行")
            return
        }
        currentJob = serviceScope.launch {
            try {
                sendMessagesToMembers(members, config)
                onTaskComplete?.invoke(true, "全部发送完成")
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                Log.e(TAG, "发送消息失败", e)
                onTaskComplete?.invoke(false, "失败: ${e.message}")
            }
        }
    }

    // ════════════════════════════════════════════════════════
    //  步骤一：获取群成员
    // ════════════════════════════════════════════════════════

    private suspend fun collectGroupMembers(groupName: String): List<GroupMember> {
        updateStatus("🔍 正在搜索群: $groupName")

        // 1. 确保在微信主界面
        ensureWeChatMainPage()
        delay(500)

        // 2. 搜索并进入群聊
        searchAndEnterChat(groupName)
        delay(1000)

        // 3. 打开群聊信息页（点击右上角 ⋯）
        updateStatus("📋 正在打开群信息...")
        openChatInfo()
        delay(1000)

        // 4. 点击「查看全部群成员」
        updateStatus("👥 正在加载成员列表...")
        openFullMemberList()
        delay(1500)

        // 5. 遍历成员列表，收集所有成员
        val members = scrollAndCollectMembers()
        updateStatus("✅ 共获取 ${members.size} 个成员")

        // 6. 返回主界面
        pressBack()
        delay(300)
        pressBack()
        delay(300)
        pressBack()
        delay(300)

        return members
    }

    /** 确保微信在主界面 */
    private suspend fun ensureWeChatMainPage() {
        // 启动微信
        val intent = packageManager.getLaunchIntentForPackage(WeChatIds.WECHAT_PKG)
        if (intent != null) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
            startActivity(intent)
        }
        delay(1500)

        // 如果不在主界面，连续按返回
        var retries = 0
        while (currentActivity != null &&
            !currentActivity!!.contains("LauncherUI") &&
            retries < 5
        ) {
            pressBack()
            delay(500)
            retries++
        }
    }

    /** 搜索并进入指定聊天 */
    private suspend fun searchAndEnterChat(name: String) {
        val root = rootInActiveWindow ?: error("无法获取窗口")

        // 点击搜索按钮（主界面顶部放大镜）
        val searchBtn = root.findById(WeChatIds.ID_SEARCH_BTN)
        if (searchBtn != null) {
            searchBtn.click()
            delay(800)
        } else {
            // 备选：尝试点击顶部搜索区域
            clickAt(screenWidth() / 2f, 140f)
            delay(800)
        }

        // 输入搜索关键词
        val searchInput = waitForEditText(5000)
            ?: error("找不到搜索输入框")
        searchInput.setText(name)
        delay(1500)

        // 点击搜索结果
        val resultRoot = rootInActiveWindow ?: error("搜索后无法获取窗口")
        val resultNode = resultRoot.findByExactText(name)
            ?: resultRoot.findByText(name)
            ?: error("搜索结果中找不到: $name")
        resultNode.click()
        delay(1500)

        updateStatus("✅ 已进入: $name")
    }

    /** 打开聊天信息页 */
    private suspend fun openChatInfo() {
        val root = rootInActiveWindow ?: error("无法获取窗口")

        // 点击右上角 ⋯ 按钮
        val moreBtn = root.findById(WeChatIds.ID_CHAT_MORE_BTN)
        if (moreBtn != null) {
            moreBtn.click()
        } else {
            // 备选：点击右上角区域
            clickAt(screenWidth() - 50f, 80f)
        }
        delay(1500)
    }

    /** 打开完整成员列表 */
    private suspend fun openFullMemberList() {
        val root = rootInActiveWindow ?: error("无法获取窗口")

        // 查找「查看全部群成员」或成员数量文本（如 "查看全部100位群成员"）
        val viewAllBtn = root.findByText("查看全部")
            ?: root.findById(WeChatIds.ID_VIEW_ALL_MEMBERS)
            ?: root.findById(WeChatIds.ID_MEMBER_COUNT)

        if (viewAllBtn != null) {
            viewAllBtn.click()
            delay(1500)
        } else {
            // 如果找不到，可能需要先滚动到成员区域
            // 群信息页顶部就是成员头像网格
            val memberGrid = root.findById(WeChatIds.ID_MEMBER_GRID)
            if (memberGrid != null) {
                // 点击成员数量区域
                memberGrid.click()
                delay(1500)
            } else {
                updateStatus("⚠️ 找不到成员列表入口，尝试滚动查找...")
                // 向下滚动查找
                swipe(
                    screenWidth() / 2f, screenHeight() * 0.7f,
                    screenWidth() / 2f, screenHeight() * 0.3f
                )
                delay(800)
                val retryRoot = rootInActiveWindow ?: return
                val retryBtn = retryRoot.findByText("查看全部")
                retryBtn?.click()
                delay(1500)
            }
        }
    }

    /** 滚动并收集所有成员 */
    private suspend fun scrollAndCollectMembers(): List<GroupMember> {
        val members = mutableListOf<GroupMember>()
        val seenNames = mutableSetOf<String>()
        var noNewCount = 0

        while (noNewCount < 3) {
            val root = rootInActiveWindow ?: break
            val beforeSize = seenNames.size

            // 查找列表中的所有文本节点
            // 成员列表通常是 ListView 或 RecyclerView，每个 item 包含昵称
            val listNode = root.findById(WeChatIds.ID_MEMBER_LIST)
                ?: root.findFirst { it.className?.toString() == WeChatIds.CLASS_LIST_VIEW }
                ?: root.findFirst { it.className?.toString() == WeChatIds.CLASS_RECYCLER_VIEW }

            if (listNode != null) {
                // 遍历列表子项
                for (i in 0 until listNode.childCount) {
                    val child = listNode.getChild(i) ?: continue
                    val texts = child.collectAllText()
                    // 第一个文本通常是昵称/备注
                    val name = texts.firstOrNull()?.trim() ?: continue

                    // 跳过非成员项（如搜索框、邀请按钮等）
                    if (name.isEmpty() || name == "邀请" || name == "删除" ||
                        name.contains("搜索") || name.length > 30
                    ) continue

                    if (seenNames.add(name)) {
                        members.add(
                            GroupMember(
                                nickname = name,
                                displayName = name
                            )
                        )
                    }
                }
            } else {
                // 备选：收集页面上所有 TextView
                val allTexts = root.collectAllText()
                for (text in allTexts) {
                    val name = text.trim()
                    if (name.isEmpty() || name.length > 30 || name.contains("群成员") ||
                        name.contains("搜索") || name == "邀请" || name == "删除"
                    ) continue
                    if (seenNames.add(name)) {
                        members.add(GroupMember(nickname = name, displayName = name))
                    }
                }
            }

            if (seenNames.size == beforeSize) {
                noNewCount++
            } else {
                noNewCount = 0
                updateStatus("👥 已收集 ${members.size} 个成员...")
            }

            // 向下滚动
            val scrollable = listNode ?: root.findFirst { it.isScrollable }
            if (scrollable != null) {
                scrollable.scrollForward()
            } else {
                swipe(
                    screenWidth() / 2f, screenHeight() * 0.8f,
                    screenWidth() / 2f, screenHeight() * 0.2f
                )
            }
            delay(800)
        }

        return members
    }

    // ════════════════════════════════════════════════════════
    //  步骤二：逐个发送私聊消息
    // ════════════════════════════════════════════════════════

    private suspend fun sendMessagesToMembers(members: List<GroupMember>, config: TaskConfig) {
        val total = members.size
        var sent = 0
        var failed = 0

        for ((index, member) in members.withIndex()) {
            if (!isActive) break

            val name = member.searchName
            updateStatus("📤 [${index + 1}/$total] 正在发送给: $name")

            try {
                // 回到微信主界面
                ensureWeChatMainPage()
                delay(500)

                // 搜索联系人
                searchAndEnterChat(name)
                delay(800)

                // 发送消息
                sendMessage(config.message)
                delay(500)

                sent++
                updateStatus("✅ [${index + 1}/$total] 已发送给: $name ($sent 成功, $failed 失败)")

                // 返回主界面
                pressBack()
                delay(300)

            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                failed++
                Log.e(TAG, "发送给 $name 失败", e)
                updateStatus("❌ [${index + 1}/$total] 发送给 $name 失败: ${e.message}")

                // 尝试返回主界面
                try {
                    pressBack()
                    delay(300)
                    pressBack()
                    delay(300)
                } catch (_: Exception) {}
            }

            // 消息间隔，防止风控
            if (index < total - 1) {
                updateStatus("⏳ 等待 ${config.delayMs / 1000} 秒...")
                delay(config.delayMs)
            }
        }

        updateStatus("🏁 发送完成: $sent 成功, $failed 失败, 共 $total 人")
    }

    /** 在当前聊天界面发送消息 */
    private suspend fun sendMessage(message: String) {
        val root = rootInActiveWindow ?: error("无法获取窗口")

        // 查找输入框
        var inputBox = root.findById(WeChatIds.ID_MSG_INPUT)

        // 如果找不到输入框，可能处于语音模式，需要切换到文字模式
        if (inputBox == null) {
            val switchBtn = root.findById(WeChatIds.ID_SWITCH_TO_TEXT)
            if (switchBtn != null) {
                switchBtn.click()
                delay(500)
            }
            inputBox = rootInActiveWindow?.findById(WeChatIds.ID_MSG_INPUT)
        }

        // 备选：查找 EditText
        if (inputBox == null) {
            inputBox = root.findFirst {
                it.className?.toString() == WeChatIds.CLASS_EDIT_TEXT && it.isEnabled
            }
        }

        inputBox ?: error("找不到消息输入框")

        // 点击输入框获取焦点
        inputBox.click()
        delay(300)

        // 设置文本
        inputBox.setText(message)
        delay(500)

        // 点击发送按钮
        val sendBtn = rootInActiveWindow?.findById(WeChatIds.ID_SEND_BTN)
            ?: rootInActiveWindow?.findByText("发送")
            ?: error("找不到发送按钮")

        sendBtn.click()
        delay(500)

        Log.i(TAG, "消息已发送")
    }

    // ════════════════════════════════════════════════════════
    //  工具方法
    // ════════════════════════════════════════════════════════

    private fun pressBack() {
        performGlobalAction(GLOBAL_ACTION_BACK)
    }

    private fun updateStatus(msg: String) {
        Log.i(TAG, msg)
        onStatusUpdate?.invoke(msg)
    }

    /** 查找页面上第一个 EditText */
    private suspend fun waitForEditText(timeoutMs: Long = 5000): AccessibilityNodeInfo? {
        val deadline = System.currentTimeMillis() + timeoutMs
        while (System.currentTimeMillis() < deadline) {
            val root = rootInActiveWindow
            if (root != null) {
                val editText = root.findFirst {
                    it.className?.toString() == WeChatIds.CLASS_EDIT_TEXT
                }
                if (editText != null) return editText
            }
            delay(300)
        }
        return null
    }

    private fun screenWidth(): Int {
        return resources.displayMetrics.widthPixels
    }

    private fun screenHeight(): Int {
        return resources.displayMetrics.heightPixels
    }

    private val isActive: Boolean
        get() = currentJob?.isActive == true
}
