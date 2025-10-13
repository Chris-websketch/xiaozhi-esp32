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
        // 定义设备的属性
        properties_.AddStringProperty("Alarm_List", "当前闹钟的描述", [this]() -> std::string {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                return std::string("AlarmManager is nullptr");
            }
            ESP_LOGI(TAG, "Alarm_List %s", app.alarm_m_->GetAlarmsStatus().c_str());
            return app.alarm_m_->GetAlarmsStatus();
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetAlarm", "设置一个闹钟", ParameterList({
            Parameter("second_from_now", "闹钟多少秒以后响", kValueTypeNumber, true),
            Parameter("alarm_name", "闹钟的描述(名字)", kValueTypeString, true),
            Parameter("repeat_type", "重复类型: 0=ONCE, 1=DAILY, 2=WEEKLY, 3=WORKDAYS, 4=WEEKENDS", kValueTypeNumber, false),
            Parameter("repeat_days", "周几掩码(仅WEEKLY使用): bit0=周日,bit1=周一...bit6=周六", kValueTypeNumber, false)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            ESP_LOGI(TAG, "SetAlarm");
            int second_from_now = parameters["second_from_now"].number();
            std::string alarm_name = parameters["alarm_name"].string();
            int repeat_type = 0;  // 默认ONCE
            int repeat_days = 0;
            
            // 检查是否提供了可选参数
            try {
                repeat_type = parameters["repeat_type"].number();
            } catch(...) {}
            try {
                repeat_days = parameters["repeat_days"].number();
            } catch(...) {}
            
            ESP_LOGI(TAG, "SetAlarm with name: '%s', seconds: %d, type: %d, days: 0x%02X", 
                     alarm_name.c_str(), second_from_now, repeat_type, repeat_days);
            app.alarm_m_->SetAlarm(second_from_now, alarm_name, 
                                   static_cast<RepeatType>(repeat_type), 
                                   static_cast<uint8_t>(repeat_days));
        });

        // 添加取消闹钟方法
        methods_.AddMethod("CancelAlarm", "取消一个闹钟", ParameterList({
            Parameter("alarm_name", "要取消的闹钟名称", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            if(app.alarm_m_ == nullptr){
                ESP_LOGE(TAG, "AlarmManager is nullptr");
                return;
            }
            ESP_LOGI(TAG, "CancelAlarm");
            std::string alarm_name = parameters["alarm_name"].string();
            ESP_LOGI(TAG, "CancelAlarm with name: '%s'", alarm_name.c_str());
            app.alarm_m_->CancelAlarm(alarm_name);
            ESP_LOGI(TAG, "CancelAlarm command sent for alarm: %s", alarm_name.c_str());
        });
    }
};

} // namespace iot

DECLARE_THING(AlarmIot);
#endif

