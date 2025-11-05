#include "iot/thing.h"
#include "application.h"
#include <esp_log.h>

#define TAG "Telemetry"

namespace iot {

class Telemetry : public Thing {
public:
    Telemetry() : Thing("Telemetry", "遥测数据上报") {
        // 定义 uplink 方法，用于立即触发一次状态上报
        methods_.AddMethod("uplink", "立即上报遥测数据", ParameterList(), 
            [this](const ParameterList& parameters) {
                ESP_LOGI(TAG, "收到 uplink 指令，立即上报遥测数据");
                Application::GetInstance().TriggerMqttUplink();
            });
    }
};

} // namespace iot

DECLARE_THING(Telemetry);
