# 闹钟功能服务端对接文档

## 概述

本文档详细说明如何将ESP32闹钟功能与云端服务器集成，包括API协议、消息格式、错误处理等内容。

## 架构说明

```
云端服务器 <---> ESP32设备 <---> AlarmManager
    ↓            ↓                ↓
  REST API    WebSocket/MQTT   本地闹钟管理
   JSON         JSON             C++ API
```

**通信流程**：
1. 服务端通过WebSocket/MQTT发送闹钟设置指令
2. ESP32解析JSON并调用AlarmManager API
3. 闹钟触发时，ESP32发送通知给服务端
4. 服务端可查询闹钟状态

## JSON 消息协议

### 1. 设置闹钟（服务端 → ESP32）

#### 1.1 设置一次性闹钟

**消息类型**: `set_alarm`

```json
{
  "type": "set_alarm",
  "data": {
    "name": "reminder_001",
    "seconds_from_now": 3600,
    "repeat_type": "once"
  }
}
```

**字段说明**：
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | ✅ | 固定值 "set_alarm" |
| data.name | string | ✅ | 闹钟名称，1-64字符，唯一标识 |
| data.seconds_from_now | int | ✅ | 从现在开始多少秒后触发（>0） |
| data.repeat_type | string | ❌ | 重复类型，默认"once" |

**ESP32处理代码**：
```cpp
void handleSetAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    int seconds = data["seconds_from_now"].as<int>();
    
    alarmManager.SetAlarm(seconds, name, RepeatType::ONCE);
    
    // 返回成功响应
    sendResponse("set_alarm_response", true, "Alarm set successfully");
}
```

#### 1.2 设置每日重复闹钟

**消息类型**: `set_daily_alarm`

```json
{
  "type": "set_daily_alarm",
  "data": {
    "name": "morning_alarm",
    "hour": 7,
    "minute": 30
  }
}
```

**字段说明**：
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | ✅ | 固定值 "set_daily_alarm" |
| data.name | string | ✅ | 闹钟名称 |
| data.hour | int | ✅ | 小时 (0-23) |
| data.minute | int | ✅ | 分钟 (0-59) |

**ESP32处理代码**：
```cpp
void handleSetDailyAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    int hour = data["hour"].as<int>();
    int minute = data["minute"].as<int>();
    
    alarmManager.SetDailyAlarm(name, hour, minute);
    
    sendResponse("set_daily_alarm_response", true, "Daily alarm set");
}
```

#### 1.3 设置每周重复闹钟

**消息类型**: `set_weekly_alarm`

```json
{
  "type": "set_weekly_alarm",
  "data": {
    "name": "gym_reminder",
    "hour": 18,
    "minute": 0,
    "weekdays": [1, 3, 5]
  }
}
```

**字段说明**：
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | ✅ | 固定值 "set_weekly_alarm" |
| data.name | string | ✅ | 闹钟名称 |
| data.hour | int | ✅ | 小时 (0-23) |
| data.minute | int | ✅ | 分钟 (0-59) |
| data.weekdays | array | ✅ | 星期数组，0=周日, 1=周一...6=周六 |

**weekdays编码对照**：
```
0 = 周日 (Sunday)
1 = 周一 (Monday)
2 = 周二 (Tuesday)
3 = 周三 (Wednesday)
4 = 周四 (Thursday)
5 = 周五 (Friday)
6 = 周六 (Saturday)
```

**ESP32处理代码**：
```cpp
void handleSetWeeklyAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    int hour = data["hour"].as<int>();
    int minute = data["minute"].as<int>();
    
    // 将数组转换为位掩码
    uint8_t weekdays = 0;
    JsonArray days = data["weekdays"].as<JsonArray>();
    for(JsonVariant day : days) {
        int d = day.as<int>();
        if(d >= 0 && d <= 6) {
            weekdays |= (1 << d);
        }
    }
    
    alarmManager.SetWeeklyAlarm(name, hour, minute, weekdays);
    
    sendResponse("set_weekly_alarm_response", true, "Weekly alarm set");
}
```

#### 1.4 设置工作日闹钟

**消息类型**: `set_workdays_alarm`

```json
{
  "type": "set_workdays_alarm",
  "data": {
    "name": "work_start",
    "hour": 7,
    "minute": 0
  }
}
```

