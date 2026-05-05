# 微信群助手 (WeChatHelper)

基于 Android 无障碍服务 (AccessibilityService) 实现的微信群管理工具。

## 功能

1. **获取群成员列表** — 自动打开指定群聊，遍历成员列表，收集每个成员的昵称/备注
2. **一对一私聊发送** — 逐个搜索成员，打开私聊窗口，发送指定消息

## 工作原理

```
用户输入群名 → 打开微信 → 搜索群名 → 进入群聊
→ 点击右上角 ⋯ → 查看全部成员 → 滚动遍历收集
→ 返回成员列表给用户

用户确认发送 → 逐个搜索成员名 → 打开私聊
→ 输入消息 → 点击发送 → 等待间隔 → 下一个
```

## 使用步骤

### 1. 安装应用

```bash
cd WeChatHelper
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### 2. 开启无障碍服务

打开应用 → 点击「去开启」→ 在系统设置中找到「微信群助手」→ 开启

### 3. 获取群成员

1. 输入群名称（需精确匹配）
2. 点击「获取群成员列表」
3. 应用会自动切换到微信并执行操作
4. 完成后返回应用查看成员列表

### 4. 发送消息

1. 在成员列表中勾选要发送的成员
2. 输入消息内容
3. 设置消息间隔（建议 ≥ 3 秒）
4. 点击「开始发送」
5. 确认后自动执行

## 注意事项

### 微信版本适配

`WeChatIds.kt` 中的 UI 节点 ID 基于微信 8.0.x 版本。如果微信更新后功能失效，需要更新这些 ID。

获取最新 ID 的方法：

```bash
# 在目标页面执行
adb shell uiautomator dump /sdcard/ui.xml
adb pull /sdcard/ui.xml

# 搜索对应元素的 resource-id
```

### 风控建议

- 消息间隔建议 ≥ 3 秒
- 单次发送人数建议 ≤ 50 人
- 避免频繁使用
- 消息内容避免敏感词

### 权限说明

- **无障碍服务**：读取微信界面元素、模拟点击操作（核心功能）
- **悬浮窗**（可选）：在微信操作时显示进度状态

## 项目结构

```
WeChatHelper/app/src/main/java/com/example/wechathelper/
├── MainActivity.kt                    # 主界面
├── WeChatIds.kt                       # 微信 UI 节点 ID 常量
├── model/
│   ├── GroupMember.kt                 # 群成员数据模型
│   └── TaskConfig.kt                  # 任务配置
├── service/
│   ├── WeChatAccessibilityService.kt  # 核心无障碍服务
│   └── FloatingStatusService.kt       # 悬浮窗状态
└── util/
    └── NodeHelper.kt                  # 节点查找/操作工具
```

## 故障排查

### 无障碍服务无法开启

部分手机需要在「开发者选项」中关闭「USB 调试安全设置」。

### 搜索群名失败

- 确认群名完全匹配（包括空格、特殊字符）
- 确认微信已登录且在主界面

### 成员列表不完整

- 大群（500人）可能需要较长时间滚动
- 如果中途被打断，重新执行即可

### 发送消息失败

- 检查是否被对方拉黑/删除
- 检查消息间隔是否太短
- 查看日志：`adb logcat | grep WeChatA11y`
