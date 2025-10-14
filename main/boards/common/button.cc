#include "button.h"

#include <esp_log.h>
#include <esp_timer.h>

static const char* TAG = "Button";
#if CONFIG_SOC_ADC_SUPPORTED
Button::Button(const button_adc_config_t& adc_cfg) {
    button_config_t button_config = {
        .type = BUTTON_TYPE_ADC,
        .long_press_time = 1000,
        .short_press_time = 50,
        .adc_button_config = adc_cfg
    };
    button_handle_ = iot_button_create(&button_config);
    if (button_handle_ == NULL) {
        ESP_LOGE(TAG, "Failed to create button handle");
        return;
    }
}
#endif

Button::Button(gpio_num_t gpio_num, bool active_high) : gpio_num_(gpio_num) {
    if (gpio_num == GPIO_NUM_NC) {
        return;
    }
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 1000,
        .short_press_time = 50,
        .gpio_button_config = {
            .gpio_num = gpio_num,
            .active_level = static_cast<uint8_t>(active_high ? 1 : 0)
        }
    };
    button_handle_ = iot_button_create(&button_config);
    if (button_handle_ == NULL) {
        ESP_LOGE(TAG, "Failed to create button handle");
        return;
    }
}

Button::~Button() {
    if (button_handle_ != NULL) {
        iot_button_delete(button_handle_);
    }
}

void Button::OnPressDown(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_down_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_PRESS_DOWN, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_press_down_) {
            button->on_press_down_();
        }
    }, this);
}

void Button::RegisterPressUpHandler() {
    if (press_up_registered_ || button_handle_ == nullptr) {
        return;
    }
    
    iot_button_register_cb(button_handle_, BUTTON_PRESS_UP, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        
        // 处理三击检测
        if (button->on_triple_click_) {
            int64_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
            
            // 检查是否在三击时间窗口内
            if (current_time - button->last_click_time_ < TRIPLE_CLICK_INTERVAL_MS) {
                button->click_count_++;
            } else {
                button->click_count_ = 1;
            }
            
            button->last_click_time_ = current_time;
            
            // 如果达到3次点击，触发三击回调
            if (button->click_count_ == 3) {
                button->click_count_ = 0;
                button->on_triple_click_();
                return; // 三击时不触发普通的press_up
            }
        }
        
        // 触发普通的press_up回调
        if (button->on_press_up_) {
            button->on_press_up_();
        }
    }, this);
    
    press_up_registered_ = true;
}

void Button::OnPressUp(std::function<void()> callback) {
    on_press_up_ = callback;
    RegisterPressUpHandler();
}

void Button::OnLongPress(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_long_press_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_LONG_PRESS_START, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_long_press_) {
            button->on_long_press_();
        }
    }, this);
}

void Button::OnClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_SINGLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_click_) {
            button->on_click_();
        }
    }, this);
}

void Button::OnDoubleClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_double_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_DOUBLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_double_click_) {
            button->on_double_click_();
        }
    }, this);
}

void Button::OnTripleClick(std::function<void()> callback) {
    on_triple_click_ = callback;
    RegisterPressUpHandler();
}
