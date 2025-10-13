# 闹钟功能更新日志

## v2.0.0 - 2025-10-13

### 🎉 重大功能更新

#### 错过闹钟自动恢复
- ✅ **智能恢复**：设备关机期间错过的重复闹钟，启动时自动重新调度到下次时间
- ✅ **数据持久化**：重复闹钟信息保存在NVS，重启后自动恢复
- ⚠️ **不补触发**：错过的闹钟不会补触发，直接跳到下次时间
- 📝 **日志提示**：`Missed repeat alarm 'xxx', rescheduling to ...`

**使用场景示例**：
```
场景：设置每日早上8:00的闹钟
情况1：设备正常运行 → 8:00准时触发，自动设置明天8:00
情况2：设备关机，10:00开机 → 自动检测并设置明天8:00（今天的不触发）
```

#### 新增重复闹钟功能
- ✅ **每日重复**：每天固定时间重复
- ✅ **每周重复**：自定义星期几重复（使用位掩码）
- ✅ **工作日模式**：周一到周五自动重复
- ✅ **周末模式**：周六周日自动重复
- ✅ **启用/禁用**：临时禁用闹钟而不删除

#### 新增API
```cpp
void SetDailyAlarm(const std::string& name, int hour, int minute);
void SetWeeklyAlarm(const std::string& name, int hour, int minute, uint8_t weekdays);
void SetWorkdaysAlarm(const std::string& name, int hour, int minute);
void SetWeekendsAlarm(const std::string& name, int hour, int minute);
void EnableAlarm(const std::string& name, bool enable);
```

#### 数据结构扩展
```cpp
enum class RepeatType : uint8_t {
    ONCE = 0,        // 一次性闹钟
    DAILY = 1,       // 每日重复
    WEEKLY = 2,      // 每周指定日期重复
    WORKDAYS = 3,    // 工作日
    WEEKENDS = 4     // 周末
};

struct Alarm {
    std::string name;
    time_t time;
    RepeatType repeat_type;  // 新增
    uint8_t repeat_days;     // 新增：位掩码
    bool enabled;            // 新增：启用状态
};
```

### 🔧 优化改进（v1.1）

#### 性能优化
- ✅ Settings对象改为成员变量，避免重复创建
- ✅ 添加 `name_to_slot_` 映射表，查找效率从O(n)提升到O(1)
- ✅ 消除重复的Settings遍历操作

#### 线程安全增强
- ✅ `GetProximateAlarm` 添加互斥锁保护
- ✅ 返回值改为 `std::optional<Alarm>`，避免悬空指针
- ✅ 所有共享资源访问统一加锁

#### 代码质量提升
- ✅ 定义 `MAX_ALARMS = 10` 常量，消除魔数
- ✅ 提取辅助方法，减少代码重复：
  - `FindFreeSlot()` - 查找空闲槽位
  - `SaveAlarmToSettings()` - 保存闹钟
  - `RemoveAlarmFromSettings()` - 删除闹钟
  - `RestartTimerForNextAlarm()` - 重启定时器

#### 健壮性增强
- ✅ 参数验证（时间、名称长度）
- ✅ `sprintf` 改为 `snprintf` 防止缓冲区溢出
- ✅ 时间戳溢出防护（使用LL后缀）
- ✅ 格式化字符串修复（%d → %zu）

### 📚 文档

新增完整API文档：
- `ALARM_CLOCK_API.md` - 详细API使用说明
- `CHANGELOG.md` - 更新日志（本文件）

### 🔄 向后兼容

- ✅ 现有 `SetAlarm` API 保持兼容（通过默认参数）
- ✅ 旧闹钟数据自动迁移（默认为ONCE类型）
- ✅ 旧代码无需修改即可继续工作

### ⚠️ 注意事项

1. **存储格式变化**：新增了3个字段（键名已优化以符合NVS 15字符限制）
   - `alarm_type_X` (重复类型)
   - `alarm_days_X` (周几掩码)
   - `alarm_en_X` (启用状态)

2. **行为变化**：
   - ONCE类型闹钟触发后自动删除（与之前相同）
   - 重复类型闹钟触发后自动计算下次时间并保留

3. **时区依赖**：
   - 请确保系统时间已正确配置
   - 重复闹钟依赖本地时间计算

### 🎯 使用示例

```cpp
// 每天早上8点
alarmManager.SetDailyAlarm("morning", 8, 0);

// 工作日早上7点
alarmManager.SetWorkdaysAlarm("work", 7, 0);

// 周末早上10点
alarmManager.SetWeekendsAlarm("weekend", 10, 0);

// 周一、周三、周五下午3点
uint8_t mwf = 0b00101010;
alarmManager.SetWeeklyAlarm("gym", 15, 0, mwf);

// 临时禁用
alarmManager.EnableAlarm("morning", false);

// 重新启用
alarmManager.EnableAlarm("morning", true);
```

---

## v1.0.0 - Initial Release

- 基础闹钟功能
- 一次性触发
- 持久化存储
- 最多10个闹钟

---

**维护者**: Cascade AI  
**项目地址**: xiaozhi-esp32/main/AlarmClock
