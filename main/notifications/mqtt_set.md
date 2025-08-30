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
例如：devices/3095dd17-a431-4a49-90e5-2207a31d327e/downlink

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
  - `SetAlarm`（设置闹钟：秒数 + 名称）
  - `CancelAlarm`（取消闹钟：名称）

```json
{
  "type": "iot",
  "commands": [
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "second_from_now": 60, "alarm_name": "测试闹钟" } },
    { "name": "Alarm", "method": "CancelAlarm", "parameters": { "alarm_name": "测试闹钟" } }
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
## 设备控制

服务器下发用于控制设备的系统/通知类消息，发布到设备专属下行主题：
- 发布主题：devices/{client_id}/downlink
- 例如：devices/3095dd17-a431-4a49-90e5-2207a31d327e/downlink
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
- 例如：devices/3095dd17-a431-4a49-90e5-2207a31d327e/uplink
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
    - alarms：数组，当前闹钟列表
  - ImageDisplay：图片显示状态
    - mode：字符串，显示模式（"animated" 或 "static"）

示例：
```json
{"type":"telemetry","online":true,"ts":1755272693,"device_name":"abrobot-1.28tft-wifi","ota_version":"1.2.3","mac":"24:6f:28:aa:bb:cc","client_id":"3095dd17-a431-4a49-90e5-2207a31d327e","battery":{"level":100,"charging":false,"discharging":true},"memory":{"free_internal":49203,"min_free_internal":17567},"wifi":{"rssi":-76},"iot_states":{"Screen":{"theme":"dark","brightness":100},"Speaker":{"volume":80},"Alarm":{"alarms":[]},"ImageDisplay":{"mode":"animated"}}}
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
  - `message_id`: 可选，设备生成的唯一消息ID（用于服务器确认机制）
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

### 4) 带确认机制的 ACK（默认行为）
设备使用 `PublishAck` 方法时，会自动添加 `message_id` 字段，启用服务器确认机制：
```json
{"type":"ack","target":"iot","status":"ok","command":{"name":"Speaker","method":"SetVolume","parameters":{"volume":100}},"request_id":"123","message_id":"msg_1704901234567_1"}
```

## ACK 确认机制（服务器端到设备端）

为了确保 ACK 消息的可靠传输，服务器收到设备的 ACK 消息后应立即回复确认。

- 发布主题：devices/{client_id}/downlink
- QoS：1 或 2
- 触发时机：服务器收到带有 `message_id` 的 ACK 消息后立即回复

### ACK 确认回复格式

```json
{
  "type": "ack_receipt",
  "message_id": "msg_1704901234567_1",
  "received_at": 1704901234,
  "status": "processed"
}
```

字段说明：
- `type`: 固定为 `ack_receipt`
- `message_id`: 对应设备发送的 ACK 消息中的 `message_id`
- `received_at`: 服务器接收消息的 Unix 时间戳（秒）
- `status`: 处理状态
  - `processed`: 消息已成功处理
  - `failed`: 消息处理失败
  - `ignored`: 消息被忽略（如重复消息）

### 设备端行为

1. **发送 ACK**：设备使用 `PublishAck` 方法发送带 `message_id` 的 ACK（自动启用确认机制）
2. **等待确认**：设备在内部跟踪待确认的消息，默认超时时间 10 秒
3. **重试机制**：如果超时未收到确认，自动重试最多 2 次，每次重试间隔 2 秒
4. **确认处理**：收到服务器确认后，从待确认列表中移除消息
5. **失败处理**：超过最大重试次数后，记录错误并放弃该消息

### 优势

- **端到端可靠性**：确保 ACK 消息真正到达服务器并被处理
- **网络故障检测**：能够及时发现网络中断、服务器故障等问题
- **自动重试**：网络抖动时自动恢复，提高系统稳定性
- **监控友好**：提供详细的日志和统计信息，便于问题定位

### 重要改进

- **默认启用确认机制**：所有通过 `PublishAck` 发送的 ACK 消息都将自动包含 `message_id` 并启用确认机制
- **提高可靠性**：彻底解决 "send failed, but message may still reach server" 的不确定性问题
- **服务器端**：必须对所有带 `message_id` 的 ACK 消息回复 `ack_receipt` 确认
- **向后兼容**：现有代码无需修改，自动享有新的可靠性保障