**ESP32处理代码**：
```cpp
void handleSetWorkdaysAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    int hour = data["hour"].as<int>();
    int minute = data["minute"].as<int>();
    
    alarmManager.SetWorkdaysAlarm(name, hour, minute);
    
    sendResponse("set_workdays_alarm_response", true, "Workdays alarm set");
}
```

#### 1.5 设置周末闹钟

**消息类型**: `set_weekends_alarm`

```json
{
  "type": "set_weekends_alarm",
  "data": {
    "name": "weekend_wakeup",
    "hour": 9,
    "minute": 30
  }
}
```

### 2. 管理闹钟（服务端 → ESP32）

#### 2.1 启用/禁用闹钟

**消息类型**: `enable_alarm`

```json
{
  "type": "enable_alarm",
  "data": {
    "name": "morning_alarm",
    "enabled": true
  }
}
```

**字段说明**：
| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| data.name | string | ✅ | 闹钟名称 |
| data.enabled | boolean | ✅ | true=启用, false=禁用 |

**ESP32处理代码**：
```cpp
void handleEnableAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    bool enabled = data["enabled"].as<bool>();
    
    alarmManager.EnableAlarm(name, enabled);
    
    sendResponse("enable_alarm_response", true, 
                 enabled ? "Alarm enabled" : "Alarm disabled");
}
```

#### 2.2 取消闹钟

**消息类型**: `cancel_alarm`

```json
{
  "type": "cancel_alarm",
  "data": {
    "name": "morning_alarm"
  }
}
```

**ESP32处理代码**：
```cpp
void handleCancelAlarm(const JsonObject& data) {
    std::string name = data["name"].as<std::string>();
    
    alarmManager.CancelAlarm(name);
    
    sendResponse("cancel_alarm_response", true, "Alarm cancelled");
}
```

#### 2.3 查询闹钟列表

**请求消息**: `list_alarms`

```json
{
  "type": "list_alarms"
}
```

**响应消息**：

```json
{
  "type": "list_alarms_response",
  "success": true,
  "data": {
    "alarms": [
      {
        "name": "morning_alarm",
        "time": 1697184000,
        "repeat_type": "daily",
        "repeat_days": 127,
        "enabled": true,
        "next_trigger": "2025-10-13 07:30:00"
      },
      {
        "name": "gym_reminder",
        "time": 1697270400,
        "repeat_type": "weekly",
        "repeat_days": 42,
        "enabled": true,
        "next_trigger": "2025-10-15 18:00:00"
      }
    ],
    "count": 2
  }
}
```

**ESP32处理代码**：
```cpp
void handleListAlarms() {
    // 构建闹钟列表JSON
    DynamicJsonDocument doc(2048);
    doc["type"] = "list_alarms_response";
    doc["success"] = true;
    
    JsonArray alarms = doc.createNestedArray("data")["alarms"];
    
    // 遍历所有闹钟（需要添加getter方法）
    for(const auto& alarm : alarmManager.GetAllAlarms()) {
        JsonObject obj = alarms.createNestedObject();
        obj["name"] = alarm.name;
        obj["time"] = alarm.time;
        obj["repeat_type"] = repeatTypeToString(alarm.repeat_type);
        obj["repeat_days"] = alarm.repeat_days;
        obj["enabled"] = alarm.enabled;
        obj["next_trigger"] = formatTime(alarm.time);
    }
    
    doc["data"]["count"] = alarms.size();
    
{{ ... }}
    // 发送响应
    sendJsonMessage(doc);
}
```

### 3. 闹钟触发通知（ESP32 → 服务端）

#### 3.1 闹钟触发事件

**消息类型**: `alarm_triggered`

```json
{
  "type": "alarm_triggered",
  "data": {
    "name": "morning_alarm",
    "triggered_at": 1697184000,
    "repeat_type": "daily",
    "next_trigger": 1697270400
  }
}
```

**字段说明**：
| 字段 | 类型 | 说明 |
|------|------|------|
| type | string | 固定值 "alarm_triggered" |
| data.name | string | 触发的闹钟名称 |
| data.triggered_at | long | 触发时间戳（Unix秒） |
| data.repeat_type | string | 重复类型 |
| data.next_trigger | long | 下次触发时间（重复闹钟） |

