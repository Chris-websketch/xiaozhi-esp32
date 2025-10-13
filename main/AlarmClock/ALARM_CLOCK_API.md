# 闹钟功能 API 文档

## 概述

AlarmManager 提供了完整的闹钟管理功能，支持一次性闹钟、每日重复、工作日、周末以及自定义周几的重复闹钟。所有闹钟数据持久化存储，系统重启后自动恢复。

## 功能特性

- ✅ **多种重复模式**：一次性、每日、每周、工作日、周末
- ✅ **持久化存储**：闹钟数据保存到Flash，重启后自动恢复
- ✅ **启用/禁用**：可以临时禁用闹钟而不删除
- ✅ **最多10个闹钟**：同时管理多个闹钟
- ✅ **线程安全**：所有操作都有互斥锁保护
- ✅ **自动调度**：重复闹钟触发后自动计算下次触发时间

## 数据结构

### RepeatType 枚举

```cpp
enum class RepeatType : uint8_t {
    ONCE = 0,        // 一次性闹钟
    DAILY = 1,       // 每日重复
    WEEKLY = 2,      // 每周指定日期重复
    WORKDAYS = 3,    // 工作日（周一到周五）
    WEEKENDS = 4     // 周末（周六周日）
};
```

### Alarm 结构

```cpp
struct Alarm {
    std::string name;              // 闹钟名称（唯一标识）
    time_t time;                   // 下一次触发的绝对时间戳
    RepeatType repeat_type;        // 重复类型
    uint8_t repeat_days;           // 位掩码：bit0=周日, bit1=周一...bit6=周六
    bool enabled;                  // 是否启用
};
```

## API 接口

### 1. 设置闹钟（通用方法）

```cpp
void SetAlarm(int seconds_from_now, std::string alarm_name, 
              RepeatType repeat_type = RepeatType::ONCE, 
              uint8_t repeat_days = 0);
```

**参数：**
- `seconds_from_now`: 从现在开始多少秒后触发（必须 > 0）
- `alarm_name`: 闹钟名称（1-64字符，唯一）
- `repeat_type`: 重复类型（默认：ONCE）
- `repeat_days`: 周几的位掩码（仅WEEKLY类型使用）

**示例：**
```cpp
// 一次性闹钟：60秒后响
alarmManager.SetAlarm(60, "test_alarm");

// 每日重复闹钟：60秒后首次响，之后每天同一时间
alarmManager.SetAlarm(60, "daily_alarm", RepeatType::DAILY);

// 自定义周几：周一、周三、周五
uint8_t days = 0b00101010; // bit1=周一, bit3=周三, bit5=周五
alarmManager.SetAlarm(60, "custom_alarm", RepeatType::WEEKLY, days);
```

### 2. 设置每日重复闹钟

```cpp
void SetDailyAlarm(const std::string& name, int hour, int minute);
```

**参数：**
- `name`: 闹钟名称
- `hour`: 小时 (0-23)
- `minute`: 分钟 (0-59)

**示例：**
```cpp
// 每天早上8点闹钟
alarmManager.SetDailyAlarm("morning", 8, 0);

// 每天晚上10点闹钟
alarmManager.SetDailyAlarm("night", 22, 0);
```

**行为：**
- 如果当天时间已过，首次触发为明天
- 之后每天同一时间重复

### 3. 设置每周重复闹钟

```cpp
void SetWeeklyAlarm(const std::string& name, int hour, int minute, uint8_t weekdays);
```

**参数：**
- `name`: 闹钟名称
- `hour`: 小时 (0-23)
- `minute`: 分钟 (0-59)
- `weekdays`: 周几位掩码 (1-0x7F)

