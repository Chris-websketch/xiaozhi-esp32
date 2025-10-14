#include "iot/thing.h"
#include "AlarmClock.h"
#include "application.h"
#include <esp_log.h>
#if CONFIG_USE_ALARM
#define TAG "AlarmIot"
// extern AlarmManager* alarm_m_;
namespace iot {

// 这里仅定义 AlarmIot 的属性和方法，不包含具体的实现
class AlarmIot : public Thing {
public:
    AlarmIot() : Thing("Alarm", "一个闹钟, 可以定时提醒") {
        // 定义设备可以被远程执行的指令（统一接口）
        methods_.AddMethod("SetAlarm", "设置一个闹钟（统一接口）", ParameterList({
            Parameter("id", "闹钟ID", kValueTypeString, true),
            Parameter("repeat_type", "重复类型: 0=ONCE, 1=DAILY, 2=WEEKLY", kValueTypeNumber, true),
            Parameter("seconds", "ONCE类型使用：从现在开始的秒数；其他类型填0", kValueTypeNumber, true),
            Parameter("hour", "DAILY/WEEKLY使用：小时(0-23)；ONCE类型填0", kValueTypeNumber, true),
            Parameter("minute", "DAILY/WEEKLY使用：分钟(0-59)；ONCE类型填0", kValueTypeNumber, true),
            Parameter("repeat_days", "WEEKLY使用：位掩码(bit0=周日...bit6=周六)；其他类型填0", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            
            // 获取所有参数（统一字段）
            std::string id = parameters["id"].string();
            int repeat_type = parameters["repeat_type"].number();
            int seconds = parameters["seconds"].number();
            int hour = parameters["hour"].number();
            int minute = parameters["minute"].number();
            int repeat_days = parameters["repeat_days"].number();
            
            ESP_LOGI(TAG, "SetAlarm: id='%s', type=%d, sec=%d, time=%d:%02d, days=0x%02X", 
                     id.c_str(), repeat_type, seconds, hour, minute, repeat_days);
            
            // 调用统一的SetAlarm接口
            app.alarm_m_->SetAlarm(seconds, hour, minute, id, 
                                   static_cast<RepeatType>(repeat_type), 
                                   static_cast<uint8_t>(repeat_days));
        });

        // 添加取消闹钟方法
        methods_.AddMethod("CancelAlarm", "取消一个闹钟", ParameterList({
            Parameter("id", "要取消的闹钟ID", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            ESP_LOGI(TAG, "CancelAlarm");
            std::string id = parameters["id"].string();
            ESP_LOGI(TAG, "CancelAlarm with id: '%s'", id.c_str());
            app.alarm_m_->CancelAlarm(id);
            ESP_LOGI(TAG, "CancelAlarm command sent for alarm: %s", id.c_str());
        });
    }
};

} // namespace iot

DECLARE_THING(AlarmIot);
#endif

