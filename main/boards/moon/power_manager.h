#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>


class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    gpio_num_t usb_detect_pin_ = GPIO_NUM_NC;
    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    uint32_t last_reported_battery_level_ = 0;
    bool is_charging_ = false;
    bool is_usb_connected_ = false;
    bool is_low_battery_ = false;
    int ticks_ = 0;
    const int kBatteryAdcInterval = 60;
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_oneshot_unit_handle_t adc_handle_;

    void CheckBatteryStatus() {
        // 检测USB连接状态（GPIO_NUM_3的USBCON信号）
        if (usb_detect_pin_ != GPIO_NUM_NC) {
            int usb_level = gpio_get_level(usb_detect_pin_);
            bool new_usb_status = usb_level == 1;  // 高电平表示USB已连接
            
            // 调试：每10秒输出一次当前USB检测状态
            static int usb_check_count = 0;
            if (++usb_check_count % 10 == 0) {
                ESP_LOGI("PowerManager", "USB检测 - GPIO[%d]=%d, 状态:%s", 
                         usb_detect_pin_, usb_level, new_usb_status ? "已连接" : "未连接");
            }
            
            if (new_usb_status != is_usb_connected_) {
                ESP_LOGI("PowerManager", "USB连接状态发生变化: %s -> %s (GPIO=%d)",
                        is_usb_connected_ ? "已连接" : "未连接",
                        new_usb_status ? "已连接" : "未连接",
                        usb_level);
                is_usb_connected_ = new_usb_status;
            }
        }
        
        // 使用GPIO_NUM_5检测充电状态（CHSTA信号）
        int gpio_level = gpio_get_level(charging_pin_);
        
        // GPIO_NUM_5的CHSTA信号：低电平表示正在充电，高电平表示未充电
        bool new_charging_status = gpio_level == 0;  // 低电平表示充电中
        
        if (new_charging_status != is_charging_) {
            ESP_LOGI("PowerManager", "充电状态发生变化: %s -> %s", 
                    is_charging_ ? "充电中" : "未充电", 
                    new_charging_status ? "充电中" : "未充电");
            is_charging_ = new_charging_status;
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据不足，则读取电池电量数据
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据充足，则每 kBatteryAdcInterval 个 tick 读取一次电池电量数据
        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    void ReadBatteryAdcData() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_0, &adc_value));
        
        // 将 ADC 值添加到队列中
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();

        // 定义电池电量区间 - 非线性映射：前60%电量消耗慢，后40%电量消耗快
        // 总ADC范围：410 (2380-1970) - 根据实际500mAh电池调整
        // 前60%电量(100%-40%)占用70%的ADC范围 = 287
        // 后40%电量(40%-0%)占用30%的ADC范围 = 123
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {1970, 0},   // 0%电量基准点
            {2039, 20},  // 20%电量，ADC差值69 (紧凑间隔，快速消耗)
            {2108, 40},  // 40%电量，ADC差值69 (紧凑间隔，快速消耗)
            {2189, 60},  // 60%电量，ADC差值81 (开始放宽间隔)
            {2280, 80},  // 80%电量，ADC差值91 (调整后的慢消耗区间)
            {2380, 100}  // 100%电量，ADC差值100 (根据实际满电ADC调整)
        };

        // 低于最低值时
        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        }
        // 高于最高值时
        else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        // Check low battery status
        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }

        // 只在电量值有显著变化时打印日志（变化超过5%）
        if (abs((int)battery_level_ - (int)last_reported_battery_level_) >= 5) {
            ESP_LOGI("PowerManager", "ADC value: %d average: %ld level: %ld", adc_value, average_adc, battery_level_);
            last_reported_battery_level_ = battery_level_;
        }
    }

public:
    PowerManager(gpio_num_t charging_pin, gpio_num_t usb_detect_pin = GPIO_NUM_NC) 
        : charging_pin_(charging_pin), usb_detect_pin_(usb_detect_pin) {
        // 初始化充电状态检测引脚
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // 启用内部上拉，解决浮空问题
        gpio_config(&io_conf);
        
        ESP_LOGI("PowerManager", "初始化充电状态检测引脚 GPIO[%d] (CHSTA)，启用内部上拉电阻", charging_pin_);
        
        // 初始化USB检测引脚
        if (usb_detect_pin_ != GPIO_NUM_NC) {
            gpio_config_t usb_conf = {};
            usb_conf.intr_type = GPIO_INTR_DISABLE;
            usb_conf.mode = GPIO_MODE_INPUT;
            usb_conf.pin_bit_mask = (1ULL << usb_detect_pin_);
            usb_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // 禁用内部下拉
            usb_conf.pull_up_en = GPIO_PULLUP_DISABLE;      // 禁用内部上拉（电路有外部电阻）
            gpio_config(&usb_conf);
            
            ESP_LOGI("PowerManager", "初始化USB检测引脚 GPIO[%d] (USBCON)，浮空输入模式（依赖外部电阻）", usb_detect_pin_);
        }

        // 创建电池电量检查定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        
        // 初始化后立即读取GPIO状态用于调试
        vTaskDelay(pdMS_TO_TICKS(100));  // 等待GPIO稳定
        int gpio_initial = gpio_get_level(charging_pin_);
        ESP_LOGI("PowerManager", "初始充电状态 - GPIO[%d] (CHSTA): %d", charging_pin_, gpio_initial);
        
        // 读取USB检测引脚状态
        if (usb_detect_pin_ != GPIO_NUM_NC) {
            int usb_initial = gpio_get_level(usb_detect_pin_);
            ESP_LOGI("PowerManager", "初始USB连接状态 - GPIO[%d] (USBCON): %d", usb_detect_pin_, usb_initial);
        }
        
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        // 初始化 ADC
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_0, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() {
        // 直接返回真实的充电状态，不受电量百分比影响
        // 即使电量100%，插上充电器时仍应显示充电图标
        return is_charging_;
    }

    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }
    
    bool IsUsbConnected() {
        // 返回USB连接状态（独立于充电状态）
        // 即使电池充满，只要USB插着就返回true
        return is_usb_connected_;
    }

    uint8_t GetBatteryLevel() {
        return battery_level_;
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
};
