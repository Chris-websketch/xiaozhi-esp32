#include "iot/thing.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "SubtitleControl"

namespace iot {

// 字幕控制IoT设备类
class SubtitleControl : public Thing {
private:
    bool subtitle_visible_ = true;  // 字幕是否可见，默认可见

public:
    SubtitleControl() : Thing("SubtitleControl", "字幕显示控制")  {
        // 定义属性：字幕可见性状态
        properties_.AddBooleanProperty("visible", "字幕是否可见", [this]() {
            return subtitle_visible_;
        });
        
        // 定义方法：显示字幕
        methods_.AddMethod("ShowSubtitle", "显示字幕", ParameterList(), [this](const ParameterList& parameters) {
            subtitle_visible_ = true;
            ApplySubtitleVisibility();
            ESP_LOGI(TAG, "字幕已显示");
        });
        
        // 定义方法：隐藏字幕
        methods_.AddMethod("HideSubtitle", "隐藏字幕", ParameterList(), [this](const ParameterList& parameters) {
            subtitle_visible_ = false;
            ApplySubtitleVisibility();
            ESP_LOGI(TAG, "字幕已隐藏");
        });
        
        // 定义方法：切换字幕显示状态
        methods_.AddMethod("ToggleSubtitle", "切换字幕显示状态", ParameterList(), [this](const ParameterList& parameters) {
            subtitle_visible_ = !subtitle_visible_;
            ApplySubtitleVisibility();
            ESP_LOGI(TAG, "字幕状态已切换为: %s", subtitle_visible_ ? "显示" : "隐藏");
        });
    }
    
private:
    // 应用字幕可见性设置到显示器
    void ApplySubtitleVisibility() {
        auto& board = Board::GetInstance();
        auto display = board.GetDisplay();
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示器对象");
            return;
        }
        
        // 使用Display的SetSubtitleEnabled方法控制字幕显示
        display->SetSubtitleEnabled(subtitle_visible_);
        ESP_LOGI(TAG, "字幕显示状态已设置为: %s", subtitle_visible_ ? "启用" : "禁用");
    }
};

} // namespace iot

DECLARE_THING(SubtitleControl);
