# MQTT调试工具 - 小智ESP32项目专用

基于PySide6和paho-mqtt的MQTT调试工具，专为小智ESP32项目EMQX Cloud部署优化。

## 功能特性

- ✅ 预配置EMQX Cloud连接（SSL/TLS）
- ✅ 多主题同时订阅（QoS 0/1/2）
- ✅ 快捷主题按钮（Downlink/Uplink/ACK）
- ✅ 丰富的消息模板库（IoT控制/系统控制/通知）
- ✅ 消息历史记录（自动格式化JSON）
- ✅ 固定ClientID支持
- ✅ 自动CA证书加载
- ✅ **ACK自动回复**（模拟服务器确认机制）

## 工具列表

本目录包含3个工具：

### 1. MQTT调试工具（图形界面）
- **文件**: `mqtt_debug_tool.py`
- **功能**: PySide6图形界面，用于调试MQTT消息
- **特性**: 消息模板、ACK自动回复、多主题订阅

### 2. 闹钟MQTT中间件（HTTP服务）
- **文件**: `alarm_mqtt_middleware.py`
- **功能**: HTTP到MQTT的中间件服务
- **特性**: REST API接口、ACK确认、多设备管理
- **文档**: 详见 `MIDDLEWARE_README.md`

### 3. 中间件测试脚本
- **文件**: `test_middleware.py`
- **功能**: 演示如何使用中间件API
- **特性**: 各种闹钟类型测试

## 安装依赖

```bash
pip install -r requirements.txt
```

依赖包括：
- `paho-mqtt` - MQTT客户端
- `PySide6` - 图形界面（仅调试工具需要）
- `Flask` - HTTP服务器（仅中间件需要）
- `flask-cors` - 跨域支持（仅中间件需要）

## 使用方法

### MQTT调试工具

```bash
python mqtt_debug_tool.py
```

### 闹钟MQTT中间件服务

```bash
# 启动中间件
python alarm_mqtt_middleware.py

# 运行测试（另一个终端）
python test_middleware.py
```

详细API文档见：[MIDDLEWARE_README.md](MIDDLEWARE_README.md)

## 默认配置

程序已内置小智项目EMQX Cloud专用配置：

- **Broker**: x6bf310e.ala.cn-hangzhou.emqxsl.cn
- **Port**: 8883 (SSL/TLS)
- **Username**: xiaoqiao
- **Password**: dzkj0000
- **设备ID**: 719ae1ad-9f2c-4277-9c99-1a317a478979（ESP32设备，用于主题拼接）
- **调试工具ID**: mqtt-debug-tool-xxxxxxxx（随机生成，避免冲突）
- **CA证书**: emqx_ca.crt（已下载）

> **重要**：调试工具使用独立的ClientID，不会与ESP32设备冲突，可以同时在线监控设备消息。

## 操作说明

### 1. 连接到Broker
1. 程序已预配置项目专用服务器，直接点击"连接"即可
2. SSL/TLS默认启用，CA证书自动加载
3. **自动回复ACK**默认启用（模拟服务器行为）
4. 调试工具使用独立ClientID，不会影响ESP32设备连接
5. 鼠标悬停在"设备ID"上可查看调试工具的实际ClientID

### 2. 订阅主题
**方式一：使用快捷按钮**
1. 点击"Downlink/Uplink/ACK"快捷按钮自动填充主题
2. 选择QoS级别（默认QoS 2）
3. 点击"添加"按钮

**方式二：手动输入**
1. 输入完整主题路径
2. 支持通配符（# 和 +）
3. 点击"添加"订阅

**取消订阅**：点击对应主题行的"删除"按钮

### 3. 发布消息
**使用消息模板（推荐）**
1. 点击快捷按钮填充目标主题（如↓Downlink）
2. 从"消息模板"下拉菜单选择分类和具体模板
3. 消息内容自动填充，可根据需要修改参数
4. 点击"发送"按钮

**手动编写消息**
1. 点击快捷按钮或手动输入主题
2. 在消息内容框输入JSON格式数据
3. 选择QoS级别
4. 点击"发送"按钮

### 4. 查看消息历史
- 所有接收到的消息显示在"消息历史"区域
- JSON格式消息自动格式化
- 时间戳精确到毫秒
- 点击"清空"清除历史记录

## 界面说明

```
┌─────────────────────────────────────────────────────┐
│  连接配置区                                          │
├──────────────────┬──────────────────────────────────┤
│  订阅管理区      │  消息发布区                       │
│  (左侧)          │  (右侧)                          │
├──────────────────┴──────────────────────────────────┤
│  消息历史区                                          │
│  (底部)                                             │
└─────────────────────────────────────────────────────┘
```

## 消息模板库

### IoT控制模板
- **屏幕控制**：设置亮度、切换主题（light/dark）
- **扬声器控制**：设置音量（0-100）
- **闹钟管理**：一次性闹钟、每天重复、工作日闹钟、取消闹钟
- **图片显示**：动态/静态模式切换
- **音乐播放器**：显示/隐藏播放器界面

### 系统控制模板
- **设备重启**：支持延迟重启（1秒/5秒）

### 通知消息模板
- **简单通知**：包含标题和内容
- **仅标题/仅内容**：灵活配置通知格式

## 主题说明

项目使用标准IoT主题结构：

