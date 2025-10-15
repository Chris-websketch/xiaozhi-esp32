#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_manager.h"
#include "iot_image_display.h"
#include "image_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>
#include <cstring>

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "iot/things/music_player.h" 

#define TAG "XINGZHI_CUBE_1_54TFT_WIFI"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);
LV_FONT_DECLARE(time50);

// 声明使用的全局变量
extern "C" {
    extern volatile iot::ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;
}

// 自定义LCD显示类，支持进度UI
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    // 构造函数
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width, int height,
                    int offset_x, int offset_y,
                    bool mirror_x, bool mirror_y, bool swap_xy,
                    DisplayFonts fonts)
        : SpiLcdDisplay(io_handle, panel_handle, width, height,
                       offset_x, offset_y, mirror_x, mirror_y, swap_xy, fonts) {
        // 优化：将消息显示位置下移到屏幕下半部分，避免遮挡图像
        DisplayLockGuard lock(this);
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_pad_top(chat_message_label_, 100, 0);
            ESP_LOGI(TAG, "消息显示位置已调整到屏幕下半部分");
        }
    }

    // 更新下载进度UI
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        DisplayLockGuard lock(this);
        
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        
        if (download_progress_container_ == nullptr) return;
        
        if (show) {
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            // 更新进度条
            if (download_progress_arc_) {
                lv_arc_set_value(download_progress_arc_, progress);
                
                // 颜色渐变
                if (progress < 30) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
                } else if (progress < 70) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FFB3), LV_PART_INDICATOR);
                } else {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FF7F), LV_PART_INDICATOR);
                }
            }
            
            // 更新百分比
            if (download_progress_label_) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            
            // 更新消息
            if (message && message_label_) {
                if (strstr(message, "下载")) {
                    lv_label_set_text(message_label_, progress == 100 ? "下载完成" : "正在下载资源");
                } else if (strstr(message, "删除")) {
                    lv_label_set_text(message_label_, "正在清理旧文件");
                } else if (strstr(message, "准备")) {
                    lv_label_set_text(message_label_, "正在准备下载");
                } else {
                    char simplified_msg[64];
                    strncpy(simplified_msg, message, sizeof(simplified_msg) - 1);
                    simplified_msg[sizeof(simplified_msg) - 1] = '\0';
                    lv_label_set_text(message_label_, simplified_msg);
                }
            }
            
            lv_obj_clear_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(download_progress_container_);
        } else {
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // 显示待机时钟页面
    void ShowIdleClock();
    
    // 隐藏待机时钟页面
    void HideIdleClock();
    
    // 更新预加载进度UI
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        DisplayLockGuard lock(this);
        
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
        }
        
        if (preload_progress_container_ == nullptr) return;
        
        if (show) {
            if (preload_progress_arc_ && total > 0) {
                int progress_value = (current * 100) / total;
                if (progress_value > 100) progress_value = 100;
                if (progress_value < 0) progress_value = 0;
                lv_arc_set_value(preload_progress_arc_, progress_value);
                lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
            }
            
            if (preload_message_label_) {
                lv_label_set_text(preload_message_label_, "设备正在预热中...");
            }
            
            lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(preload_progress_container_);
        } else {
            lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

public:
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_arc_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;
    
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_arc_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    
    // 待机时钟页面UI元素
    lv_obj_t* idle_clock_container_ = nullptr;
    lv_obj_t* idle_time_label_ = nullptr;        // 时:分
    lv_obj_t* idle_second_label_ = nullptr;      // 秒
    lv_obj_t* idle_date_label_ = nullptr;        // 日期
    lv_obj_t* idle_weekday_label_ = nullptr;     // 星期
    lv_timer_t* idle_clock_timer_ = nullptr;     // 时钟更新定时器

private:

    void CreateDownloadProgressUI() {
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(download_progress_container_);
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(download_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(download_progress_container_, 0, 0);

        download_progress_arc_ = lv_arc_create(download_progress_container_);
        lv_obj_set_size(download_progress_arc_, 120, 120);
        lv_arc_set_rotation(download_progress_arc_, 270);
        lv_arc_set_bg_angles(download_progress_arc_, 0, 360);
        lv_arc_set_value(download_progress_arc_, 0);
        lv_obj_align(download_progress_arc_, LV_ALIGN_CENTER, 0, 0);
        
        lv_obj_set_style_arc_width(download_progress_arc_, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_arc_width(download_progress_arc_, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(download_progress_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(download_progress_arc_, LV_OBJ_FLAG_CLICKABLE);

        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_black(), 0);
        lv_label_set_text(download_progress_label_, "0%");
        lv_obj_align_to(download_progress_label_, download_progress_arc_, LV_ALIGN_CENTER, 0, 0);

        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(message_label_, lv_pct(80));
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(message_label_, "正在准备下载资源...");
        lv_obj_align_to(message_label_, download_progress_arc_, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        lv_obj_move_foreground(download_progress_container_);
    }

    void CreatePreloadProgressUI() {
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(preload_progress_container_);
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preload_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(preload_progress_container_, 0, 0);
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        preload_progress_arc_ = lv_arc_create(preload_progress_container_);
        lv_obj_set_size(preload_progress_arc_, 80, 80);
        lv_arc_set_rotation(preload_progress_arc_, 270);
        lv_arc_set_bg_angles(preload_progress_arc_, 0, 360);
        lv_arc_set_value(preload_progress_arc_, 0);
        lv_obj_set_style_arc_width(preload_progress_arc_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_set_style_arc_width(preload_progress_arc_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(preload_progress_arc_, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(preload_progress_arc_, LV_OBJ_FLAG_CLICKABLE);

        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_white(), 0);
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(preload_message_label_, "设备正在预热中...");
        
        lv_obj_move_foreground(preload_progress_container_);
    }
    
    // 创建待机时钟页面UI
    // 包含时钟显示、日期显示、星期显示
    void CreateIdleClockUI() {
        // 创建全屏容器作为时钟页面的根容器
        idle_clock_container_ = lv_obj_create(lv_scr_act());
        // 设置容器大小为屏幕分辨率
        lv_obj_set_size(idle_clock_container_, LV_HOR_RES, LV_VER_RES);
        // 将容器居中对齐
        lv_obj_center(idle_clock_container_);
        // 设置容器背景色为黑色
        lv_obj_set_style_bg_color(idle_clock_container_, lv_color_black(), 0);
        // 设置容器背景不透明度为完全不透明
        lv_obj_set_style_bg_opa(idle_clock_container_, LV_OPA_COVER, 0);
        // 设置容器边框宽度为0（无边框）
        lv_obj_set_style_border_width(idle_clock_container_, 0, 0);
        // 设置容器圆角半径为0（无圆角）
        lv_obj_set_style_radius(idle_clock_container_, 0, 0);
        // 设置容器内边距为0
        lv_obj_set_style_pad_all(idle_clock_container_, 0, 0);
        
        // 创建秒钟标签（显示在顶部）
        idle_second_label_ = lv_label_create(idle_clock_container_);
        // 设置秒钟标签字体为time50
        lv_obj_set_style_text_font(idle_second_label_, &time50, 0);
        // 设置秒钟标签文字颜色为白色
        lv_obj_set_style_text_color(idle_second_label_, lv_color_white(), 0);
        // 设置秒钟标签初始文本为"00"
        lv_label_set_text(idle_second_label_, "00");
        // 将秒钟标签对齐到顶部中心位置，向下偏移10像素
        lv_obj_align(idle_second_label_, LV_ALIGN_TOP_MID, 0, 30);
        
        // 创建主时间标签（显示时:分，使用大字体）
        idle_time_label_ = lv_label_create(idle_clock_container_);
        // 设置主时间标签字体为time50
        lv_obj_set_style_text_font(idle_time_label_, &time50, 0);
        // 设置主时间标签文字颜色为黄色
        lv_obj_set_style_text_color(idle_time_label_, lv_color_hex(0xFFFF00), 0);
        // 设置主时间标签初始文本为"00:00"
        lv_label_set_text(idle_time_label_, "00:00");
        // 将主时间标签对齐到屏幕中心，向上偏移10像素
        lv_obj_align(idle_time_label_, LV_ALIGN_CENTER, 0, -10);
        
        // 创建日期标签（显示年-月-日）
        idle_date_label_ = lv_label_create(idle_clock_container_);
        // 设置日期标签字体为普惠字体20号
        lv_obj_set_style_text_font(idle_date_label_, &font_puhui_20_4, 0);
        // 设置日期标签文字颜色为白色
        lv_obj_set_style_text_color(idle_date_label_, lv_color_white(), 0);
        // 设置日期标签初始文本为"2024-01-01"
        lv_label_set_text(idle_date_label_, "2024-01-01");
        // 将日期标签对齐到屏幕中心，向下偏移30像素
        lv_obj_align(idle_date_label_, LV_ALIGN_CENTER, 0, 30);
        
        // 创建星期标签（显示星期几）
        idle_weekday_label_ = lv_label_create(idle_clock_container_);
        // 设置星期标签字体为普惠字体20号
        lv_obj_set_style_text_font(idle_weekday_label_, &font_puhui_20_4, 0);
        // 设置星期标签文字颜色为灰色（0xAAAAAA）
        lv_obj_set_style_text_color(idle_weekday_label_, lv_color_hex(0xAAAAAA), 0);
        // 设置星期标签初始文本为"星期一"
        lv_label_set_text(idle_weekday_label_, "星期一");
        // 将星期标签对齐到屏幕中心，向下偏移55像素
        lv_obj_align(idle_weekday_label_, LV_ALIGN_CENTER, 0, 55);
        
        // 初始状态下隐藏时钟容器
        lv_obj_add_flag(idle_clock_container_, LV_OBJ_FLAG_HIDDEN);
        
        // 记录日志：待机时钟页面UI创建完成
        ESP_LOGI(TAG, "待机时钟页面UI创建完成");
    }
    
    // 更新待机时钟显示（内部方法）
    void UpdateIdleClockInternal() {
        if (idle_time_label_ == nullptr) return;
        
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // 更新时:分
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text(idle_time_label_, time_buf);
        
        // 更新秒
        char sec_buf[8];
        snprintf(sec_buf, sizeof(sec_buf), "%02d", timeinfo.tm_sec);
        lv_label_set_text(idle_second_label_, sec_buf);
        
        // 更新日期
        char date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", 
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        lv_label_set_text(idle_date_label_, date_buf);
        
        // 更新星期
        const char* weekdays[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
        lv_label_set_text(idle_weekday_label_, weekdays[timeinfo.tm_wday]);
    }
};

class XINGZHI_CUBE_1_54TFT_WIFI : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    CustomLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    bool is_charging_ = false;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    TaskHandle_t image_task_handle_ = nullptr;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_38);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            is_charging_ = is_charging;
            if (is_charging_) {
                power_save_timer_->WakeUp();
            }
            power_save_timer_->SetEnabled(true);
        });
    }

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_21);
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_21, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 15, 180);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            display_->ShowIdleClock();  // 显示待机时钟页面
            if (!is_charging_) {
                GetBacklight()->SetBrightness(10);  // 降低亮度但保持可见
            }
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->HideIdleClock();  // 隐藏待机时钟页面
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Entering deep sleep mode with minimal brightness");
            rtc_gpio_set_level(GPIO_NUM_21, 0);
            // 启用保持功能，确保睡眠期间电平不变
            rtc_gpio_hold_en(GPIO_NUM_21);
            // 不关闭显示屏，仅在非充电状态下降至1%
            if (!is_charging_) {
                GetBacklight()->SetBrightness(1);
            }
            // 注意：这里不调用 esp_deep_sleep_start()，保持显示时钟运行
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeSt7789Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        display_ = new CustomLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, 
        {
            .text_font = &font_puhui_20_4,
            .icon_font = &font_awesome_20_4
        });
    }

    // 初始化IoT设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));         // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Screen"));          // 添加屏幕设备
        thing_manager.AddThing(iot::CreateThing("ImageDisplay"));    // 添加图片显示控制设备
        // thing_manager.AddThing(iot::CreateThing("MusicPlayer"));     // 添加音乐播放器控制设备
        // 直接创建MusicPlayer实例（避免静态初始化顺序问题）
        thing_manager.AddThing(new iot::MusicPlayerThing());