**ESP32发送代码**：
```cpp
void notifyAlarmTriggered(const Alarm& alarm, time_t next_time) {
    DynamicJsonDocument doc(512);
    doc["type"] = "alarm_triggered";
    
    JsonObject data = doc.createNestedObject("data");
    data["name"] = alarm.name;
    data["triggered_at"] = time(NULL);
    data["repeat_type"] = repeatTypeToString(alarm.repeat_type);
    
    if(alarm.repeat_type != RepeatType::ONCE) {
        data["next_trigger"] = next_time;
    }
    
    sendJsonMessage(doc);
}

// 在OnAlarm中调用
void AlarmManager::OnAlarm() {
    // ... 现有触发逻辑 ...
    
    if(alarm.repeat_type != RepeatType::ONCE) {
        time_t next_time = CalculateNextTriggerTime(alarm, now);
        notifyAlarmTriggered(alarm, next_time);
    } else {
        notifyAlarmTriggered(alarm, 0);
    }
}
```

### 4. 响应消息格式（ESP32 → 服务端）

#### 4.1 成功响应

```json
{
  "type": "set_alarm_response",
  "success": true,
  "message": "Alarm set successfully",
  "data": {
    "name": "morning_alarm",
    "next_trigger": 1697184000
  }
}
```

#### 4.2 错误响应

```json
{
  "type": "set_alarm_response",
  "success": false,
  "error": {
    "code": "INVALID_PARAMETER",
    "message": "Invalid time: hour must be 0-23"
  }
}
```

**错误码定义**：
| 错误码 | 说明 |
|--------|------|
| INVALID_PARAMETER | 参数无效 |
| ALARM_NOT_FOUND | 闹钟不存在 |
| TOO_MANY_ALARMS | 闹钟数量超限（>10） |
| NO_FREE_SLOT | 无可用存储槽位 |
| INVALID_TIME | 时间格式错误 |
| NAME_REQUIRED | 名称为空 |

## 服务端实现示例

### Node.js + WebSocket 示例

```javascript
const WebSocket = require('ws');

class AlarmService {
  constructor(wsUrl) {
    this.ws = new WebSocket(wsUrl);
    this.setupHandlers();
  }
  
  setupHandlers() {
    this.ws.on('message', (data) => {
      const msg = JSON.parse(data);
      this.handleMessage(msg);
    });
  }
  
  handleMessage(msg) {
    switch(msg.type) {
      case 'alarm_triggered':
        this.onAlarmTriggered(msg.data);
        break;
      case 'list_alarms_response':
        this.onAlarmsList(msg.data);
        break;
      // ... 其他消息处理
    }
  }
  
  // 设置每日闹钟
  setDailyAlarm(name, hour, minute) {
    const msg = {
      type: 'set_daily_alarm',
      data: {
        name: name,
        hour: hour,
        minute: minute
      }
    };
    this.ws.send(JSON.stringify(msg));
  }
  
  // 设置每周闹钟
  setWeeklyAlarm(name, hour, minute, weekdays) {
    const msg = {
      type: 'set_weekly_alarm',
      data: {
        name: name,
        hour: hour,
        minute: minute,
        weekdays: weekdays  // 例如: [1, 3, 5]
      }
    };
    this.ws.send(JSON.stringify(msg));
  }
  
  // 启用/禁用闹钟
  enableAlarm(name, enabled) {
    const msg = {
      type: 'enable_alarm',
      data: {
        name: name,
        enabled: enabled
      }
    };
    this.ws.send(JSON.stringify(msg));
  }
  
  // 取消闹钟
  cancelAlarm(name) {
    const msg = {
      type: 'cancel_alarm',
      data: {
        name: name
      }
    };
    this.ws.send(JSON.stringify(msg));
  }
  
  // 查询闹钟列表
  listAlarms() {
    const msg = {
      type: 'list_alarms'
    };
    this.ws.send(JSON.stringify(msg));
  }
  
  // 闹钟触发事件处理
  onAlarmTriggered(data) {
    console.log(`Alarm triggered: ${data.name}`);
    
    // 可以在这里：
    // 1. 推送通知到移动端
    // 2. 记录触发日志
    // 3. 触发其他自动化任务
    
    if(data.next_trigger) {
      console.log(`Next trigger: ${new Date(data.next_trigger * 1000)}`);
    }
  }
  
  // 闹钟列表响应处理
  onAlarmsList(data) {
    console.log(`Total alarms: ${data.count}`);
    data.alarms.forEach(alarm => {
      console.log(`- ${alarm.name}: ${alarm.next_trigger} (${alarm.enabled ? 'enabled' : 'disabled'})`);
    });
  }
}

// 使用示例
const service = new AlarmService('ws://192.168.1.100:8080');

// 设置每天早上7:30闹钟
service.setDailyAlarm('morning', 7, 30);

// 设置周一、三、五下午6点健身提醒
service.setWeeklyAlarm('gym', 18, 0, [1, 3, 5]);

// 临时禁用闹钟
service.enableAlarm('morning', false);

// 查询所有闹钟
service.listAlarms();
```