### Downlink（下行 - 服务器→设备）
- **主题**: `devices/{client_id}/downlink`
- **用途**: 服务器向设备发送控制指令
- **QoS**: 2（确保送达）
- **示例**: `devices/719ae1ad-9f2c-4277-9c99-1a317a478979/downlink`

### Uplink（上行 - 设备→服务器）
- **主题**: `devices/{client_id}/uplink`
- **用途**: 设备上报遥测数据（心跳、状态）
- **QoS**: 0（快速上报）
- **频率**: 每30秒

### ACK（确认 - 设备→服务器）
- **主题**: `devices/{client_id}/ack`
- **用途**: 设备执行指令后的结果确认
- **QoS**: 2（可靠送达）
- **调试工具行为**: 自动回复`ack_receipt`确认（模拟真实服务器）

## 注意事项

- 必须先连接成功后才能订阅和发布消息
- 断开连接时会自动清空所有订阅
- 消息历史仅保存在内存中，关闭程序后会丢失
- 支持MQTT 3.1.1协议
- CA证书有效期至2031.11.10

## 快速测试

### 测试设备控制
1. 连接成功后，点击订阅区的"Downlink"按钮
2. QoS设为2，点击"添加"订阅下行主题
3. 在发布区点击"↓Downlink"按钮
4. 选择模板"IoT控制" → "扬声器 - 设置音量"
5. 修改volume参数（如改为60）
6. 点击"发送"

### 监控设备上报
1. 点击订阅区的"Uplink"按钮
2. 点击"添加"订阅上行主题
3. 等待设备每30秒自动上报遥测数据
4. 在消息历史区查看设备状态（电量、内存、WiFi等）

### 测试ACK自动回复机制
1. 确保"自动回复ACK"已勾选
2. 订阅ACK主题：`devices/719ae1ad.../ack`
3. 向设备发送任意控制指令（如设置音量）
4. 观察消息历史：
   - 设备发送ACK消息（带`message_id`）
   - 调试工具自动回复`ack_receipt`确认
   - 设备日志不再显示"ACK timeout"警告

## 示例消息

### 设置屏幕亮度
```json
{
  "type": "iot",
  "commands": [
    {
      "name": "Screen",
      "method": "SetBrightness",
      "parameters": {
        "brightness": 80
      }
    }
  ]
}
```

### 设置闹钟
```json
{
  "type": "iot",
  "commands": [
    {
      "name": "Alarm",
      "method": "SetAlarm",
      "parameters": {
        "second_from_now": 60,
        "alarm_name": "测试闹钟"
      }
    }
  ]
}
```

### 通知消息
```json
{
  "type": "notify",
  "title": "系统通知",
  "body": "这是一条测试通知"
}
```

### ACK确认回复（自动）
当设备发送ACK消息：
```json
{
  "type": "ack",
  "target": "iot",
  "status": "ok",
  "message_id": "msg_1760377513676_1",
  "command": {...}
}
```

调试工具自动回复（QoS 1）：
```json
{
  "type": "ack_receipt",
  "message_id": "msg_1760377513676_1",
  "received_at": 1760377514,
  "status": "processed"
}
```

## ACK自动回复机制

调试工具模拟真实服务器行为，自动处理ACK确认：

1. **监听ACK主题** - 订阅 `devices/{client_id}/ack`
2. **检测ACK消息** - 识别包含`type: "ack"`和`message_id`的消息
3. **自动回复** - 立即发送`ack_receipt`到`downlink`主题
4. **防止超时** - 设备收到确认后不再重试

**优势**：
- 避免设备"ACK timeout"警告
- 模拟真实服务器响应
- 提高测试可靠性
- 支持完整的确认机制测试

## 闹钟MQTT中间件快速开始

中间件服务提供HTTP API接口，将HTTP请求转换为MQTT消息并等待设备确认。

### 启动中间件服务

```bash
python alarm_mqtt_middleware.py
```

输出：
```
============================================================
闹钟MQTT中间件服务
============================================================
正在连接到MQTT Broker...
✅ MQTT连接成功
🚀 启动HTTP服务器 http://0.0.0.0:5000
============================================================
```

### HTTP API示例

**设置每日闹钟（curl）**:
```bash
curl -X POST http://localhost:5000/api/v1/alarm/quick \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "719ae1ad-9f2c-4277-9c99-1a317a478979",
    "name": "morning",
    "type": "daily",
    "hour": 7,
    "minute": 30
  }'
```

**Python客户端**:
```python
import requests

response = requests.post('http://localhost:5000/api/v1/alarm/quick', json={
    'device_id': '719ae1ad-9f2c-4277-9c99-1a317a478979',
    'name': 'morning',
    'type': 'daily',
    'hour': 7,
    'minute': 30
})

result = response.json()
print(f"成功: {result['success']}")
```

### 运行测试脚本

```bash
python test_middleware.py
```

测试脚本会依次演示：
1. 健康检查
2. 一次性闹钟（30秒后）
3. 每日闹钟（8:00）
4. 每周闹钟（周一三五 18:00）
5. 工作日闹钟（7:00）
6. 周末闹钟（9:00）
7. 完整参数接口
8. 取消闹钟
9. 错误处理

### API端点

- `GET  /health` - 健康检查
- `POST /api/v1/alarm/set` - 设置闹钟（完整参数）
- `POST /api/v1/alarm/cancel` - 取消闹钟
- `POST /api/v1/alarm/quick` - 快速设置闹钟（简化接口）

详细文档：[MIDDLEWARE_README.md](MIDDLEWARE_README.md)
