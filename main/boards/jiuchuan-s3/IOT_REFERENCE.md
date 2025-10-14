# Jiuchuan-S3 IoT设备远程控制参考

## 概述

Jiuchuan-S3板子支持通过IoT协议（MQTT/WebSocket）远程控制以下设备功能：

- **Speaker** - 音量控制
- **Screen** - 屏幕亮度和主题
- **ImageDisplay** - 图片显示模式（动画/静态）
- **MusicPlayer** - 音乐播放器
- **AlarmIot** - 闹钟管理（需启用CONFIG_USE_ALARM）

---

## 1. Speaker（扬声器）

### 属性

| 属性名 | 类型 | 说明 | 取值范围 |
|--------|------|------|----------|
| `volume` | Number | 当前音量值 | 0-100 |

### 方法

#### SetVolume - 设置音量

**参数：**
- `volume` (Number, 必需) - 目标音量值，0-100之间的整数

**示例：**
```json
{
  "device": "Speaker",
  "method": "SetVolume",
  "params": {
    "volume": 50
  }
}
```

---

## 2. Screen（屏幕）

### 属性

| 属性名 | 类型 | 说明 | 取值范围 |
|--------|------|------|----------|
| `theme` | String | 当前主题 | "dark" 或 "light" |
| `brightness` | Number | 当前亮度百分比 | 0-100 |

### 方法

#### SetTheme - 设置主题

**参数：**
- `theme_name` (String, 必需) - 主题模式，"light" 或 "dark"

**示例：**
```json
{
  "device": "Screen",
  "method": "SetTheme",
  "params": {
    "theme_name": "light"
  }
}
```

#### SetBrightness - 设置亮度

**参数：**
- `brightness` (Number, 必需) - 亮度值，0-100之间的整数

**示例：**
```json
{
  "device": "Screen",
  "method": "SetBrightness",
  "params": {
    "brightness": 80
  }
}
```

---

## 3. ImageDisplay（图片显示控制）

### 属性

| 属性名 | 类型 | 说明 | 取值范围 |
|--------|------|------|----------|
| `display_mode` | Number | 显示模式 | 0=动画，1=静态logo |

### 方法

#### SetAnimatedMode - 设置为动画模式

**说明：** 说话时播放动画序列

**参数：** 无

**示例：**
```json
{
  "device": "ImageDisplay",
  "method": "SetAnimatedMode"
}
```

#### SetStaticMode - 设置为静态模式

**说明：** 固定显示logo图片

**参数：** 无

**示例：**
```json
{
  "device": "ImageDisplay",
  "method": "SetStaticMode"
}
```

#### ToggleDisplayMode - 切换显示模式

**说明：** 在动画和静态模式之间切换

**参数：** 无

**示例：**
```json
{
  "device": "ImageDisplay",
  "method": "ToggleDisplayMode"
}
```

**配置持久化：** 所有模式设置都会保存到NVS，重启后自动恢复

---

## 4. MusicPlayer（音乐播放器）

### 功能

- 音乐播放控制（播放、暂停、下一首、上一首）
- 播放列表管理
- 播放状态查询

详细接口参见 `main/iot/things/music_player.h`

---

## 5. AlarmIot（闹钟）

⚠️ **注意：** 需要在配置中启用 `CONFIG_USE_ALARM`

### 属性

| 属性名 | 类型 | 说明 |
|--------|------|------|
| `Alarm_List` | String | 当前所有闹钟的描述信息 |

### 方法

#### SetAlarm - 设置闹钟

**参数：**
- `second_from_now` (Number, 必需) - 闹钟多少秒后响起
- `alarm_name` (String, 必需) - 闹钟的描述（名字）

**示例：**
```json
{
  "device": "Alarm",
  "method": "SetAlarm",
  "params": {
    "second_from_now": 600,
    "alarm_name": "喝水提醒"
  }
}
```

---

## 常见使用场景

### 场景1：夜间模式

降低亮度并切换到暗色主题，显示静态logo：

```json
// 1. 设置低亮度
{"device": "Screen", "method": "SetBrightness", "params": {"brightness": 10}}

// 2. 切换到暗色主题
{"device": "Screen", "method": "SetTheme", "params": {"theme_name": "dark"}}

// 3. 切换到静态logo
{"device": "ImageDisplay", "method": "SetStaticMode"}
```

### 场景2：演示模式

提高亮度，启用动画：

```json
// 1. 设置高亮度
{"device": "Screen", "method": "SetBrightness", "params": {"brightness": 100}}

// 2. 启用动画模式
{"device": "ImageDisplay", "method": "SetAnimatedMode"}

// 3. 设置合适音量
{"device": "Speaker", "method": "SetVolume", "params": {"volume": 70}}
```

### 场景3：省电模式

最小化功耗：

```json
// 1. 最低亮度
{"device": "Screen", "method": "SetBrightness", "params": {"brightness": 5}}

// 2. 静态显示
{"device": "ImageDisplay", "method": "SetStaticMode"}
```

---

## 技术说明

### 实现文件

- **IoT设备注册**：`main/boards/jiuchuan-s3/jiuchuan_dev_board.cc` - `InitializeIot()`
- **图片显示控制**：`main/boards/jiuchuan-s3/iot_image_display.cc`
- **通用IoT设备**：`main/iot/things/` 目录

### 配置存储

- 图片显示模式保存在 NVS 命名空间 `image_display`
- 主题设置保存在显示器配置中
- 亮度自动保存并恢复

### 调试

启用日志查看IoT设备状态：

```c
// 启用TAG
"ImageDisplay"  // 图片显示
"Screen"        // 屏幕控制
"Speaker"       // 音量控制
"AlarmIot"      // 闹钟
```

---

## 故障排查

### 问题：IoT设备未注册

**检查：**
1. 确认 `InitializeIot()` 在构造函数中被调用
2. 查看启动日志是否有 "初始化IoT设备" 和 "IoT设备初始化完成"

### 问题：闹钟设备不可用

**解决：**
在 `sdkconfig` 中启用 `CONFIG_USE_ALARM=y`

### 问题：图片模式切换无效

**检查：**
1. 确认logo图片已下载（查看 `g_static_image` 是否非空）
2. 查看日志确认模式切换成功
3. 确认图片轮播任务正在运行

---

## 更多信息

- 完整功能文档：`IMAGE_FEATURES.md`
- IoT框架说明：`main/iot/thing.h`
- 图片资源管理：`main/image_manager.h`
