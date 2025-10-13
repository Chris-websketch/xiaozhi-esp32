#include "AlarmClock.h"
#include "board.h"
#include "display.h"
#define TAG "AlarmManager"

#if CONFIG_USE_ALARM
// 公开接口：加锁版本
std::optional<Alarm> AlarmManager::GetProximateAlarm(time_t now){
    std::lock_guard<std::mutex> lock(mutex_);
    return GetProximateAlarmUnlocked(now);
}

// 内部方法：不加锁版本（调用者已持有锁）
std::optional<Alarm> AlarmManager::GetProximateAlarmUnlocked(time_t now){
    std::optional<Alarm> current_alarm;
    for(const auto& alarm : alarms_){
        // 只考虑启用的且时间在未来的闹钟
        if(alarm.enabled && alarm.time > now && 
           (!current_alarm.has_value() || alarm.time < current_alarm->time)){
            current_alarm = alarm; // 获取当前时间以后第一个发生的时钟句柄
        }
    }
    return current_alarm;
}
/// @brief 删除超过时间的一次性闹钟（重复闹钟不删除）
/// @param now 
void AlarmManager::ClearOverdueAlarm(time_t now){
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto it = alarms_.begin(); it != alarms_.end();){
        // 只删除ONCE类型且已过期的闹钟
        if(it->time <= now && it->repeat_type == RepeatType::ONCE){
            ESP_LOGI(TAG, "Removing overdue ONCE alarm '%s' at %lld", it->name.c_str(), (long long)it->time);
            RemoveAlarmFromSettings(it->name);
            it = alarms_.erase(it); // 删除过期的闹钟, 此时it指向下一个元素
        }else{
            it++;
        }
    }
}

AlarmManager::AlarmManager() : settings_("alarm_clock", true) {
    ESP_LOGI(TAG, "AlarmManager init");
    ring_flag = false;
    running_flag = false;

    // 从Setting里面读取闹钟列表并构建映射
    for(int i = 0; i < MAX_ALARMS; i++){
        std::string alarm_name = settings_.GetString("alarm_" + std::to_string(i));
        if(!alarm_name.empty()){
            Alarm alarm;
            alarm.name = alarm_name;
            alarm.time = settings_.GetInt("alarm_time_" + std::to_string(i));
            // 加载重复类型字段（默认为ONCE以兼容旧数据）
            alarm.repeat_type = static_cast<RepeatType>(settings_.GetInt("alarm_type_" + std::to_string(i), 0));
            alarm.repeat_days = static_cast<uint8_t>(settings_.GetInt("alarm_days_" + std::to_string(i), 0));
            alarm.enabled = settings_.GetInt("alarm_en_" + std::to_string(i), 1) != 0; // 默认启用
            
            alarms_.push_back(alarm);
            name_to_slot_[alarm_name] = i; // 构建映射
            ESP_LOGI(TAG, "Alarm %s loaded: time=%lld, type=%d, days=0x%02X, enabled=%d (slot %d)", 
                     alarm.name.c_str(), (long long)alarm.time, (int)alarm.repeat_type, 
                     alarm.repeat_days, alarm.enabled, i);
        }
    }

    // 建立一个时钟
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            AlarmManager* alarm_manager = (AlarmManager*)arg;
            alarm_manager->OnAlarm(); // 闹钟响了
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "alarm_timer",
        .skip_unhandled_events = false
    };
    esp_timer_create(&timer_args, &timer_);
    time_t now = time(NULL);
    // 获取最近的闹钟, 同时清除过期的闹钟
    printf("now: %lld\n", (long long)now);

    // 修复过期的重复闹钟（设备关机期间错过的闹钟）
    for(auto& alarm : alarms_) {
        if(alarm.repeat_type != RepeatType::ONCE && alarm.time <= now && alarm.enabled) {
            time_t new_time = CalculateNextTriggerTime(alarm, now);
            ESP_LOGW(TAG, "Missed repeat alarm '%s' (was at %lld), rescheduling to %lld", 
                     alarm.name.c_str(), (long long)alarm.time, (long long)new_time);
            alarm.time = new_time;
            
            // 更新存储
            auto it = name_to_slot_.find(alarm.name);
            if(it != name_to_slot_.end()) {
                SaveAlarmToSettings(alarm, it->second);
            }
        }
    }

    ClearOverdueAlarm(now);

    auto next_alarm = GetProximateAlarm(now);
    // 启动闹钟
    if(next_alarm.has_value()){
        int64_t new_timer_time = next_alarm->time - now;  // P0修复：使用int64_t防止溢出
        
        // 检查时间有效性
        if(new_timer_time <= 0) {
            ESP_LOGW(TAG, "Timer duration is non-positive: %lld, skipping", (long long)new_timer_time);
            return;
        }
        
        ESP_LOGI(TAG, "begin a alarm at %lld", (long long)new_timer_time);
        // P0修复：安全转换为微秒，防止溢出
        uint64_t microseconds = static_cast<uint64_t>(new_timer_time) * 1000000ULL;
        esp_timer_start_once(timer_, microseconds);
        running_flag = true;
    }
}


