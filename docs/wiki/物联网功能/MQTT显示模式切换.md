# MQTT 显示模式切换

## 功能说明

abrobot-1.28tft-wifi 板子支持通过 MQTT 下发命令切换显示模式，包括动画模式、静态模式和表情包模式。

## 支持的显示模式

1. **动画模式 (animated)**: 根据音频状态自动播放动画
2. **静态模式 (static)**: 显示静态 logo 图片
3. **表情包模式 (emoticon/emotion)**: 根据 AI 回复情绪显示表情包

## MQTT 命令格式

### 主题 (Topic)

使用音乐控制的下行主题（与音乐播放器共用）

### 消息格式

```json
{
  "command": "set_display_mode",
  "params": {
    "mode": "emoticon"
  }
}
```

### 参数说明

- `command`: 固定为 `"set_display_mode"`
- `params.mode`: 显示模式，可选值：
  - `"animated"`: 切换到动画模式
  - `"static"`: 切换到静态模式
  - `"emoticon"` 或 `"emotion"`: 切换到表情包模式

## 使用示例

### 示例 1: 切换到表情包模式

```json
{
  "command": "set_display_mode",
  "params": {
    "mode": "emoticon"
  }
}
```

### 示例 2: 切换到动画模式

```json
{
  "command": "set_display_mode",
  "params": {
    "mode": "animated"
  }
}
```

### 示例 3: 切换到静态模式

```json
{
  "command": "set_display_mode",
  "params": {
    "mode": "static"
  }
}
```

## Python 示例代码

```python
import json
import paho.mqtt.client as mqtt

# MQTT 配置
MQTT_BROKER = "your-mqtt-broker.com"
MQTT_PORT = 1883
MQTT_USERNAME = "your-username"
MQTT_PASSWORD = "your-password"
DOWNLINK_TOPIC = "your/device/downlink/topic"

# 创建 MQTT 客户端
client = mqtt.Client()
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
client.connect(MQTT_BROKER, MQTT_PORT)

# 切换到表情包模式
message = {
    "command": "set_display_mode",
    "params": {
        "mode": "emoticon"
    }
}

client.publish(DOWNLINK_TOPIC, json.dumps(message))
print("已发送切换到表情包模式命令")

client.disconnect()
```

## 注意事项

1. 切换到表情包模式前，确保表情包资源已成功加载（启动时会自动从 `/resources/emoticons/` 加载）
2. 切换到静态模式需要 logo 图片已下载完成
3. 模式切换会自动保存到系统配置，设备重启后会恢复上次的模式
4. 支持的表情包类型：
   - happy (开心)
   - sad (悲伤)
   - angry (生气)
   - surprised (惊讶)
   - calm (平静)
   - shy (害羞)

## 相关接口

除了 MQTT 控制外，还可以通过以下方式切换显示模式：

### IoT Thing 接口

通过 IoT Thing 系统调用 `ImageDisplay` 设备的方法：

- `SetAnimatedMode`: 设置为动画模式
- `SetStaticMode`: 设置为静态模式
- `SetEmoticonMode`: 设置为表情包模式
- `ToggleDisplayMode`: 循环切换模式

### 本地语音控制

支持通过语音命令切换（需配置本地意图识别）：
- "切换到表情包模式"
- "静态模式"
- "动态壁纸"
- 等

## 日志输出

成功切换模式时会输出相应日志：

```
I (12345) abrobot-1.28tft-wifi: 收到音乐控制命令: set_display_mode, 参数: {"mode":"emoticon"}
I (12346) abrobot-1.28tft-wifi: MQTT切换到表情包模式
I (12347) ImageDisplay: 已设置为表情包模式
I (12348) abrobot-1.28tft-wifi: 显示模式切换成功
```
