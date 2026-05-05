package com.example.wechathelper.model

/**
 * 群成员数据模型
 *
 * @param nickname  微信昵称
 * @param remark    备注名（如果有）
 * @param displayName 群内显示名（优先备注 > 群昵称 > 微信昵称）
 * @param isSelf    是否是自己
 */
data class GroupMember(
    val nickname: String,
    val remark: String = "",
    val displayName: String = "",
    val isSelf: Boolean = false
) {
    /** 用于发消息时搜索的名称：优先备注，其次昵称 */
    val searchName: String get() = remark.ifEmpty { nickname }

    /** 显示用名称 */
    val showName: String get() = when {
        displayName.isNotEmpty() -> displayName
        remark.isNotEmpty() -> "$nickname（备注: $remark）"
        else -> nickname
    }
}