AlarmManager::~AlarmManager(){
    if(timer_ != nullptr){
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}


void AlarmManager::SetAlarm(int seconds_from_now, std::string alarm_name, 
                            RepeatType repeat_type, uint8_t repeat_days){
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 参数验证
    if(seconds_from_now <= 0){
        ESP_LOGE(TAG, "Invalid alarm time: %d", seconds_from_now);
        return;
    }
    
    if(alarm_name.empty() || alarm_name.length() > 64){
        ESP_LOGE(TAG, "Invalid alarm name");
        return;
    }

    time_t now = time(NULL);
    time_t new_time = now + seconds_from_now;
    
    // 检查是否已存在同名闹钟
    bool found_existing = false;
    for(auto& alarm : alarms_){
        if(alarm.name == alarm_name){
            ESP_LOGI(TAG, "Found existing alarm '%s', updating", alarm_name.c_str());
            
            // 更新现有闹钟
            alarm.time = new_time;
            alarm.repeat_type = repeat_type;
            alarm.repeat_days = repeat_days;
            alarm.enabled = true;
            
            // 更新存储（使用映射快速定位）
            auto it = name_to_slot_.find(alarm_name);
            if(it != name_to_slot_.end()){
                SaveAlarmToSettings(alarm, it->second);
                ESP_LOGI(TAG, "Updated alarm in slot %d", it->second);
            }
            found_existing = true;
            break;
        }
    }
    
    // 如果没有找到同名闹钟，创建新闹钟
    if(!found_existing){
        if(alarms_.size() >= MAX_ALARMS){
            ESP_LOGE(TAG, "Too many alarms (max: %d)", MAX_ALARMS);
            return;
        }
        
        // 查找空闲槽位
        int slot = FindFreeSlot();
        if(slot < 0){
            ESP_LOGE(TAG, "No free slot available");
            return;
        }
        
        Alarm alarm;
        alarm.name = alarm_name;
        alarm.time = new_time;
        alarm.repeat_type = repeat_type;
        alarm.repeat_days = repeat_days;
        alarm.enabled = true;
        alarms_.push_back(alarm);
        
        // 保存到存储并更新映射
        SaveAlarmToSettings(alarm, slot);
        ESP_LOGI(TAG, "Created new alarm '%s' type=%d (slot %d)", 
                 alarm_name.c_str(), (int)repeat_type, slot);
    }

    // 重启定时器指向最近的闹钟
    RestartTimerForNextAlarm(now);
}

void AlarmManager::CancelAlarm(std::string alarm_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool found = false;
    
    ESP_LOGI(TAG, "开始取消闹钟: %s", alarm_name.c_str());
    
    // 从内存中删除指定名称的闹钟
    for(auto it = alarms_.begin(); it != alarms_.end();) {
        if(it->name == alarm_name) {
            ESP_LOGI(TAG, "从内存中移除闹钟: %s (时间: %lld)", alarm_name.c_str(), (long long)it->time);
            
            // 从存储中删除（使用映射快速定位）
            RemoveAlarmFromSettings(alarm_name);
            
            it = alarms_.erase(it); // 删除闹钟并更新迭代器
            found = true;
            // 注意：不使用break，因为要删除所有同名闹钟（虽然理论上不应该有）
        } else {
            ++it;
        }
    }
    
    if (!found) {
        ESP_LOGW(TAG, "未找到名为 %s 的闹钟", alarm_name.c_str());
        return;
    }
    
    // 输出所有剩余闹钟的信息，用于调试
    ESP_LOGI(TAG, "剩余闹钟列表 (共 %zu 个):", alarms_.size());
    for(const auto& alarm : alarms_) {
        ESP_LOGI(TAG, "  - %s (时间: %lld)", alarm.name.c_str(), (long long)alarm.time);
    }
    
    // 重置定时器，使其指向下一个闹钟
    time_t now = time(NULL);
    RestartTimerForNextAlarm(now);
    
    ESP_LOGI(TAG, "闹钟 %s 已成功取消", alarm_name.c_str());
}

void AlarmManager::OnAlarm(){
    std::lock_guard<std::mutex> lock(mutex_);  // P0修复：添加并发保护
    
    ESP_LOGI(TAG, "=----闹钟触发----=");
    ring_flag = true;
    
    // 闹钟触发后，CustomBoard 的监听器会自动检测并处理超级省电模式唤醒
    
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    // 找到当前触发的闹钟并处理重复逻辑
    time_t now = time(NULL);
    std::string triggered_name;
    
    for(auto& alarm : alarms_){
        if(alarm.enabled && alarm.time <= now){
            triggered_name = alarm.name;
            
            // 显示闹钟信息
            char message_buf_send[256];
            snprintf(message_buf_send, sizeof(message_buf_send), 
                     "{\"type\":\"listen\",\"state\":\"detect\",\"text\":\"闹钟-#%s\",\"source\":\"text\"}", 
                     alarm.name.c_str());
            now_alarm_name = message_buf_send;
            display->SetStatus(alarm.name.c_str());
            
            ESP_LOGI(TAG, "Alarm '%s' triggered (type=%d)", alarm.name.c_str(), (int)alarm.repeat_type);
            
            // 如果是重复闹钟，计算下一次触发时间
            if(alarm.repeat_type != RepeatType::ONCE){
                time_t next_time = CalculateNextTriggerTime(alarm, now);
                alarm.time = next_time;
                
                // 更新存储
                auto it = name_to_slot_.find(alarm.name);
                if(it != name_to_slot_.end()){
                    SaveAlarmToSettings(alarm, it->second);
                    ESP_LOGI(TAG, "Repeat alarm '%s' rescheduled to %lld", 
                             alarm.name.c_str(), (long long)next_time);
                }
            }
            break; // 只处理第一个触发的闹钟
        }
    }
    
    // 清理过期的一次性闹钟（需要使用不加锁版本，因为已持有锁）
    // 直接在这里实现，避免递归加锁
    for(auto it = alarms_.begin(); it != alarms_.end();){
        if(it->time <= now && it->repeat_type == RepeatType::ONCE){
            ESP_LOGI(TAG, "Removing overdue ONCE alarm '%s' at %lld", it->name.c_str(), (long long)it->time);
            RemoveAlarmFromSettings(it->name);
            it = alarms_.erase(it);
        }else{
            it++;
        }
    }
    // 设置下一个闹钟
    RestartTimerForNextAlarm(now);
}

std::string AlarmManager::GetAlarmsStatus(){
    std::lock_guard<std::mutex> lock(mutex_);
    std::string status;
    for(auto& alarm : alarms_){
        if(!status.empty()){
            status += "; ";
        }
        status += alarm.name + " at " + std::to_string(alarm.time);
    }
    return status;
}

// 辅助方法实现

int AlarmManager::FindFreeSlot() {
    for(int i = 0; i < MAX_ALARMS; i++) {
        if(settings_.GetString("alarm_" + std::to_string(i)).empty()) {
            return i;
        }
    }
    return -1; // 无可用槽位
}

void AlarmManager::SaveAlarmToSettings(const Alarm& alarm, int slot) {
    settings_.SetString("alarm_" + std::to_string(slot), alarm.name);
    settings_.SetInt("alarm_time_" + std::to_string(slot), alarm.time);
    settings_.SetInt("alarm_type_" + std::to_string(slot), static_cast<int>(alarm.repeat_type));
    settings_.SetInt("alarm_days_" + std::to_string(slot), alarm.repeat_days);
    settings_.SetInt("alarm_en_" + std::to_string(slot), alarm.enabled ? 1 : 0);
    name_to_slot_[alarm.name] = slot;
    ESP_LOGI(TAG, "Saved alarm '%s' to slot %d", alarm.name.c_str(), slot);
}

void AlarmManager::RemoveAlarmFromSettings(const std::string& name) {
    auto it = name_to_slot_.find(name);
    if(it != name_to_slot_.end()) {
        int slot = it->second;
        settings_.SetString("alarm_" + std::to_string(slot), "");
        settings_.SetInt("alarm_time_" + std::to_string(slot), 0);
        name_to_slot_.erase(it);
        ESP_LOGI(TAG, "Removed alarm '%s' from slot %d", name.c_str(), slot);
    }
}

void AlarmManager::RestartTimerForNextAlarm(time_t now) {
    // 注意：调用者已持有mutex_锁，使用不加锁版本避免死锁
    // 停止当前定时器
    if(running_flag) {
        esp_timer_stop(timer_);
        running_flag = false;
        ESP_LOGI(TAG, "Stopped current timer");
    }
    
    // 获取下一个闹钟（使用不加锁版本）
    auto next_alarm = GetProximateAlarmUnlocked(now);
    if(next_alarm.has_value()) {
        int seconds_from_now = next_alarm->time - now;
        if(seconds_from_now > 0) {
            ESP_LOGI(TAG, "Setting timer for alarm '%s' in %d seconds", 
                     next_alarm->name.c_str(), seconds_from_now);
            esp_timer_start_once(timer_, static_cast<uint64_t>(seconds_from_now) * 1000000LL);
            running_flag = true;
        } else {
            ESP_LOGW(TAG, "Next alarm time is in the past, skipping");
        }
    } else {
        ESP_LOGI(TAG, "No more alarms scheduled");
    }
}

// ========== 新增重复闹钟API实现 ==========

void AlarmManager::SetDailyAlarm(const std::string& name, int hour, int minute) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if(hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        ESP_LOGE(TAG, "Invalid time: %d:%d", hour, minute);
        return;
    }
    
    time_t alarm_time = GetTodayTime(hour, minute);
    time_t now = time(NULL);
    
    // 如果今天的时间已过，设置为明天
    if(alarm_time <= now) {
        alarm_time += 86400; // 加24小时
    }
    
    // 检查是否已存在同名闹钟
    bool found = false;
    for(auto& alarm : alarms_) {
        if(alarm.name == name) {
            alarm.time = alarm_time;
            alarm.repeat_type = RepeatType::DAILY;
            alarm.repeat_days = 0x7F; // 每天：bit0-6都设置
            alarm.enabled = true;
            
            auto it = name_to_slot_.find(name);
            if(it != name_to_slot_.end()) {
                SaveAlarmToSettings(alarm, it->second);
            }
            found = true;
            ESP_LOGI(TAG, "Updated daily alarm '%s' at %d:%02d", name.c_str(), hour, minute);
            break;
        }
    }
    
    if(!found) {
        if(alarms_.size() >= MAX_ALARMS) {
            ESP_LOGE(TAG, "Too many alarms");
            return;
        }
        
        int slot = FindFreeSlot();
        if(slot < 0) {
            ESP_LOGE(TAG, "No free slot");
            return;
        }
        
        Alarm alarm;
        alarm.name = name;
        alarm.time = alarm_time;
        alarm.repeat_type = RepeatType::DAILY;
        alarm.repeat_days = 0x7F;
        alarm.enabled = true;
        alarms_.push_back(alarm);
        
        SaveAlarmToSettings(alarm, slot);
        ESP_LOGI(TAG, "Created daily alarm '%s' at %d:%02d", name.c_str(), hour, minute);
    }
    
    RestartTimerForNextAlarm(now);
}

