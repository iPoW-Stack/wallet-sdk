package com.example.wechathelper.model

/**
 * 任务配置
 *
 * @param groupName     目标群名称
 * @param message       要发送的消息内容
 * @param excludeSelf   是否排除自己
 * @param delayMs       每条消息之间的延迟（毫秒），防止风控
 */
data class TaskConfig(
    val groupName: String,
    val message: String,
    val excludeSelf: Boolean = true,
    val delayMs: Long = 3000L
)