**weekdays 位掩码说明：**
```
bit0 = 周日 (Sunday)    = 0x01 = 0b00000001
bit1 = 周一 (Monday)    = 0x02 = 0b00000010
bit2 = 周二 (Tuesday)   = 0x04 = 0b00000100
bit3 = 周三 (Wednesday) = 0x08 = 0b00001000
bit4 = 周四 (Thursday)  = 0x10 = 0b00010000
bit5 = 周五 (Friday)    = 0x20 = 0b00100000
bit6 = 周六 (Saturday)  = 0x40 = 0b01000000
```

**示例：**
```cpp
// 周一和周五早上7点
uint8_t mon_fri = 0b00100010; // bit1=周一, bit5=周五
alarmManager.SetWeeklyAlarm("workout", 7, 0, mon_fri);

// 周二、周四、周六下午3点
uint8_t tue_thu_sat = 0b01010100;
alarmManager.SetWeeklyAlarm("class", 15, 0, tue_thu_sat);

// 每天（等同于DAILY）
uint8_t everyday = 0x7F; // 0b01111111
alarmManager.SetWeeklyAlarm("medication", 9, 30, everyday);
```

### 4. 设置工作日闹钟

```cpp
void SetWorkdaysAlarm(const std::string& name, int hour, int minute);
```

**参数：**
- `name`: 闹钟名称
- `hour`: 小时 (0-23)
- `minute`: 分钟 (0-59)

**示例：**
```cpp
// 工作日早上7点闹钟（周一到周五）
alarmManager.SetWorkdaysAlarm("work", 7, 0);
```

**行为：**
- 自动设置为周一至周五重复
- 周六周日不触发

### 5. 设置周末闹钟

```cpp
void SetWeekendsAlarm(const std::string& name, int hour, int minute);
```

**参数：**
- `name`: 闹钟名称
- `hour`: 小时 (0-23)
- `minute`: 分钟 (0-59)

**示例：**
```cpp
// 周末早上10点闹钟（周六周日）
alarmManager.SetWeekendsAlarm("weekend", 10, 0);
```

**行为：**
- 自动设置为周六、周日重复
- 工作日不触发

### 6. 启用/禁用闹钟

```cpp
void EnableAlarm(const std::string& name, bool enable);
```

**参数：**
- `name`: 闹钟名称
- `enable`: true=启用, false=禁用

**示例：**
```cpp
// 临时禁用闹钟（不删除）
alarmManager.EnableAlarm("morning", false);

// 重新启用闹钟
alarmManager.EnableAlarm("morning", true);
```

**行为：**
- 禁用的闹钟不会触发，但保留配置
- 启用后立即参与调度计算

### 7. 取消闹钟

```cpp
void CancelAlarm(std::string alarm_name);
```

**参数：**
- `alarm_name`: 闹钟名称

**示例：**
```cpp
// 完全删除闹钟
alarmManager.CancelAlarm("morning");
```

**行为：**
- 从内存和存储中永久删除闹钟
- 自动重新计算下一个触发的闹钟

### 8. 查询闹钟状态

```cpp
std::string GetAlarmsStatus();
```

**返回：** 所有闹钟的状态字符串

**示例：**
```cpp
std::string status = alarmManager.GetAlarmsStatus();
ESP_LOGI(TAG, "Alarms: %s", status.c_str());
// 输出: "morning at 1697184000; work at 1697270400"
```

### 9. 检查闹钟是否触发

```cpp
bool IsRing();
void ClearRing();
```

**示例：**
```cpp
if(alarmManager.IsRing()) {
    // 处理闹钟触发事件
    std::string alarm_name = alarmManager.get_now_alarm_name();
    ESP_LOGI(TAG, "Alarm triggered: %s", alarm_name.c_str());
    
    // 清除触发标志
    alarmManager.ClearRing();
}
```

## 使用场景示例

### 场景1：每天固定时间提醒

```cpp
// 每天早上7:30起床闹钟
alarmManager.SetDailyAlarm("wake_up", 7, 30);

// 每天中午12:00吃药提醒
alarmManager.SetDailyAlarm("medication", 12, 0);

// 每天晚上10:00睡觉提醒
alarmManager.SetDailyAlarm("bedtime", 22, 0);
```

