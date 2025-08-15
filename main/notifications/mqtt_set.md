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
{"type":"telemetry","online":true,"ts":1755272693,"battery":{"level":100,"charging":false,"discharging":true},"memory":{"free_internal":49203,"min_free_internal":17567},"wifi":{"rssi":-76},"iot_states":{"Screen":{"theme":"dark","brightness":100},"Speaker":{"volume":80},"Alarm":{"alarms":[]},"ImageDisplay":{"mode":"animated"}}}
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
