# 小智ESP32 - 服务端MQTT架构改造文档

**文档版本**: v1.0  
**最后更新**: 2025-10-13  
**目标读者**: 服务端开发人员

---

## 📋 目录

- [1. 概述](#1-概述)
- [2. 架构变更说明](#2-架构变更说明)
- [3. MQTT通信协议](#3-mqtt通信协议)
- [4. 闹钟功能详细规范](#4-闹钟功能详细规范)
- [5. 本地意图识别机制](#5-本地意图识别机制)
- [6. 服务端实现指南](#6-服务端实现指南)
- [7. 附录](#7-附录)

---

## 1. 概述

### 1.1 改造目标

本次架构改造的核心目标是**降低服务端负担，提升系统响应速度**：

1. **本地意图识别优先**：音量、亮度、主题等常用控制由ESP32设备本地识别并执行
2. **精简IoT下发**：服务端仅保留闹钟功能的IoT指令下发
3. **标准化MQTT协议**：统一使用MQTT作为唯一通信协议

### 1.2 关键变化

| 功能类型 | 改造前 | 改造后 |
|---------|--------|--------|
| 音量控制 | 服务端AI解析→下发IoT指令 | ESP32本地识别→执行→通知服务端 |
| 亮度控制 | 服务端AI解析→下发IoT指令 | ESP32本地识别→执行→通知服务端 |
| 主题切换 | 服务端AI解析→下发IoT指令 | ESP32本地识别→执行→通知服务端 |
| **闹钟功能** | 服务端AI解析→下发IoT指令 | ✅ **保持不变** |

### 1.3 架构优势

- ✅ **降低延迟**：本地识别<100ms vs 云端识别300-800ms
- ✅ **减少云端负载**：常用控制无需调用大模型
- ✅ **离线可用**：基础控制功能在网络中断时仍可工作
- ✅ **降低成本**：减少AI API调用次数

---

## 2. 架构变更说明

### 2.1 新架构流程

```
用户语音 → ESP32 
    ├─→ [本地意图识别] → 匹配成功 → 立即执行 → 上报结果到服务端
    └─→ [识别失败] → 转发到服务端AI → 处理复杂意图
```

**闹钟特殊路径**：
```
用户语音 → ESP32 → 服务端AI → 解析闹钟意图 → 下发闹钟IoT指令 → ESP32执行
```

### 2.2 职责划分

#### ESP32设备端
- ✅ 本地意图识别（音量、亮度、主题、显示模式）
- ✅ 本地指令执行
- ✅ 闹钟管理（接收服务端指令）
- ✅ 状态上报

#### 服务端
- ✅ 闹钟意图识别与指令下发
- ✅ 复杂对话处理
- ✅ 接收设备上报的执行结果
- ✅ ACK确认回复
- ⚠️ **不再下发**音量、亮度等常用控制的IoT指令

---

## 3. MQTT通信协议

### 3.1 连接配置

```yaml
Broker: x6bf310e.ala.cn-hangzhou.emqxsl.cn
Port: 8883 (TLS/SSL)
Protocol: MQTT 3.1.1
Username: xiaoqiao
Password: dzkj0000
KeepAlive: 90秒
```

### 3.2 主题结构

#### 3.2.1 Downlink（服务端→设备）

**格式**: `devices/{client_id}/downlink`

- **用途**: 服务端向设备下发控制指令（主要是闹钟）
- **QoS**: 2（确保送达）

#### 3.2.2 Uplink（设备→服务端）

**格式**: `devices/{client_id}/uplink`

- **用途**: 设备上报遥测数据、心跳、事件、本地执行结果
- **QoS**: 0（快速上报）
- **频率**: 心跳60秒，事件立即上报

#### 3.2.3 ACK（设备→服务端）

**格式**: `devices/{client_id}/ack`

- **用途**: 设备执行指令后的结果确认
- **QoS**: 2（可靠送达）
- **超时**: 30秒
- **最大重试**: 3次

### 3.3 ACK确认机制

#### 双向确认流程

```
服务端 --[Downlink]-> 设备: IoT指令
设备 --[ACK]-> 服务端: 执行结果 + message_id
服务端 --[Downlink]-> 设备: ack_receipt + message_id
设备: 确认收到，停止重试
```

#### ACK消息格式

设备发送：
```json
{
  "type": "ack",
  "target": "iot",
  "status": "ok",
  "message_id": "msg_1760377513676_1",
  "command": {...}
}
```

服务端回复：
```json
{
  "type": "ack_receipt",
  "message_id": "msg_1760377513676_1",
  "received_at": 1760377514,
  "status": "processed"
}
```

---

## 4. 闹钟功能详细规范

### 4.1 为什么闹钟保留在服务端

1. **时间解析复杂性**：需要理解"明天早上8点"、"每周一三五"等自然语言
2. **时区计算**：需要考虑用户所在时区
3. **精确性要求高**：闹钟不能因本地识别错误而失效

### 4.2 支持的闹钟类型

#### 4.2.1 每日重复（DAILY）

**示例**: "每天早上7点叫醒我"

**IoT指令**:
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "SetDailyAlarm",
    "parameters": {
      "hour": 7,
      "minute": 0,
      "alarm_name": "起床闹钟"
    }
  }]
}
```

#### 4.2.2 每周重复（WEEKLY）

**示例**: "每周一三五早上7点健身提醒"

**IoT指令**:
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "SetWeeklyAlarm",
    "parameters": {
      "hour": 7,
      "minute": 0,
      "weekdays": 42,
      "alarm_name": "健身提醒"
    }
  }]
}
```

**weekdays位掩码**:
```
bit0=周日=1, bit1=周一=2, bit2=周二=4, bit3=周三=8
bit4=周四=16, bit5=周五=32, bit6=周六=64

示例：
  周一+周三+周五 = 2+8+32 = 42
  工作日 = 2+4+8+16+32 = 62
  周末 = 1+64 = 65
```

#### 4.2.3 工作日（WORKDAYS）

**示例**: "工作日早上6点半叫醒我"

**IoT指令**:
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "SetWorkdaysAlarm",
    "parameters": {
      "hour": 6,
      "minute": 30,
      "alarm_name": "工作日起床"
    }
  }]
}
```

#### 4.2.4 周末（WEEKENDS）

**IoT指令**:
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "SetWeekendsAlarm",
    "parameters": {
      "hour": 9,
      "minute": 0,
      "alarm_name": "周末起床"
    }
  }]
}
```

#### 4.2.5 一次性（ONCE）

**示例**: "30分钟后提醒我关火"

**IoT指令**:
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "SetAlarm",
    "parameters": {
      "second_from_now": 1800,
      "alarm_name": "关火提醒"
    }
  }]
}
```

### 4.3 闹钟管理操作

#### 取消闹钟
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "CancelAlarm",
    "parameters": {
      "alarm_name": "起床闹钟"
    }
  }]
}
```