#if CONFIG_USE_ALARM
        thing_manager.AddThing(iot::CreateThing("AlarmIot"));
#endif
    }
    // 初始化图片资源管理器
    void InitializeImageResources() {
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 设置预加载进度回调（虽然开机使用静默预加载，但设置回调以备将来使用）
        image_manager.SetPreloadProgressCallback([this](int current, int total, const char* message) {
            if (display_) {
                display_->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        
        esp_err_t result = image_manager.Initialize();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "图片资源管理器初始化失败");
        }
        // 开机阶段同步静默全量加载（不限时，不触发UI遮罩）
        image_manager.PreloadRemainingImagesSilent(0);
    }

    // 检查图片资源
    void CheckImageResources() {
        ESP_LOGI(TAG, "开始检查图片资源...");
        
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 等待WiFi连接
        auto& wifi = WifiStation::GetInstance();
        int wifi_wait_count = 0;
        while (!wifi.IsConnected() && wifi_wait_count < 100) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wifi_wait_count++;
        }
        
        if (!wifi.IsConnected()) {
            ESP_LOGW(TAG, "WiFi未连接，跳过图片资源检查");
            return;
        }
        
        // 设置下载和预加载进度回调
        image_manager.SetDownloadProgressCallback([this](int current, int total, const char* message) {
            if (display_) {
                int percent = (total > 0) ? (current * 100 / total) : 0;
                display_->UpdateDownloadProgressUI(true, percent, message);
            }
        });
        
        image_manager.SetPreloadProgressCallback([this](int current, int total, const char* message) {
            if (display_) {
                display_->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        
        // 取消并等待预加载完成
        ESP_LOGI(TAG, "取消并等待预加载完成...");
        image_manager.CancelPreload();
        image_manager.WaitForPreloadToFinish(1000);
        ESP_LOGI(TAG, "预加载处理完成");

        // 检查并更新所有资源
        esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(CONFIG_IMAGE_API_URL, CONFIG_IMAGE_VERSION_URL);
        
        if (all_resources_result == ESP_OK) {
            ESP_LOGI(TAG, "图片资源检查完成，资源已是最新版本或更新成功");
            
            // 资源正常，设备就绪，播放开机成功提示音
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                ESP_LOGI(TAG, "设备就绪，播放开机成功提示音");
                app.PlaySound(Lang::Sounds::P3_SUCCESS);
            }
        } else {
            ESP_LOGW(TAG, "图片资源更新失败，错误码: %s (%d)", 
                    esp_err_to_name(all_resources_result), all_resources_result);
        }
        
        // 更新静态logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已设置");
        } else {
            ESP_LOGW(TAG, "logo图片不可用");
        }
        
        ESP_LOGI(TAG, "图片资源检查完成");
    }

    // 启动图片循环显示任务
    void StartImageSlideshow() {
        // 启动图片轮播任务
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 8192, this, 1, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
        
        // 设置图片资源检查回调
        auto& app = Application::GetInstance();
        app.SetImageResourceCallback([this]() {
            ESP_LOGI(TAG, "OTA检查完成，开始检查图片资源");
            BaseType_t task_result = xTaskCreate([](void* param) {
                XINGZHI_CUBE_1_54TFT_WIFI* board = static_cast<XINGZHI_CUBE_1_54TFT_WIFI*>(param);
                board->CheckImageResources();
                vTaskDelete(NULL);
            }, "img_resource_check", 8192, this, 3, NULL);
            
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "图片资源检查任务创建失败");
            } else {
                ESP_LOGI(TAG, "图片资源检查任务创建成功");
            }
        });
    }

    // 图片循环显示任务实现
    static void ImageSlideshowTask(void* arg) {
        XINGZHI_CUBE_1_54TFT_WIFI* board = static_cast<XINGZHI_CUBE_1_54TFT_WIFI*>(arg);
        Display* display = board->GetDisplay();
        auto& app = Application::GetInstance();
        auto& image_manager = ImageResourceManager::GetInstance();
        
        ESP_LOGI(TAG, "🎬 图片播放任务启动 - 配置强力音频保护机制");
        
        // **智能分级音频保护配置**
        const bool ENABLE_DYNAMIC_PRIORITY = true;   // 启用动态优先级调节
        
        // **性能优化设置**
        if (ENABLE_DYNAMIC_PRIORITY) {
            // 适度降低图片任务优先级，为音频任务让出资源
            vTaskPrioritySet(NULL, 2); // 从1调整到2
            ESP_LOGI(TAG, "💡 图片任务优先级已调整，音频任务享有更高优先权");
        }
        
        // 启用适度的音频优先模式（不再过度严格）
        app.SetAudioPriorityMode(false); // 关闭严格模式，使用智能保护
        
        ESP_LOGI(TAG, "🎯 智能音频保护已激活，图片播放将根据音频状态智能调节");
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 设置图片显示参数
        int imgWidth = DISPLAY_WIDTH;
        int imgHeight = DISPLAY_HEIGHT;
        
        // 创建图像描述符
        lv_image_dsc_t img_dsc = {
            .header = {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_RGB565,
                .flags = 0,
                .w = (uint32_t)imgWidth,
                .h = (uint32_t)imgHeight,
                .stride = (uint32_t)(imgWidth * 2),
                .reserved_2 = 0,
            },
            .data_size = (uint32_t)(imgWidth * imgHeight * 2),
            .data = NULL,
            .reserved = NULL
        };
        
        // 创建图像容器
        lv_obj_t* img_container = nullptr;
        lv_obj_t* img_obj = nullptr;
        
        {
            DisplayLockGuard lock(display);
            
            lv_obj_t* screen = lv_screen_active();
            img_container = lv_obj_create(screen);
            lv_obj_remove_style_all(img_container);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_center(img_container);
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            lv_obj_move_foreground(img_container);
            
            img_obj = lv_img_create(img_container);
            lv_obj_center(img_obj);
            lv_obj_move_foreground(img_obj);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 优化：预加载状态快速检查
        ESP_LOGI(TAG, "优化检查：快速检查预加载状态...");
        int preload_check_count = 0;
        while (preload_check_count < 50) { // 最多5秒
            bool isPreloadActive = false;
            if (display) {
                CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
                if (customDisplay->download_progress_container_ != nullptr &&
                    !lv_obj_has_flag(customDisplay->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                    isPreloadActive = true;
                }
            }
            
            if (!isPreloadActive) {
                break; // 预加载已完成或未开始
            }
            
            ESP_LOGI(TAG, "快速检查预加载状态... (%d/50)", preload_check_count + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            preload_check_count++;
        }
        
        if (preload_check_count >= 50) {
            ESP_LOGW(TAG, "预加载等待优化：超时后继续启动图片轮播");
        } else {
            ESP_LOGI(TAG, "预加载状态检查完成，快速启动图片轮播");
        }
        
        // 尝试获取logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "已从资源管理器获取logo图片");
        }
        
        // 立即显示静态图片或第一张动画图片
        if (g_image_display_mode == iot::MODE_STATIC && g_static_image) {
            DisplayLockGuard lock(display);
            img_dsc.data = g_static_image;
            lv_img_set_src(img_obj, &img_dsc);
            ESP_LOGI(TAG, "开机显示logo图片");
        } else {
            const auto& imageArray = image_manager.GetImageArray();
            if (!imageArray.empty() && imageArray[0]) {
                DisplayLockGuard lock(display);
                img_dsc.data = imageArray[0];
                lv_img_set_src(img_obj, &img_dsc);
                ESP_LOGI(TAG, "开机显示第一张图片");
            }
        }
        
        // 主循环
        int currentIndex = 0;
        bool directionForward = true;
        TickType_t lastUpdateTime = xTaskGetTickCount();
        const TickType_t cycleInterval = pdMS_TO_TICKS(150);
        
        bool isAudioPlaying = false;
        DeviceState previousState = app.GetDeviceState();
        bool pendingAnimationStart = false;
        TickType_t stateChangeTime = 0;
        
        while (true) {
            const auto& imageArray = image_manager.GetImageArray();
            
            if (imageArray.empty()) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
            
            if (currentIndex >= imageArray.size()) {
                currentIndex = 0;
            }
            
            // 确保图片容器在底层，不遮挡状态栏和消息字幕
            {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_move_to_index(img_container, 0);  // 移到最底层
                    
                    // 确保图片对象在容器内显示
                    if (img_obj) {
                        lv_obj_center(img_obj);
                        lv_obj_move_foreground(img_obj);  // 在容器内前置
                    }
                }
            }
            
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // 检测说话状态变化
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                pendingAnimationStart = true;
                stateChangeTime = currentTime;
                directionForward = true;
                ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
            }
            
            if (currentState != kDeviceStateSpeaking && isAudioPlaying) {
                isAudioPlaying = false;
                ESP_LOGI(TAG, "退出说话状态，停止动画");
                
                // 优化：动画结束时回到第一帧，避免停在中间
                currentIndex = 0;
                directionForward = true;
                
                // 显示第一帧图片
                if (!imageArray.empty() && imageArray[0]) {
                    DisplayLockGuard lock(display);
                    img_dsc.data = imageArray[0];
                    lv_img_set_src(img_obj, &img_dsc);
                    ESP_LOGI(TAG, "动画结束，已重置到第一帧");
                }
            }
            
            // 延迟启动动画
            if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                currentIndex = 1;
                directionForward = true;
                
                if (currentIndex < imageArray.size()) {
                    int actual_image_index = currentIndex + 1;
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        image_manager.LoadImageOnDemand(actual_image_index);
                    }
                    
                    const uint8_t* currentImage = imageArray[currentIndex];
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        img_dsc.data = currentImage;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                    
                    ESP_LOGI(TAG, "开始播放动画");
                    lastUpdateTime = currentTime;
                    isAudioPlaying = true;
                    pendingAnimationStart = false;
                }
            }
            
            // 动画播放逻辑
            bool shouldAnimate = isAudioPlaying && g_image_display_mode == iot::MODE_ANIMATED;
            
            if (shouldAnimate && !pendingAnimationStart && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 优化：往返逻辑边界与abrobot保持一致
                if (directionForward) {
                    currentIndex++;
                    // 提前一帧折返，避免索引越界
                    if (currentIndex >= imageArray.size() - 1) {
                        currentIndex = imageArray.size() - 1;
                        directionForward = false;
                    }
                } else {
                    currentIndex--;
                    // 到达第一帧时切换方向
                    if (currentIndex <= 0) {
                        currentIndex = 0;
                        directionForward = true;
                    }
                }
                
                int actual_image_index = currentIndex + 1;
                if (!image_manager.IsImageLoaded(actual_image_index)) {
                    image_manager.LoadImageOnDemand(actual_image_index);
                }
                
                if (currentIndex >= 0 && currentIndex < imageArray.size()) {
                    const uint8_t* currentImage = imageArray[currentIndex];
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        img_dsc.data = currentImage;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                lastUpdateTime = currentTime;
            }
            
            // 静态模式显示logo
            if (g_image_display_mode == iot::MODE_STATIC && g_static_image) {
                DisplayLockGuard lock(display);
                img_dsc.data = g_static_image;
                lv_img_set_src(img_obj, &img_dsc);
            }
            
            previousState = currentState;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

public:
    XINGZHI_CUBE_1_54TFT_WIFI() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeButtons();
        InitializeSt7789Display();
        InitializeIot();
        InitializeImageResources();
        GetBacklight()->RestoreBrightness();
        StartImageSlideshow();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

// CustomLcdDisplay 公共方法实现
void CustomLcdDisplay::ShowIdleClock() {
    DisplayLockGuard lock(this);
    
    if (idle_clock_container_ == nullptr) {
        CreateIdleClockUI();
    }
    
    // 更新时钟显示
    UpdateIdleClockInternal();
    
    // 显示时钟容器
    lv_obj_clear_flag(idle_clock_container_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(idle_clock_container_);
    
    // 启动定时器，每秒更新一次
    if (idle_clock_timer_ == nullptr) {
        idle_clock_timer_ = lv_timer_create([](lv_timer_t* timer) {
            CustomLcdDisplay* display = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer));
            if (display) {
                DisplayLockGuard lock(display);
                display->UpdateIdleClockInternal();
            }
        }, 1000, this);
    }
    
    ESP_LOGI(TAG, "待机时钟页面已显示");
}

void CustomLcdDisplay::HideIdleClock() {
    DisplayLockGuard lock(this);
    
    if (idle_clock_container_ != nullptr) {
        lv_obj_add_flag(idle_clock_container_, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 停止定时器
    if (idle_clock_timer_ != nullptr) {
        lv_timer_del(idle_clock_timer_);
        idle_clock_timer_ = nullptr;
    }
    
    ESP_LOGI(TAG, "待机时钟页面已隐藏");
}

DECLARE_BOARD(XINGZHI_CUBE_1_54TFT_WIFI);
