这是一份详细的 MQTT 消息协议文档，用于描述 xiaoqiao 设备如何通过 MQTT 与服务器进行通信。

1：mqtt服务器详情
MQTT 连接信息
连接地址：x6bf310e.ala.cn-hangzhou.emqxsl.cn
MQTT over TLS/SSL 端口：8883
WebSocket over TLS/SSL 端口：8084
CA 证书文件：https://imgbad.xmduzhong.com/i/2025/08/15/p1ogi7.crt
2031.11.10 CA 证书到期

mqtt服务器控制台地址：https://cloud.emqx.com/console/deployments/x6bf310e/overview

mqtt在线调试地址：https://cloud.emqx.com/console/deployments/x6bf310e/online_test

客户端认证
用户名：xiaoqiao
密码：dzkj0000

终端开发语言 Demo（Java）
https://docs.emqx.com/zh/cloud/latest/connect_to_deployments/java_sdk.html

第三方 SDK 推荐（Java）
https://github.com/eclipse-paho/paho.mqtt.java


服务器发送消息，指定客户端设备接受订阅
发布主题为：devices/{client_id}/downlink
例如：devices/719ae1ad-9f2c-4277-9c99-1a317a478979/downlink

# IoT 设备 MQTT 设置消息

## 1) Screen（屏幕主题亮度控制）
- 名称：`Screen`
- 可用方法：
  - `SetTheme`（设置主题：light 或 dark）
  - `SetBrightness`（设置亮度：0-100）

```json
{
  "type": "iot",
  "commands": [
    { "name": "Screen", "method": "SetTheme", "parameters": { "theme_name": "dark" } },
    { "name": "Screen", "method": "SetBrightness", "parameters": { "brightness": 50 } }
  ]
}
```

## 2) Speaker（扬声器控制）
- 名称：`Speaker`
- 可用方法：
  - `SetVolume`（设置音量：0-100）

```json
{
  "type": "iot",
  "commands": [
    { "name": "Speaker", "method": "SetVolume", "parameters": { "volume": 80 } }
  ]
}
```

## 3) Alarm（闹钟管理）
- 名称：`Alarm`
- 可用方法：
  - `SetAlarm`（设置闹钟：统一接口，所有字段必填）
  - `CancelAlarm`（取消闹钟：名称）

### SetAlarm 参数说明（所有字段必填）
- `id`（必填）：字符串，闹钟ID（1-64字符，同ID会覆盖）
- `repeat_type`（必填）：整数，重复类型
  - `0` = ONCE（一次性，触发后自动删除）
  - `1` = DAILY（每天重复）
  - `2` = WEEKLY（每周指定日期重复）
- `seconds`（必填）：整数，ONCE类型使用，从现在开始的秒数；其他类型填0
- `hour`（必填）：整数，DAILY/WEEKLY使用，小时(0-23)；ONCE类型填0
- `minute`（必填）：整数，DAILY/WEEKLY使用，分钟(0-59)；ONCE类型填0
- `repeat_days`（必填）：整数，WEEKLY使用，周几位掩码；其他类型填0
  - bit0 = 周日 (1)
  - bit1 = 周一 (2)
  - bit2 = 周二 (4)
  - bit3 = 周三 (8)
  - bit4 = 周四 (16)
  - bit5 = 周五 (32)
  - bit6 = 周六 (64)
  - 示例：周一三五 = 2+8+32 = 42，工作日 = 62，周末 = 65

### 使用示例

**一次性闹钟（60秒后触发）**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "id": "test_alarm", "repeat_type": 0, "seconds": 60, "hour": 0, "minute": 0, "repeat_days": 0 } }
  ]
}
```

**每日闹钟（每天早上7:30）**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "id": "daily_reminder", "repeat_type": 1, "seconds": 0, "hour": 7, "minute": 30, "repeat_days": 0 } }
  ]
}
```

**每周闹钟（周一、三、五 18:00）**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "id": "fitness_reminder", "repeat_type": 2, "seconds": 0, "hour": 18, "minute": 0, "repeat_days": 42 } }
  ]
}
```

**工作日闹钟（周一到周五 7:00）**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "id": "wakeup_alarm", "repeat_type": 2, "seconds": 0, "hour": 7, "minute": 0, "repeat_days": 62 } }
  ]
}
```
说明：repeat_days=62 即工作日（周一到周五）