void AlarmManager::SetWeeklyAlarm(const std::string& name, int hour, int minute, uint8_t weekdays) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if(hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        ESP_LOGE(TAG, "Invalid time: %d:%d", hour, minute);
        return;
    }
    
    if(weekdays == 0 || weekdays > 0x7F) {
        ESP_LOGE(TAG, "Invalid weekdays mask: 0x%02X", weekdays);
        return;
    }
    
    time_t now = time(NULL);
    time_t alarm_time = GetNextWeekdayTime(hour, minute, weekdays, now);
    
    // 检查是否已存在同名闹钟
    bool found = false;
    for(auto& alarm : alarms_) {
        if(alarm.name == name) {
            alarm.time = alarm_time;
            alarm.repeat_type = RepeatType::WEEKLY;
            alarm.repeat_days = weekdays;
            alarm.enabled = true;
            
            auto it = name_to_slot_.find(name);
            if(it != name_to_slot_.end()) {
                SaveAlarmToSettings(alarm, it->second);
            }
            found = true;
            ESP_LOGI(TAG, "Updated weekly alarm '%s' at %d:%02d, days=0x%02X", 
                     name.c_str(), hour, minute, weekdays);
            break;
        }
    }
    
    if(!found) {
        if(alarms_.size() >= MAX_ALARMS) {
            ESP_LOGE(TAG, "Too many alarms");
            return;
        }
        
        int slot = FindFreeSlot();
        if(slot < 0) {
            ESP_LOGE(TAG, "No free slot");
            return;
        }
        
        Alarm alarm;
        alarm.name = name;
        alarm.time = alarm_time;
        alarm.repeat_type = RepeatType::WEEKLY;
        alarm.repeat_days = weekdays;
        alarm.enabled = true;
        alarms_.push_back(alarm);
        
        SaveAlarmToSettings(alarm, slot);
        ESP_LOGI(TAG, "Created weekly alarm '%s' at %d:%02d, days=0x%02X", 
                 name.c_str(), hour, minute, weekdays);
    }
    
    RestartTimerForNextAlarm(now);
}

