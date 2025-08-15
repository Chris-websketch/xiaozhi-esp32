发布主题为：devices/3095dd17-a431-4a49-90e5-2207a31d327e/downlink

# IoT 设备 MQTT 设置消息

## 1) Screen（屏幕控制）
- 设备名：`Screen`
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
- 设备名：`Speaker`
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
- 设备名：`Alarm`
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

## 4) RotateDisplay（屏幕旋转）
```json
{
  "type": "iot",
  "commands": [
    { "name": "RotateDisplay", "method": "RotateDisplay", "parameters": {} }
  ]
}
```

## 5) ColorStrip（WS2812 彩灯）
```json
{
  "type": "iot",
  "commands": [
    { "name": "ColorStrip", "method": "TurnOn", "parameters": {} },
    { "name": "ColorStrip", "method": "SetBrightness", "parameters": { "brightness": 80 } },
    { "name": "ColorStrip", "method": "ChangeEffectMode", "parameters": {} },
    { "name": "ColorStrip", "method": "TurnOff", "parameters": {} }
  ]
}
```

## 6) ImageDisplay（图片显示模式）
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

## 设备重启消息

### 方案1：如果有系统控制类 IoT 设备
```json
{
  "type": "iot",
  "commands": [
    { "name": "System", "method": "Restart", "parameters": {} }
  ]
}
```

### 方案2：通过特殊的通知消息触发重启
```json
{
  "type": "system",
  "command": "restart"
}
```

### 方案3：如果设备支持 OTA 重启
```json
{
  "type": "ota",
  "command": "restart"
}
```

## 组合示例
```json
{
  "type": "iot",
  "commands": [
    { "name": "Screen", "method": "SetBrightness", "parameters": { "brightness": 100 } },
    { "name": "Speaker", "method": "SetVolume", "parameters": { "volume": 90 } },
    { "name": "ColorStrip", "method": "TurnOn", "parameters": {} },
    { "name": "Alarm", "method": "SetAlarm", "parameters": { "second_from_now": 300, "alarm_name": "5分钟提醒" } }
  ]
}
```
```

接下来，我将把这个内容写入到指定的文件中。