### 场景2：工作日和周末不同作息

```cpp
// 工作日早上6:30起床
alarmManager.SetWorkdaysAlarm("workday_wake", 6, 30);

// 周末早上9:00起床
alarmManager.SetWeekendsAlarm("weekend_wake", 9, 0);
```

### 场景3：自定义周几的重复任务

```cpp
// 每周一、三、五下午3点健身提醒
uint8_t workout_days = 0b00101010; // 周一、周三、周五
alarmManager.SetWeeklyAlarm("gym", 15, 0, workout_days);

// 每周二、四上午10点会议提醒
uint8_t meeting_days = 0b00010100; // 周二、周四
alarmManager.SetWeeklyAlarm("meeting", 10, 0, meeting_days);
```

### 场景4：临时禁用/启用

```cpp
// 周末出游，临时禁用工作日闹钟
alarmManager.EnableAlarm("workday_wake", false);

// 周一回来，重新启用
alarmManager.EnableAlarm("workday_wake", true);
```

### 场景5：一次性紧急提醒

```cpp
// 30分钟后提醒关火
alarmManager.SetAlarm(1800, "turn_off_stove", RepeatType::ONCE);

// 1小时后提醒取快递
alarmManager.SetAlarm(3600, "pickup_package", RepeatType::ONCE);
```

## 周几位掩码快速参考

| 描述 | 位掩码（二进制） | 位掩码（十六进制） | 说明 |
|------|-----------------|-------------------|------|
| 周日 | 0b00000001 | 0x01 | bit0 |
| 周一 | 0b00000010 | 0x02 | bit1 |
| 周二 | 0b00000100 | 0x04 | bit2 |
| 周三 | 0b00001000 | 0x08 | bit3 |
| 周四 | 0b00010000 | 0x10 | bit4 |
| 周五 | 0b00100000 | 0x20 | bit5 |
| 周六 | 0b01000000 | 0x40 | bit6 |
| 工作日 | 0b00111110 | 0x3E | 周一到周五 |
| 周末 | 0b01000001 | 0x41 | 周六+周日 |
| 每天 | 0b01111111 | 0x7F | 所有天 |

**组合示例：**
```cpp
// 周一 + 周三 + 周五
uint8_t mwf = 0b00101010; // 或 0x2A

// 周二 + 周四
uint8_t tt = 0b00010100; // 或 0x14

// 周一到周五
uint8_t weekdays = 0b00111110; // 或 0x3E

// 周六 + 周日
uint8_t weekend = 0b01000001; // 或 0x41
```

## 重要注意事项

### 1. 时区设置

闹钟功能依赖系统时间（`time(NULL)` 和 `localtime()`），请确保：
- ESP32 已通过NTP或RTC同步正确时间
- 已设置正确的时区（使用 `setenv("TZ", "CST-8", 1); tzset();` for中国）

### 2. 闹钟数量限制

- 最多同时存在 **10个闹钟**
- 超过限制时 `SetAlarm` 系列方法会返回错误日志

### 3. 闹钟名称规则

- 名称长度：**1-64个字符**
- 名称必须唯一，同名会覆盖旧闹钟
- 建议使用有意义的英文名称

### 4. 重复闹钟行为

#### 正常触发
- **ONCE**: 触发后自动删除
- **DAILY/WEEKLY/WORKDAYS/WEEKENDS**: 触发后自动计算下次时间，永久保留

#### 错过闹钟处理
如果设备在闹钟应该触发时处于关机状态（例如：每日8点闹钟，但设备在10点才开机）：

- **ONCE类型**: 过期闹钟会被删除，不会触发
- **重复类型**: 
  - ✅ 系统启动时自动检测过期的重复闹钟
  - ✅ 自动重新调度到下一个有效时间（例如：错过今天8点，自动设置明天8点）
  - ⚠️ 错过的闹钟**不会补触发**，直接跳到下次
  - 📝 日志会显示: `Missed repeat alarm 'xxx', rescheduling to ...`