void AlarmManager::SetWorkdaysAlarm(const std::string& name, int hour, int minute) {
    // 工作日：周一到周五，bit1-5
    uint8_t workdays = 0b00111110; // bit1=周一, bit2=周二...bit5=周五
    SetWeeklyAlarm(name, hour, minute, workdays);
    ESP_LOGI(TAG, "Set workdays alarm '%s'", name.c_str());
}

void AlarmManager::SetWeekendsAlarm(const std::string& name, int hour, int minute) {
    // 周末：周六周日，bit0和bit6
    uint8_t weekends = 0b01000001; // bit0=周日, bit6=周六
    SetWeeklyAlarm(name, hour, minute, weekends);
    ESP_LOGI(TAG, "Set weekends alarm '%s'", name.c_str());
}

void AlarmManager::EnableAlarm(const std::string& name, bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool found = false;
    for(auto& alarm : alarms_) {
        if(alarm.name == name) {
            alarm.enabled = enable;
            
            // 更新存储
            auto it = name_to_slot_.find(name);
            if(it != name_to_slot_.end()) {
                settings_.SetInt("alarm_en_" + std::to_string(it->second), enable ? 1 : 0);
            }
            
            found = true;
            ESP_LOGI(TAG, "Alarm '%s' %s", name.c_str(), enable ? "enabled" : "disabled");
            break;
        }
    }
    
    if(!found) {
        ESP_LOGW(TAG, "Alarm '%s' not found", name.c_str());
        return;
    }
    
    // 重新计算定时器
    time_t now = time(NULL);
    RestartTimerForNextAlarm(now);
}