### Python + MQTT 示例

```python
import json
import paho.mqtt.client as mqtt
from datetime import datetime

class AlarmMQTTClient:
    def __init__(self, broker, port, device_id):
        self.client = mqtt.Client()
        self.device_id = device_id
        self.topic_prefix = f"device/{device_id}/alarm"
        
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
        self.client.connect(broker, port, 60)
        self.client.loop_start()
    
    def on_connect(self, client, userdata, flags, rc):
        print(f"Connected with result code {rc}")
        # 订阅响应主题
        self.client.subscribe(f"{self.topic_prefix}/response")
        self.client.subscribe(f"{self.topic_prefix}/triggered")
    
    def on_message(self, client, userdata, msg):
        payload = json.loads(msg.payload.decode())
        
        if msg.topic.endswith('/triggered'):
            self.handle_alarm_triggered(payload)
        elif msg.topic.endswith('/response'):
            self.handle_response(payload)
    
    def set_daily_alarm(self, name, hour, minute):
        msg = {
            "type": "set_daily_alarm",
            "data": {
                "name": name,
                "hour": hour,
                "minute": minute
            }
        }
        self.client.publish(f"{self.topic_prefix}/command", json.dumps(msg))
    
    def set_weekly_alarm(self, name, hour, minute, weekdays):
        msg = {
            "type": "set_weekly_alarm",
            "data": {
                "name": name,
                "hour": hour,
                "minute": minute,
                "weekdays": weekdays
            }
        }
        self.client.publish(f"{self.topic_prefix}/command", json.dumps(msg))
    
    def enable_alarm(self, name, enabled):
        msg = {
            "type": "enable_alarm",
            "data": {
                "name": name,
                "enabled": enabled
            }
        }
        self.client.publish(f"{self.topic_prefix}/command", json.dumps(msg))
    
    def cancel_alarm(self, name):
        msg = {
            "type": "cancel_alarm",
            "data": {
                "name": name
            }
        }
        self.client.publish(f"{self.topic_prefix}/command", json.dumps(msg))
    
    def list_alarms(self):
        msg = {"type": "list_alarms"}
        self.client.publish(f"{self.topic_prefix}/command", json.dumps(msg))
    
    def handle_alarm_triggered(self, payload):
        data = payload.get('data', {})
        name = data.get('name')
        triggered_at = data.get('triggered_at')
        
        print(f"[ALARM TRIGGERED] {name} at {datetime.fromtimestamp(triggered_at)}")
        
        # 推送通知、记录日志等
        self.send_push_notification(name)
        self.log_alarm_event(name, triggered_at)
    
    def handle_response(self, payload):
        if payload.get('success'):
            print(f"[SUCCESS] {payload.get('message')}")
        else:
            error = payload.get('error', {})
            print(f"[ERROR] {error.get('code')}: {error.get('message')}")
    
    def send_push_notification(self, alarm_name):
        # 实现推送通知逻辑
        pass
    
    def log_alarm_event(self, name, timestamp):
        # 记录到数据库
        pass

# 使用示例
client = AlarmMQTTClient('mqtt.example.com', 1883, 'esp32_001')

# 设置每天早上8点
client.set_daily_alarm('morning', 8, 0)

# 设置工作日闹钟（需要转换）
# 周一到周五 = [1, 2, 3, 4, 5]
client.set_weekly_alarm('work', 7, 30, [1, 2, 3, 4, 5])
```

## REST API 设计参考

如果使用HTTP REST API，可以设计如下接口：

### 1. 设置闹钟

```
POST /api/v1/devices/{device_id}/alarms
Content-Type: application/json

{
  "name": "morning_alarm",
  "type": "daily",
  "hour": 7,
  "minute": 30
}
```