**示例**:
```
场景: 每日早上8:00闹钟，设备在10:00开机
结果: 闹钟自动调度到明天8:00，今天的不会触发
日志: W AlarmManager: Missed repeat alarm 'morning' (was at 1760356800), rescheduling to 1760443200
```

### 5. 持久化存储

所有闹钟存储在NVS（非易失性存储）中，占用字段：
```
alarm_0 ... alarm_9       // 闹钟名称
alarm_time_0 ... 9        // 触发时间戳
alarm_type_0 ... 9        // 重复类型
alarm_days_0 ... 9        // 周几掩码
alarm_en_0 ... 9          // 启用状态
```

### 6. 性能考虑

- 所有操作都有互斥锁保护，线程安全
- 使用 `std::unordered_map` 实现 O(1) 查找
- 闹钟触发通过ESP32硬件定时器实现，精度±100ms

### 7. 调试日志

所有操作都有ESP_LOG输出，TAG为 `"AlarmManager"`：
```cpp
ESP_LOGI(TAG, "Created daily alarm '%s' at %d:%02d", name, hour, minute);
ESP_LOGI(TAG, "Alarm '%s' triggered (type=%d)", name, type);
ESP_LOGI(TAG, "Repeat alarm '%s' rescheduled to %lld", name, next_time);
```

## 完整示例代码

```cpp
#include "AlarmClock.h"

void setup_alarms() {
    // 获取AlarmManager实例（假设从Board中获取）
    auto& alarmManager = GetAlarmManager();
    
    // 1. 每天早上7点起床闹钟
    alarmManager.SetDailyAlarm("wake_up", 7, 0);
    
    // 2. 工作日早上8:30上班提醒
    alarmManager.SetWorkdaysAlarm("work_start", 8, 30);
    
    // 3. 周末早上10点悠闲起床
    alarmManager.SetWeekendsAlarm("weekend_wake", 10, 0);
    
    // 4. 每周一、三、五下午6点健身
    uint8_t workout = 0b00101010;
    alarmManager.SetWeeklyAlarm("gym", 18, 0, workout);
    
    // 5. 15分钟后一次性提醒
    alarmManager.SetAlarm(900, "quick_reminder", RepeatType::ONCE);
    
    ESP_LOGI("APP", "All alarms configured");
}

void check_alarm() {
    auto& alarmManager = GetAlarmManager();
    
    if(alarmManager.IsRing()) {
        std::string alarm_name = alarmManager.get_now_alarm_name();
        ESP_LOGI("APP", "Alarm triggered: %s", alarm_name.c_str());
        
        // TODO: 播放铃声、显示提醒等
        
        // 清除标志
        alarmManager.ClearRing();
    }
}
```

## 常见问题（FAQ）

**Q: 如何修改已有闹钟的时间？**  
A: 使用相同的名称调用 `SetDailyAlarm` 或 `SetWeeklyAlarm`，会自动覆盖旧设置。

**Q: 闹钟触发时会做什么？**  
A: 设置 `ring_flag = true`，调用 `Display->SetStatus()` 显示名称，发送JSON消息到应用层。

**Q: 如何实现"响铃5分钟后自动停止"？**  
A: 在应用层检测到 `IsRing()` 后，启动一个5分钟定时器，超时后调用 `ClearRing()`。

**Q: 重复闹钟能否跳过某一次？**  
A: 使用 `EnableAlarm(name, false)` 临时禁用，之后再 `EnableAlarm(name, true)` 启用。

**Q: 如何实现"每隔N小时提醒"？**  
A: 当前API不直接支持，建议在应用层使用ONCE类型，触发后重新设置下一个。

---

**文档版本**: v2.0  
**最后更新**: 2025-10-13  
**维护者**: Cascade AI