// ========== 重复闹钟辅助方法实现 ==========

time_t AlarmManager::GetTodayTime(int hour, int minute) {
    time_t now = time(NULL);
    struct tm tm_now;  // P0修复：线程安全
    localtime_r(&now, &tm_now);
    
    tm_now.tm_hour = hour;
    tm_now.tm_min = minute;
    tm_now.tm_sec = 0;
    
    return mktime(&tm_now);
}

time_t AlarmManager::GetNextWeekdayTime(int hour, int minute, uint8_t weekdays, time_t from_time) {
    // 从今天开始检查（最多检查7天）
    for(int day_offset = 0; day_offset <= 7; day_offset++) {
        time_t candidate = from_time + day_offset * 86400;
        struct tm tm_candidate;  // P0修复：线程安全
        localtime_r(&candidate, &tm_candidate);
        
        // 检查这一天是否在weekdays掩码中
        int weekday_bit = 1 << tm_candidate.tm_wday;
        if(weekdays & weekday_bit) {
            // 设置为指定时分
            tm_candidate.tm_hour = hour;
            tm_candidate.tm_min = minute;
            tm_candidate.tm_sec = 0;
            
            time_t result = mktime(&tm_candidate);
            
            // 如果是今天，检查时间是否已过
            if(day_offset == 0 && result <= from_time) {
                continue; // 今天时间已过，继续检查明天
            }
            
            return result;
        }
    }
    
    // 理论上不应该到这里（除非weekdays为0，但已经检查过了）
    return from_time + 86400; // 返回明天同一时间
}

time_t AlarmManager::CalculateNextTriggerTime(const Alarm& alarm, time_t current_time) {
    struct tm tm_alarm;  // P0修复：线程安全
    localtime_r(&alarm.time, &tm_alarm);
    int hour = tm_alarm.tm_hour;
    int minute = tm_alarm.tm_min;
    
    switch(alarm.repeat_type) {
        case RepeatType::DAILY: {
            // P1修复：正确处理DST，而非固定+86400
            struct tm tm_next = tm_alarm;
            tm_next.tm_mday += 1;  // 加1天，mktime会自动处理DST
            return mktime(&tm_next);
        }
            
        case RepeatType::WEEKLY:
            // 每周指定日期重复
            return GetNextWeekdayTime(hour, minute, alarm.repeat_days, current_time);
            
        case RepeatType::WORKDAYS:
            // 工作日
            return GetNextWeekdayTime(hour, minute, 0b00111110, current_time);
            
        case RepeatType::WEEKENDS:
            // 周末
            return GetNextWeekdayTime(hour, minute, 0b01000001, current_time);
            
        case RepeatType::ONCE:
        default:
            // 一次性闹钟不应该调用这个函数
            return alarm.time;
    }
    // P0修复：添加括号匹配
}

bool AlarmManager::ShouldTrigger(const Alarm& alarm, time_t now) {
    return alarm.enabled && alarm.time <= now;
}

#endif