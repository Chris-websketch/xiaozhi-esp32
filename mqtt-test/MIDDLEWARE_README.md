# 闹钟MQTT中间件服务

这是一个HTTP到MQTT的中间件服务，允许通过简单的HTTP API来控制ESP32设备的闹钟功能。

## 功能特性

- ✅ HTTP REST API接口，方便集成
- ✅ 自动转换HTTP请求为MQTT消息
- ✅ 等待设备ACK确认，保证消息送达
- ✅ 支持完整闹钟参数和快捷接口
- ✅ 支持多设备管理
- ✅ CORS支持，可用于Web前端

## 安装依赖

```bash
pip install -r requirements.txt
```

所需依赖：
- `paho-mqtt>=1.6.1` - MQTT客户端
- `Flask>=2.3.0` - HTTP服务器
- `flask-cors>=4.0.0` - 跨域支持

## 配置

编辑 `alarm_mqtt_middleware.py` 中的配置项：

```python
MQTT_BROKER = "x6bf310e.ala.cn-hangzhou.emqxsl.cn"
MQTT_PORT = 8883
MQTT_USERNAME = "xiaoqiao"
MQTT_PASSWORD = "dzkj0000"
MQTT_USE_SSL = True
MQTT_CA_CERT = "emqx_ca.crt"

HTTP_PORT = 6999
ACK_TIMEOUT = 10  # ACK等待超时时间（秒）
```

## 启动服务

```bash
python alarm_mqtt_middleware.py
```

启动后会看到：
```
============================================================
闹钟MQTT中间件服务
============================================================
正在连接到MQTT Broker x6bf310e.ala.cn-hangzhou.emqxsl.cn:8883...
✅ MQTT连接成功
已订阅: devices/+/ack

🚀 启动HTTP服务器 http://0.0.0.0:6999

API端点:
  - POST /api/v1/alarm/set       设置闹钟（完整参数）
  - POST /api/v1/alarm/cancel    取消闹钟
  - POST /api/v1/alarm/quick     快速设置闹钟（简化接口）
  - GET  /api/v1/device/status   检查设备在线状态
  - GET  /health                 健康检查
============================================================
```

## API文档

### 1. 健康检查

**端点**: `GET /health`

**响应**:
```json
{
  "status": "ok",
  "mqtt_connected": true,
  "timestamp": "2025-10-13T22:30:00.123456"
}
```

### 2. 设备状态检查

**端点**: `GET /api/v1/device/status`

**查询参数**:
| 参数 | 类型 | 说明 |
|------|------|------|
| device_id | string | 设备ID（必填） |

**请求示例**:
```bash
curl "http://duzhong.oaibro.cn:6999/api/v1/device/status?device_id=719ae1ad-9f2c-4277-9c99-1a317a478979"
```

**响应**:
```json
{
  "success": true,
  "data": {
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "online": true,
    "last_heartbeat": "2025-10-14T11:30:45.123456",
    "can_set_alarm": true,
    "seconds_since_heartbeat": 15,
    "mqtt_connected": true
  }
}
```

**字段说明**:
- `online`: 设备是否在线（120秒内有心跳视为在线）
- `last_heartbeat`: 最后一次心跳时间
- `can_set_alarm`: 是否可以设置闹钟（设备在线且MQTT已连接）
- `seconds_since_heartbeat`: 距离最后一次心跳的秒数

### 3. 设置闹钟

**端点**: `POST /api/v1/alarm/set`

**请求体**:
```json
{
  "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
  "id": "morning_alarm",
  "repeat_type": 1,
  "seconds": 0,
  "hour": 7,
  "minute": 30,
  "repeat_days": 0
}
```

**参数说明**:
| 字段 | 类型 | 说明 |
|------|------|------|
| device_id | string | 设备ID（必填） |
| id | string | 闹钟ID（必填） |
| repeat_type | int | 重复类型：0=ONCE, 1=DAILY, 2=WEEKLY（必填） |
| seconds | int | ONCE类型使用，从现在开始的秒数；其他类型填0（必填） |
| hour | int | DAILY/WEEKLY使用，小时(0-23)；ONCE类型填0（必填） |
| minute | int | DAILY/WEEKLY使用，分钟(0-59)；ONCE类型填0（必填） |
| repeat_days | int | WEEKLY使用，位掩码；其他类型填0（必填） |