**周末闹钟（周六周日 9:00）**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "id": "weekend_exercise", "repeat_type": 2, "seconds": 0, "hour": 9, "minute": 0, "repeat_days": 65 } }
  ]
}
```
说明：repeat_days=65 即周末（周六周日）

**取消闹钟**：
```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "CancelAlarm", "parameters": { "id": "test_alarm" } }
  ]
}
```

## 4) ImageDisplay（图片显示模式）
```json
{
  "type": "iot",
  "commands": [
    { "name": "ImageDisplay", "method": "SetAnimatedMode", "parameters": {} },
    { "name": "ImageDisplay", "method": "SetStaticMode", "parameters": {} },
    { "name": "ImageDisplay", "method": "ToggleDisplayMode", "parameters": {} }
  ]
}
```

## 5) MusicPlayer（音乐播放器界面控制）
- 名称：`MusicPlayer`
- 可用方法：
  - `Show`（显示音乐播放器界面：持续时间 + 歌曲信息）
  - `Hide`（隐藏音乐播放器界面）

```json
{
  "type": "iot",
  "commands": [
    { "name": "MusicPlayer", "method": "Show", "parameters": { "duration_ms": 30000, "song_title": "夜曲", "artist_name": "周杰伦" } },
    { "name": "MusicPlayer", "method": "Hide", "parameters": {} }
  ]
}
```
## 设备控制

服务器下发用于控制设备的系统/通知类消息，发布到设备专属下行主题：
- 发布主题：devices/{client_id}/downlink
- 例如：devices/719ae1ad-9f2c-4277-9c99-1a317a478979/downlink
- QoS：2

### 1) 设备重启（system.reboot）
- 字段说明：
  - type: 固定为 "system"
  - action: 固定为 "reboot"
  - delay_ms: 可选，延时毫秒数，范围 0–10000，默认 1000。超出范围将被裁剪到边界值
- 设备行为：
  - 接收到消息后，设备会显示“即将重启...”提示，随后按 delay_ms 延时执行重启

示例：
```json
{"type":"system","action":"reboot","delay_ms":1000}
```

### 2) 通知消息（notify）
- 字段说明：
  - type: 固定为 "notify"
  - title: 可选，通知标题（字符串）
  - body: 可选，通知正文（字符串）
- 设备行为：
  - 设备将把 title 与 body 组合为一条消息（若两者同时存在，以换行分隔），在屏幕上显示约 10 秒
  - 若仅提供其一，则显示提供的字段内容；若二者均缺省，则忽略该消息

示例：
```json
{"type":"notify","title":"标题","body":"内容"}
```

## 设备上报（uplink 遥测）

- 发布主题：devices/{client_id}/uplink
- 例如：devices/719ae1ad-9f2c-4277-9c99-1a317a478979/uplink
- 上报频率：每 30 秒（心跳任务定期上报）
- QoS：0

字段说明：
- type：固定为 "telemetry"
- online：布尔值，当前是否在线（恒为 true）
- ts：整数，Unix 时间戳（秒）
- device_name：字符串，设备名称（编译期板卡名）
- ota_version：字符串，当前固件版本
- mac：字符串，设备 MAC 地址（格式：xx:xx:xx:xx:xx:xx）
- client_id：字符串，MQTT 客户端 ID
- battery：电池信息对象
  - level：整数，电量百分比
  - charging：布尔值，是否在充电
  - discharging：布尔值，是否在放电
- memory：内存信息对象（单位：字节）
  - free_internal：当前可用内部 SRAM 大小
  - min_free_internal：运行以来内部 SRAM 最小可用值
- wifi：Wi-Fi 信息对象
  - rssi：整数，当前 RSSI（dBm，通常为负值）
- iot_states（可选）：IoT 设备状态对象
  - Screen：屏幕控制状态
    - theme：字符串，当前主题（"light" 或 "dark"）
    - brightness：整数，当前亮度（0-100）
  - Speaker：扬声器状态
    - volume：整数，当前音量（0-100）
  - Alarm：闹钟状态
    - alarms：数组，当前闹钟列表（每个闹钟对象包含以下字段）
      - id：字符串，闹钟ID
      - time：整数，下次触发的Unix时间戳（秒）
      - repeat_type：整数，重复类型（0=ONCE, 1=DAILY, 2=WEEKLY）
      - repeat_days：整数，周几掩码（WEEKLY类型使用）
  - ImageDisplay：图片显示状态
    - mode：字符串，显示模式（"animated" 或 "static"）
  - MusicPlayer：音乐播放器状态
    - visible：布尔值，界面是否可见
    - song_title：字符串，当前歌曲标题（如果显示中）
    - artist_name：字符串，当前艺术家名称（如果显示中）

示例（无闹钟）：
```json
{"type":"telemetry","online":true,"ts":1755272693,"device_name":"abrobot-1.28tft-wifi","ota_version":"1.2.3","mac":"24:6f:28:aa:bb:cc","client_id":"3095dd17-a431-4a49-90e5-2207a31d327e","battery":{"level":100,"charging":false,"discharging":true},"memory":{"free_internal":49203,"min_free_internal":17567},"wifi":{"rssi":-76},"iot_states":{"Screen":{"theme":"dark","brightness":100},"Speaker":{"volume":80},"Alarm":{"alarms":[]},"ImageDisplay":{"mode":"animated"},"MusicPlayer":{"visible":false,"song_title":"","artist_name":""}}}
```

示例（包含闹钟）：
```json
{"type":"telemetry","online":true,"ts":1760371234,"device_name":"abrobot-1.28tft-wifi","ota_version":"1.2.0","mac":"b8:f8:62:f0:b3:58","client_id":"719ae1ad-9f2c-4277-9c99-1a317a478979","battery":{"level":95,"charging":false,"discharging":true},"memory":{"free_internal":45678,"min_free_internal":15234},"wifi":{"rssi":-68},"iot_states":{"Screen":{"theme":"dark","brightness":100},"Speaker":{"volume":80},"Alarm":{"alarms":[{"name":"daily_morning","time":1760414400,"repeat_type":1,"repeat_days":0},{"name":"weekly_meeting","time":1760457600,"repeat_type":2,"repeat_days":42}]},"ImageDisplay":{"mode":"animated"},"MusicPlayer":{"visible":false,"song_title":"","artist_name":""}}}
```

## 指令执行结果上报（ACK）

- 发布主题：devices/{client_id}/ack
- QoS：2
- 触发时机：设备完成执行服务端下发的 `system` / `notify` / `iot` 指令后立即上报
- 通用字段：
  - `type`: 固定为 `ack`
  - `target`: `system` | `notify` | `iot`
  - `status`: `ok` | `error`
  - `request_id`: 可选，透传下行中的 `request_id`（如果存在）
  - `ts`: 可选，Unix 时间戳（秒）

### 1) iot 指令 ACK
成功示例：
```json
{"type":"ack","target":"iot","status":"ok","command":{"name":"Speaker","method":"SetVolume","parameters":{"volume":100}},"request_id":"123"}
```
失败示例（参数缺失等）：
```json
{"type":"ack","target":"iot","status":"error","command":{"name":"Speaker","method":"SetVolume","parameters":{"volume":100}},"error":"invalid volume","request_id":"123"}
```

### 2) system 指令 ACK（重启）
```json
{"type":"ack","target":"system","action":"reboot","status":"ok","delay_ms":1000,"request_id":"abc"}
```

### 3) notify 指令 ACK
```json
{"type":"ack","target":"notify","status":"ok","request_id":"n1"}
```

**说明**：
- ACK消息发送后，服务器无需回复确认
- 依赖MQTT QoS=2保证消息传输可靠性
- 设备发送ACK后即可继续处理下一个任务

---

## 附录：闹钟功能快速测试

### 使用在线MQTT调试工具测试

访问：https://cloud.emqx.com/console/deployments/x6bf310e/online_test

**连接参数**：
- 用户名：`xiaoqiao`
- 密码：`dzkj0000`

**发布主题**：`devices/719ae1ad-9f2c-4277-9c99-1a317a478979/downlink`

### 快速测试命令集

**1. 一次性闹钟（30秒后触发）**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"SetAlarm","parameters":{"id":"test_once","repeat_type":0,"seconds":30,"hour":0,"minute":0,"repeat_days":0}}]}
```

