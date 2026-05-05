package com.example.wechathelper

/**
 * 微信 UI 节点 ID 和类名常量
 *
 * 基于微信 8.0.x 版本。微信更新后可能需要调整。
 * 使用 Layout Inspector 或 uiautomator dump 获取最新 ID。
 *
 * 获取方法：
 *   adb shell uiautomator dump /sdcard/ui.xml
 *   adb pull /sdcard/ui.xml
 *   然后搜索对应页面的 resource-id
 */
object WeChatIds {

    const val WECHAT_PKG = "com.tencent.mm"

    // ── Activity 类名 ──────────────────────────────────────
    /** 微信主界面（消息列表） */
    const val LAUNCHER_UI = "com.tencent.mm.ui.LauncherUI"
    /** 聊天界面 */
    const val CHAT_UI = "com.tencent.mm.ui.chatting.ChattingUI"
    /** 群成员列表界面 */
    const val CHATROOM_MEMBER_UI = "com.tencent.mm.chatroom.ui.ChatroomMemberListUI"
    /** 联系人详情界面 */
    const val CONTACT_INFO_UI = "com.tencent.mm.plugin.profile.ui.ContactInfoUI"
    /** 搜索界面 */
    const val SEARCH_UI = "com.tencent.mm.plugin.fts.ui.FTSMainUI"

    // ── 主界面 ─────────────────────────────────────────────
    /** 搜索按钮 */
    const val ID_SEARCH_BTN = "$WECHAT_PKG:id/kbq"
    /** 搜索输入框 */
    const val ID_SEARCH_INPUT = "$WECHAT_PKG:id/cd7"
    /** 搜索结果列表项标题 */
    const val ID_SEARCH_RESULT_TITLE = "$WECHAT_PKG:id/kpx"

    // ── 聊天界面 ───────────────────────────────────────────
    /** 聊天标题（群名/联系人名） */
    const val ID_CHAT_TITLE = "$WECHAT_PKG:id/okm"
    /** 消息输入框 */
    const val ID_MSG_INPUT = "$WECHAT_PKG:id/b4a"
    /** 发送按钮 */
    const val ID_SEND_BTN = "$WECHAT_PKG:id/b8k"
    /** 聊天界面更多按钮（右上角 ⋯） */
    const val ID_CHAT_MORE_BTN = "$WECHAT_PKG:id/okn"
    /** 切换到键盘输入 */
    const val ID_SWITCH_TO_TEXT = "$WECHAT_PKG:id/b47"

    // ── 群聊信息页 ─────────────────────────────────────────
    /** 群成员头像列表（GridView / RecyclerView） */
    const val ID_MEMBER_GRID = "$WECHAT_PKG:id/hif"
    /** 查看全部成员按钮 */
    const val ID_VIEW_ALL_MEMBERS = "$WECHAT_PKG:id/bhn"
    /** 群成员数量文本 */
    const val ID_MEMBER_COUNT = "$WECHAT_PKG:id/hig"

    // ── 群成员列表页 ───────────────────────────────────────
    /** 成员列表 ListView / RecyclerView */
    const val ID_MEMBER_LIST = "$WECHAT_PKG:id/kx"
    /** 成员昵称 */
    const val ID_MEMBER_NICKNAME = "$WECHAT_PKG:id/ky"

    // ── 联系人详情页 ───────────────────────────────────────
    /** 备注名 */
    const val ID_CONTACT_REMARK = "$WECHAT_PKG:id/hbp"
    /** 微信号 */
    const val ID_CONTACT_WXID = "$WECHAT_PKG:id/hbq"
    /** 发消息按钮 */
    const val ID_SEND_MSG_BTN = "$WECHAT_PKG:id/khj"

    // ── 通用 ───────────────────────────────────────────────
    /** 返回按钮 */
    const val ID_BACK_BTN = "$WECHAT_PKG:id/okl"
    /** ListView 通用 */
    const val ID_LIST_VIEW = "android:id/list"

    // ── 节点类名 ───────────────────────────────────────────
    const val CLASS_LIST_VIEW = "android.widget.ListView"
    const val CLASS_RECYCLER_VIEW = "androidx.recyclerview.widget.RecyclerView"
    const val CLASS_EDIT_TEXT = "android.widget.EditText"
    const val CLASS_TEXT_VIEW = "android.widget.TextView"
    const val CLASS_IMAGE_VIEW = "android.widget.ImageView"
    const val CLASS_BUTTON = "android.widget.Button"
}