**响应**:
```json
{
  "success": true,
  "message": "命令已发送",
  "data": {
    "alarm_id": "morning_alarm"
  }
}
```

### 4. 取消闹钟

**端点**: `POST /api/v1/alarm/cancel`

**请求体**:
```json
{
  "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
  "id": "morning_alarm"
}
```

**响应**:
```json
{
  "success": true,
  "message": "命令已发送",
  "data": {
    "alarm_id": "morning_alarm"
  }
}
```


## 使用示例

### curl命令

**设置一次性闹钟（60秒后）**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 闹钟ID
# - repeat_type: 0=一次性闹钟
# - seconds: 60秒后触发
# - hour/minute: 一次性闹钟不使用，填0
# - repeat_days: 一次性闹钟不使用，填0

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/set \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "quick_reminder",
    "repeat_type": 0,
    "seconds": 60,
    "hour": 0,
    "minute": 0,
    "repeat_days": 0
  }'
```

**设置每日闹钟（每天7:30）**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 闹钟ID
# - repeat_type: 1=每日闹钟
# - seconds: 每日闹钟不使用，填0
# - hour: 小时(0-23)
# - minute: 分钟(0-59)
# - repeat_days: 每日闹钟不使用，填0

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/set \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "morning_alarm",
    "repeat_type": 1,
    "seconds": 0,
    "hour": 7,
    "minute": 30,
    "repeat_days": 0
  }'
```

**设置每周闹钟（周一、三、五18:00）**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 闹钟ID
# - repeat_type: 2=每周闹钟
# - seconds: 每周闹钟不使用，填0
# - hour: 小时(0-23)
# - minute: 分钟(0-59)
# - repeat_days: 位掩码 42 = 2+8+32 (周一+周三+周五)

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/set \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "gym_reminder",
    "repeat_type": 2,
    "seconds": 0,
    "hour": 18,
    "minute": 0,
    "repeat_days": 42
  }'
```

**设置工作日闹钟（周一到周五7:00）**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 闹钟ID
# - repeat_type: 2=每周闹钟
# - seconds: 每周闹钟不使用，填0
# - hour: 小时(0-23)
# - minute: 分钟(0-59)
# - repeat_days: 位掩码 62 = 2+4+8+16+32 (周一至周五)

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/set \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "work_alarm",
    "repeat_type": 2,
    "seconds": 0,
    "hour": 7,
    "minute": 0,
    "repeat_days": 62
  }'
```

**设置周末闹钟（周六周日9:00）**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 闹钟ID
# - repeat_type: 2=每周闹钟
# - seconds: 每周闹钟不使用，填0
# - hour: 小时(0-23)
# - minute: 分钟(0-59)
# - repeat_days: 位掩码 65 = 1+64 (周日+周六)

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/set \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "weekend_alarm",
    "repeat_type": 2,
    "seconds": 0,
    "hour": 9,
    "minute": 0,
    "repeat_days": 65
  }'
```

**取消闹钟**:
```bash
# 参数说明：
# - device_id: 设备ID
# - id: 要取消的闹钟ID

curl -X POST http://duzhong.oaibro.cn:6999/api/v1/alarm/cancel \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "id": "morning_alarm"
  }'
```

### Python客户端

```python
import requests

# 设置每日闹钟
response = requests.post('http://duzhong.oaibro.cn:6999/api/v1/alarm/set', json={
    'device_id': '719ae1ad-9f2c-4277-9c99-1a317a478979',
    'id': 'morning_alarm',
    'repeat_type': 1,
    'seconds': 0,
    'hour': 7,
    'minute': 30,
    'repeat_days': 0
})