### 2. 查询闹钟列表

```
GET /api/v1/devices/{device_id}/alarms

Response:
{
  "alarms": [
    {
      "name": "morning_alarm",
      "type": "daily",
      "hour": 7,
      "minute": 30,
      "enabled": true,
      "next_trigger": "2025-10-14T07:30:00Z"
    }
  ]
}
```

### 3. 更新闹钟状态

```
PATCH /api/v1/devices/{device_id}/alarms/{alarm_name}
Content-Type: application/json

{
  "enabled": false
}
```

### 4. 删除闹钟

```
DELETE /api/v1/devices/{device_id}/alarms/{alarm_name}
```

## 数据库设计参考

### alarms 表

```sql
CREATE TABLE alarms (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    name VARCHAR(64) NOT NULL,
    repeat_type ENUM('once', 'daily', 'weekly', 'workdays', 'weekends') NOT NULL,
    hour INT,
    minute INT,
    weekdays TINYINT UNSIGNED,  -- 位掩码
    enabled BOOLEAN DEFAULT TRUE,
    next_trigger TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    
    UNIQUE KEY uk_device_name (device_id, name),
    INDEX idx_device_id (device_id),
    INDEX idx_next_trigger (next_trigger)
);
```

### alarm_logs 表

```sql
CREATE TABLE alarm_logs (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    alarm_name VARCHAR(64) NOT NULL,
    triggered_at TIMESTAMP NOT NULL,
    status ENUM('triggered', 'dismissed', 'snoozed') NOT NULL,
    
    INDEX idx_device_id (device_id),
    INDEX idx_triggered_at (triggered_at)
);
```

## 完整集成流程示例

### 场景：用户通过APP设置闹钟

```
1. 用户操作
   移动APP → 点击"设置闹钟" → 选择"每天7:30"

2. APP → 服务端
   POST /api/v1/devices/esp32_001/alarms
   {"name": "morning", "type": "daily", "hour": 7, "minute": 30}

3. 服务端 → ESP32
   WebSocket/MQTT 发送:
   {"type": "set_daily_alarm", "data": {"name": "morning", "hour": 7, "minute": 30}}

4. ESP32处理
   alarmManager.SetDailyAlarm("morning", 7, 30);

5. ESP32 → 服务端（响应）
   {"type": "set_daily_alarm_response", "success": true, "data": {...}}

6. 服务端 → 数据库
   INSERT INTO alarms (device_id, name, repeat_type, hour, minute) 
   VALUES ('esp32_001', 'morning', 'daily', 7, 30);

7. 服务端 → APP
   HTTP 200 OK
   {"success": true, "alarm": {...}}

8. 闹钟触发（第二天7:30）
   ESP32 → 服务端:
   {"type": "alarm_triggered", "data": {"name": "morning", "triggered_at": 1697184000}}

9. 服务端处理
   - 记录到alarm_logs表
   - 推送通知到用户APP
   - 触发其他自动化（如智能家居场景）
```

## 安全建议

### 1. 身份认证

```json
// WebSocket连接时携带token
{
  "type": "auth",
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

### 2. 消息签名

```json
{
  "type": "set_alarm",
  "data": {...},
  "timestamp": 1697184000,
  "signature": "sha256_hash_of_data_and_secret"
}
```

### 3. 限流保护

- 每个设备每分钟最多10次设置操作
- 每个设备最多10个闹钟
- 超时重传最多3次

## 测试清单

- [ ] 设置各种类型的闹钟
- [ ] 启用/禁用闹钟
- [ ] 取消闹钟
- [ ] 查询闹钟列表
- [ ] 闹钟触发通知
- [ ] 重复闹钟自动重调度
- [ ] 系统重启后闹钟恢复
- [ ] 网络断开重连后数据同步
- [ ] 并发操作的线程安全
- [ ] 异常情况错误处理

## 常见问题

**Q: 网络断开期间设置的闹钟如何同步？**  
A: 建议服务端维护期望状态，设备重连后主动拉取最新配置。

**Q: 如何处理时区变化？**  
A: 服务端发送UTC时间戳，设备根据本地时区转换。

**Q: 闹钟触发消息丢失怎么办？**  
A: 设备本地记录触发日志，定期同步到服务端。

---

**文档版本**: v1.0  
**最后更新**: 2025-10-13  
**维护者**: Cascade AI
