#ifndef ALARMCLOCK_H
#define ALARMCLOCK_H

#include <string>
#include <vector>
#include <esp_log.h>
#include <esp_timer.h>
#include "time.h"
#include <mutex>
#include "settings.h"
#include <atomic>
#include <optional>
#include <unordered_map>

#if CONFIG_USE_ALARM

static constexpr int MAX_ALARMS = 10;

// 闹钟重复类型
enum class RepeatType : uint8_t {
    ONCE = 0,        // 一次性闹钟
    DAILY = 1,       // 每日重复
    WEEKLY = 2,      // 每周指定日期重复
    WORKDAYS = 3,    // 工作日（周一到周五）
    WEEKENDS = 4     // 周末（周六周日）
};

struct Alarm {
    std::string name;              // 闹钟名称
    time_t time;                   // 下一次触发的绝对时间戳
    RepeatType repeat_type;        // 重复类型
    uint8_t repeat_days;           // 位掩码：bit0=周日, bit1=周一...bit6=周六
    bool enabled;                  // 是否启用
};

class AlarmManager {
public:
    AlarmManager();
    ~AlarmManager();

    // 设置闹钟（支持重复类型）
    void SetAlarm(int seconds_from_now, std::string alarm_name, 
                  RepeatType repeat_type = RepeatType::ONCE, 
                  uint8_t repeat_days = 0);
    
    // 设置每日重复闹钟（指定时分）
    void SetDailyAlarm(const std::string& name, int hour, int minute);
    
    // 设置每周重复闹钟（自定义周几，使用位掩码）
    void SetWeeklyAlarm(const std::string& name, int hour, int minute, uint8_t weekdays);
    
    // 设置工作日闹钟（周一到周五）
    void SetWorkdaysAlarm(const std::string& name, int hour, int minute);
    
    // 设置周末闹钟（周六周日）
    void SetWeekendsAlarm(const std::string& name, int hour, int minute);
    
    // 启用/禁用闹钟（不删除）
    void EnableAlarm(const std::string& name, bool enable);
    
    // 取消指定名称的闹钟
    void CancelAlarm(std::string alarm_name);
    // 获取闹钟列表状态
    std::string GetAlarmsStatus();
    // 清除过时的闹钟
    void ClearOverdueAlarm(time_t now);
    // 获取从现在开始第一个响的闹钟
    std::optional<Alarm> GetProximateAlarm(time_t now);
    // 闹钟响了的处理函数
    void OnAlarm();
    // 闹钟是不是响了的标志位
    bool IsRing(){ return ring_flag; };
    // 清除闹钟标志位
    void ClearRing(){ESP_LOGI("Alarm", "clear");ring_flag = false;};
    std::string get_now_alarm_name(){return now_alarm_name;};

private:
    std::vector<Alarm> alarms_; // 闹钟列表
    std::unordered_map<std::string, int> name_to_slot_; // 名称到存储槽位的映射
    std::mutex mutex_; // 互斥锁，保护所有共享资源
    esp_timer_handle_t timer_; // 定时器
    std::string now_alarm_name; // 当前响的闹钟名字
    std::atomic<bool> ring_flag{false}; 
    std::atomic<bool> running_flag{false};
    Settings settings_; // 持久化存储对象
    
    // 辅助方法
    int FindFreeSlot(); // 查找空闲的存储槽位
    void SaveAlarmToSettings(const Alarm& alarm, int slot); // 保存闹钟到Settings
    void RemoveAlarmFromSettings(const std::string& name); // 从Settings删除闹钟
    void RestartTimerForNextAlarm(time_t now); // 重启定时器指向下一个闹钟
    std::optional<Alarm> GetProximateAlarmUnlocked(time_t now); // 内部使用：获取最近闹钟（不加锁）
    
    // 重复闹钟辅助方法
    time_t CalculateNextTriggerTime(const Alarm& alarm, time_t current_time); // 计算下一次触发时间
    time_t GetTodayTime(int hour, int minute); // 获取今天指定时分的时间戳
    time_t GetNextWeekdayTime(int hour, int minute, uint8_t weekdays, time_t from_time); // 获取下一个匹配星期的时间
    bool ShouldTrigger(const Alarm& alarm, time_t now); // 检查闹钟是否应该触发
};

#endif
#endif