result = response.json()
print(f"成功: {result['success']}")
print(f"消息: {result['message']}")
```

### JavaScript (Fetch API)

```javascript
// 设置工作日闹钟
fetch('http://duzhong.oaibro.cn:6999/api/v1/alarm/set', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
  },
  body: JSON.stringify({
    device_id: '719ae1ad-9f2c-4277-9c99-1a317a478979',
    id: 'work_alarm',
    repeat_type: 2,
    seconds: 0,
    hour: 7,
    minute: 0,
    repeat_days: 62
  })
})
.then(response => response.json())
.then(data => {
  console.log('成功:', data.success);
  console.log('消息:', data.message);
});
```

## 错误处理

所有API端点都返回统一的响应格式：

**成功响应** (HTTP 200):
```json
{
  "success": true,
  "message": "命令已发送",
  "data": {
    "alarm_id": "morning_alarm"
  }
}
```

**失败响应** (HTTP 400/500):
```json
{
  "success": false,
  "message": "错误原因描述"
}
```

**常见错误**:
- `缺少必需字段` - 请求体缺少必需参数
- `MQTT未连接` - MQTT连接断开
- `MQTT发送失败` - 消息发布到MQTT Broker失败

## 工作原理

1. **HTTP请求到达** → 中间件接收HTTP API请求
2. **参数验证** → 验证必需字段和参数合法性
3. **构建MQTT消息** → 转换为标准IoT命令格式
4. **发送到设备** → 通过MQTT发布到 `devices/{device_id}/downlink`
5. **返回结果** → 命令发送成功后立即返回，不等待设备ACK
6. **设备处理** → 设备接收并执行命令，发送ACK到 `devices/{device_id}/ack`

## 位掩码参考

用于WEEKLY类型的repeat_days字段：

| 星期 | 位值 | 十进制 |
|------|------|--------|
| 周日 | bit0 | 1 |
| 周一 | bit1 | 2 |
| 周二 | bit2 | 4 |
| 周三 | bit3 | 8 |
| 周四 | bit4 | 16 |
| 周五 | bit5 | 32 |
| 周六 | bit6 | 64 |

**常用组合**:
- 周一三五：2 + 8 + 32 = **42**
- 周二四：4 + 16 = **20**
- 周末：1 + 64 = **65**
- 工作日：2 + 4 + 8 + 16 + 32 = **62**
- 每天：1 + 2 + 4 + 8 + 16 + 32 + 64 = **127**

## 日志输出

服务运行时会输出详细日志：

```
💓 设备心跳: device_id=719ae1ad-9f2c-4277-9c99-1a317a478979
📤 发送命令到: devices/719ae1ad-9f2c-4277-9c99-1a317a478979/downlink
   内容: {"type":"iot","commands":[...],"request_id":"req_..."}
📨 收到消息: devices/719ae1ad-9f2c-4277-9c99-1a317a478979/ack
   内容: {"type":"ack","status":"ok",...}
✅ 设备ACK: status=ok, request_id=req_...
```

## 注意事项

1. **设备ID**: 确保使用正确的设备client_id
2. **CA证书**: SSL连接需要正确配置 `emqx_ca.crt` 文件
3. **并发处理**: 服务支持同时处理多个设备的请求
4. **闹钟ID**: 同ID闹钟会覆盖旧设置
5. **消息发送**: 中间件发送命令后即返回，不等待设备ACK

## 故障排查

**MQTT连接失败**:
- 检查网络连接
- 验证MQTT_BROKER地址
- 确认用户名密码正确
- 检查CA证书文件是否存在

**设备无响应**:
- 使用 `/api/v1/device/status` 检查设备是否在线
- 验证device_id是否正确
- 检查设备网络连接
- 查看MQTT日志确认消息是否送达

**参数错误**:
- 参考API文档确认必需字段
- 检查参数类型和范围
- 使用/health端点测试服务状态

---

**文档版本**: v1.2  
**最后更新**: 2025-10-14  
**端口**: 6999  
**新增功能**: 设备在线状态检查、心跳监控
