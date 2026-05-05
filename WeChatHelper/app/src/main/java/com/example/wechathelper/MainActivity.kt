package com.example.wechathelper

import android.accessibilityservice.AccessibilityServiceInfo
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityManager
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.example.wechathelper.model.GroupMember
import com.example.wechathelper.model.TaskConfig
import com.example.wechathelper.service.FloatingStatusService
import com.example.wechathelper.service.WeChatAccessibilityService

class MainActivity : AppCompatActivity() {

    private val TAG = "WeChatHelper"

    private lateinit var tvServiceStatus: TextView
    private lateinit var btnEnableService: Button
    private lateinit var etGroupName: EditText
    private lateinit var btnCollectMembers: Button
    private lateinit var rvMembers: RecyclerView
    private lateinit var tvMemberCount: TextView
    private lateinit var etMessage: EditText
    private lateinit var etDelay: EditText
    private lateinit var cbExcludeSelf: CheckBox
    private lateinit var btnSelectAll: Button
    private lateinit var btnDeselectAll: Button
    private lateinit var btnSendMessages: Button
    private lateinit var btnCancel: Button
    private lateinit var tvStatus: TextView

    private val memberAdapter = MemberAdapter()
    private var collectedMembers = mutableListOf<GroupMember>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        initViews()
        setupListeners()
    }

    override fun onResume() {
        super.onResume()
        updateServiceStatus()
    }

    private fun initViews() {
        tvServiceStatus = findViewById(R.id.tvServiceStatus)
        btnEnableService = findViewById(R.id.btnEnableService)
        etGroupName = findViewById(R.id.etGroupName)
        btnCollectMembers = findViewById(R.id.btnCollectMembers)
        rvMembers = findViewById(R.id.rvMembers)
        tvMemberCount = findViewById(R.id.tvMemberCount)
        etMessage = findViewById(R.id.etMessage)
        etDelay = findViewById(R.id.etDelay)
        cbExcludeSelf = findViewById(R.id.cbExcludeSelf)
        btnSelectAll = findViewById(R.id.btnSelectAll)
        btnDeselectAll = findViewById(R.id.btnDeselectAll)
        btnSendMessages = findViewById(R.id.btnSendMessages)
        btnCancel = findViewById(R.id.btnCancel)
        tvStatus = findViewById(R.id.tvStatus)

        rvMembers.layoutManager = LinearLayoutManager(this)
        rvMembers.adapter = memberAdapter
    }

    private fun setupListeners() {
        // 开启无障碍服务
        btnEnableService.setOnClickListener {
            if (isAccessibilityServiceEnabled()) {
                toast("无障碍服务已开启")
            } else {
                AlertDialog.Builder(this)
                    .setTitle("开启无障碍服务")
                    .setMessage(
                        "请在设置中找到「微信群助手」并开启无障碍服务。\n\n" +
                        "路径：设置 → 无障碍 → 已下载的服务 → 微信群助手 → 开启"
                    )
                    .setPositiveButton("去设置") { _, _ ->
                        startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))
                    }
                    .setNegativeButton("取消", null)
                    .show()
            }
        }

        // 获取群成员
        btnCollectMembers.setOnClickListener {
            val groupName = etGroupName.text.toString().trim()
            if (groupName.isEmpty()) {
                toast("请输入群名称")
                return@setOnClickListener
            }
            if (!isAccessibilityServiceEnabled()) {
                toast("请先开启无障碍服务")
                return@setOnClickListener
            }

            val service = WeChatAccessibilityService.instance
            if (service == null) {
                toast("无障碍服务未就绪，请重新开启")
                return@setOnClickListener
            }

            // 设置回调
            service.onStatusUpdate = { msg ->
                runOnUiThread { tvStatus.text = msg }
            }
            service.onMembersCollected = { members ->
                runOnUiThread {
                    collectedMembers.clear()
                    collectedMembers.addAll(members)
                    memberAdapter.setMembers(members)
                    tvMemberCount.text = "群成员: ${members.size} 人"
                    tvMemberCount.visibility = View.VISIBLE
                }
            }
            service.onTaskComplete = { success, message ->
                runOnUiThread {
                    tvStatus.text = if (success) "✅ $message" else "❌ $message"
                    btnCollectMembers.isEnabled = true
                }
            }

            btnCollectMembers.isEnabled = false
            tvStatus.text = "⏳ 正在获取群成员，请切换到微信..."

            // 启动悬浮窗
            startFloatingStatus()

            // 开始任务（3秒后执行，给用户时间切换到微信）
            service.startCollectMembers(groupName)

            // 切换到微信
            launchWeChat()
        }

        // 全选 / 取消全选
        btnSelectAll.setOnClickListener { memberAdapter.selectAll() }
        btnDeselectAll.setOnClickListener { memberAdapter.deselectAll() }

        // 发送消息
        btnSendMessages.setOnClickListener {
            val message = etMessage.text.toString().trim()
            if (message.isEmpty()) {
                toast("请输入消息内容")
                return@setOnClickListener
            }

            val selectedMembers = memberAdapter.getSelectedMembers()
            if (selectedMembers.isEmpty()) {
                toast("请选择至少一个成员")
                return@setOnClickListener
            }

            if (!isAccessibilityServiceEnabled()) {
                toast("请先开启无障碍服务")
                return@setOnClickListener
            }

            val service = WeChatAccessibilityService.instance ?: run {
                toast("无障碍服务未就绪")
                return@setOnClickListener
            }

            val delayMs = try {
                ((etDelay.text.toString().toDoubleOrNull() ?: 3.0) * 1000).toLong()
            } catch (_: Exception) { 3000L }

            val config = TaskConfig(
                groupName = etGroupName.text.toString().trim(),
                message = message,
                excludeSelf = cbExcludeSelf.isChecked,
                delayMs = delayMs
            )

            // 确认对话框
            AlertDialog.Builder(this)
                .setTitle("确认发送")
                .setMessage(
                    "将向 ${selectedMembers.size} 人发送消息：\n\n" +
                    "「$message」\n\n" +
                    "每条间隔 ${delayMs / 1000} 秒\n" +
                    "预计耗时 ${selectedMembers.size * delayMs / 1000} 秒"
                )
                .setPositiveButton("开始发送") { _, _ ->
                    service.onStatusUpdate = { msg ->
                        runOnUiThread { tvStatus.text = msg }
                        // 更新悬浮窗
                        FloatingStatusService.instance?.updateText(msg)
                    }
                    service.onTaskComplete = { success, msg ->
                        runOnUiThread {
                            tvStatus.text = if (success) "✅ $msg" else "❌ $msg"
                            btnSendMessages.isEnabled = true
                            stopFloatingStatus()
                        }
                    }

                    btnSendMessages.isEnabled = false
                    startFloatingStatus()
                    service.startSendMessages(selectedMembers, config)
                    launchWeChat()
                }
                .setNegativeButton("取消", null)
                .show()
        }

        // 取消任务
        btnCancel.setOnClickListener {
            WeChatAccessibilityService.instance?.cancelTask()
            btnCollectMembers.isEnabled = true
            btnSendMessages.isEnabled = true
            stopFloatingStatus()
        }
    }

    // ── 工具方法 ───────────────────────────────────────────

    private fun isAccessibilityServiceEnabled(): Boolean {
        val am = getSystemService(Context.ACCESSIBILITY_SERVICE) as AccessibilityManager
        val enabledServices = am.getEnabledAccessibilityServiceList(
            AccessibilityServiceInfo.FEEDBACK_GENERIC
        )
        return enabledServices.any {
            it.resolveInfo.serviceInfo.packageName == packageName
        }
    }

    private fun updateServiceStatus() {
        val enabled = isAccessibilityServiceEnabled()
        tvServiceStatus.text = if (enabled) {
            "✅ 无障碍服务已开启"
        } else {
            "❌ 无障碍服务未开启"
        }
        tvServiceStatus.setTextColor(
            if (enabled) 0xFF4CAF50.toInt() else 0xFFF44336.toInt()
        )
        btnEnableService.text = if (enabled) "已开启" else "去开启"
    }

    private fun launchWeChat() {
        val intent = packageManager.getLaunchIntentForPackage(WeChatIds.WECHAT_PKG)
        if (intent != null) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            startActivity(intent)
        } else {
            toast("未安装微信")
        }
    }

    private fun startFloatingStatus() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this)) {
            // 没有悬浮窗权限，跳过
            return
        }
        startService(Intent(this, FloatingStatusService::class.java))
    }

    private fun stopFloatingStatus() {
        stopService(Intent(this, FloatingStatusService::class.java))
    }

    private fun toast(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }

    // ════════════════════════════════════════════════════════
    //  成员列表 Adapter
    // ════════════════════════════════════════════════════════

    inner class MemberAdapter : RecyclerView.Adapter<MemberAdapter.VH>() {

        private val items = mutableListOf<Pair<GroupMember, Boolean>>() // member, selected

        fun setMembers(members: List<GroupMember>) {
            items.clear()
            items.addAll(members.map { it to true }) // 默认全选
            notifyDataSetChanged()
        }

        fun selectAll() {
            for (i in items.indices) items[i] = items[i].first to true
            notifyDataSetChanged()
        }

        fun deselectAll() {
            for (i in items.indices) items[i] = items[i].first to false
            notifyDataSetChanged()
        }

        fun getSelectedMembers(): List<GroupMember> {
            return items.filter { it.second }.map { it.first }
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context)
                .inflate(R.layout.item_member, parent, false)
            return VH(view)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val (member, selected) = items[position]
            holder.cbSelected.isChecked = selected
            holder.tvName.text = member.showName
            holder.tvIndex.text = "${position + 1}"

            holder.cbSelected.setOnCheckedChangeListener { _, isChecked ->
                val pos = holder.adapterPosition
                if (pos != RecyclerView.NO_POSITION) {
                    items[pos] = items[pos].first to isChecked
                }
            }
            holder.itemView.setOnClickListener {
                val pos = holder.adapterPosition
                if (pos != RecyclerView.NO_POSITION) {
                    val newState = !items[pos].second
                    items[pos] = items[pos].first to newState
                    holder.cbSelected.isChecked = newState
                }
            }
        }

        override fun getItemCount() = items.size

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val cbSelected: CheckBox = view.findViewById(R.id.cbSelected)
            val tvName: TextView = view.findViewById(R.id.tvName)
            val tvIndex: TextView = view.findViewById(R.id.tvIndex)
        }
    }
}