**2. 每日闹钟（每天早上8:00）**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"SetAlarm","parameters":{"id":"test_daily","repeat_type":1,"seconds":0,"hour":8,"minute":0,"repeat_days":0}}]}
```

**3. 每周一三五闹钟（18:00）**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"SetAlarm","parameters":{"id":"test_weekly","repeat_type":2,"seconds":0,"hour":18,"minute":0,"repeat_days":42}}]}
```

**4. 工作日闹钟（周一到周五 7:00）**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"SetAlarm","parameters":{"id":"test_workdays","repeat_type":2,"seconds":0,"hour":7,"minute":0,"repeat_days":62}}]}
```

**5. 周末闹钟（周六周日 9:00）**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"SetAlarm","parameters":{"id":"test_weekends","repeat_type":2,"seconds":0,"hour":9,"minute":0,"repeat_days":65}}]}
```

**6. 取消闹钟**
```json
{"type":"iot","commands":[{"name":"Alarm","method":"CancelAlarm","parameters":{"id":"test_once"}}]}
```

### 周几掩码计算参考

| 星期 | 位值 | 十进制 |
|------|------|--------|
| 周日 | bit0 | 1      |
| 周一 | bit1 | 2      |
| 周二 | bit2 | 4      |
| 周三 | bit3 | 8      |
| 周四 | bit4 | 16     |
| 周五 | bit5 | 32     |
| 周六 | bit6 | 64     |

**常用组合**：
- 周一三五：2 + 8 + 32 = **42**
- 周二四：4 + 16 = **20**
- 周末：1 + 64 = **65**
- 工作日：2 + 4 + 8 + 16 + 32 = **62**
- 每天：1 + 2 + 4 + 8 + 16 + 32 + 64 = **127**

---

**文档版本**: v2.0  
**最后更新**: 2025-10-14  
**重大变更**: 移除ACK确认机制，设备发送ACK后无需等待服务器回复