#### 启用/禁用闹钟
```json
{
  "type": "iot",
  "commands": [{
    "name": "Alarm",
    "method": "EnableAlarm",
    "parameters": {
      "alarm_name": "起床闹钟",
      "enable": false
    }
  }]
}
```

---

## 5. 本地意图识别机制

### 5.1 支持的本地意图

| 意图类型 | 设备 | 方法 | 参数 |
|---------|------|------|------|
| 音量控制 | Speaker | SetVolume | volume: 0-100 |
| 亮度控制 | Screen | SetBrightness | brightness: 0-100 |
| 主题控制 | Screen | SetTheme | theme_name: light/dark |
| 显示模式 | ImageDisplay | SetAnimatedMode/SetStaticMode | - |

### 5.2 识别关键词

**音量控制**:
```
关键词：音量、声音、volume
特殊情况：
  "音量最大" → 100
  "音量最小" / "静音" → 0
  "音量大一点" → +10
  "音量小一点" → -10
  "音量调到50" → 50
```

**亮度控制**:
```
关键词：亮度、屏幕、brightness
特殊情况：
  "亮度最大" → 100
  "亮度最小" → 0
  "亮度大一点" → +10
  "亮度小一点" → -10
```

**主题控制**:
```
白色主题/白天模式 → light
黑色主题/黑夜模式 → dark
```

### 5.3 本地执行后的上报

设备执行本地意图后，通过Uplink主题上报：

```json
{
  "type": "local_intent_result",
  "ts": 1760377514,
  "intent": {
    "type": "VOLUME_CONTROL",
    "device": "Speaker",
    "action": "SetVolume",
    "parameters": {"volume": "50"},
    "confidence": 0.95
  },
  "execution": {
    "status": "ok",
    "duration_ms": 12
  },
  "user_text": "音量调到50"
}
```

---

## 6. 服务端实现指南

### 6.1 MQTT连接（Python示例）

```python
import paho.mqtt.client as mqtt
import ssl

client = mqtt.Client(client_id="server-backend-001")
client.username_pw_set("xiaoqiao", "dzkj0000")
client.tls_set(ca_certs="emqx_ca.crt", tls_version=ssl.PROTOCOL_TLSv1_2)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe("devices/+/uplink", qos=0)
        client.subscribe("devices/+/ack", qos=2)

client.on_connect = on_connect
client.connect("x6bf310e.ala.cn-hangzhou.emqxsl.cn", 8883, 90)
client.loop_start()
```

### 6.2 ACK确认回复（必须实现）

```python
import json, time

def handle_ack_message(topic, payload):
    data = json.loads(payload)
    if data.get('type') != 'ack':
        return
    
    message_id = data.get('message_id')
    client_id = topic.split('/')[1]
    
    # 立即回复ack_receipt
    receipt = {
        "type": "ack_receipt",
        "message_id": message_id,
        "received_at": int(time.time()),
        "status": "processed"
    }
    client.publish(f"devices/{client_id}/downlink", json.dumps(receipt), qos=1)
```

### 6.3 闹钟指令下发

```python
def send_daily_alarm(client_id, hour, minute, alarm_name):
    command = {
        "type": "iot",
        "commands": [{
            "name": "Alarm",
            "method": "SetDailyAlarm",
            "parameters": {
                "hour": hour,
                "minute": minute,
                "alarm_name": alarm_name
            }
        }]
    }
    client.publish(f"devices/{client_id}/downlink", json.dumps(command), qos=2)
```

### 6.4 处理本地意图上报

```python
def handle_uplink_message(topic, payload):
    data = json.loads(payload)
    msg_type = data.get('type')
    
    if msg_type == 'local_intent_result':
        # 记录本地执行结果
        intent = data.get('intent', {})
        execution = data.get('execution', {})
        
        log_intent_execution(
            device_id=topic.split('/')[1],
            intent_type=intent.get('type'),
            status=execution.get('status'),
            user_text=data.get('user_text')
        )
    elif msg_type == 'telemetry':
        # 处理心跳数据
        handle_heartbeat(data)
```

---

## 7. 附录

### 7.1 完整消息示例

详见项目中的`mqtt-test/README.md`文档

### 7.2 测试工具

项目提供了MQTT调试工具：`mqtt-test/mqtt_debug_tool.py`

### 7.3 相关文档

- 闹钟API文档：`main/AlarmClock/ALARM_CLOCK_API.md`
- 闹钟流程图：`main/AlarmClock/ALARM_FLOW.md`
- IoT控制模块：`main/iot/README.md`

### 7.4 注意事项

1. **必须订阅ACK主题并及时回复**：否则设备会持续重试30秒
2. **闹钟指令使用QoS 2**：确保可靠送达
3. **本地意图上报仅供日志记录**：服务端无需对其做出响应
4. **weekdays位掩码**：确保计算正确，错误会导致闹钟在错误日期触发

---

**文档维护**: 如有疑问请联系何高阳
