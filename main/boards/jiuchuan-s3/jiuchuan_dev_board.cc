#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "settings.h"
#include "i2c_device.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "esp_lcd_panel_gc9301.h"
#include "image_manager.h"
#include "iot_image_display.h"
#include "iot/thing_manager.h"
#include "iot/things/music_player.h"
#include <inttypes.h>

#include "power_save_timer.h"
#include "power_manager.h"
#include "power_controller.h"
#include "gpio_manager.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <cstring>

#define BOARD_TAG "JiuchuanDevBoard"

// 取消power_manager.h中的TAG定义，使用本地TAG
#ifdef TAG
#undef TAG
#endif
#define TAG "JiuchuanDevBoard"

#define __USER_GPIO_PWRDOWN__

// 声明使用的LVGL字体
LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

// 外部声明图片显示模式变量
extern "C" {
    extern volatile iot::ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;
}

// 自定义LCD显示器类，用于圆形屏幕适配
class CustomLcdDisplay : public SpiLcdDisplay
{
public:
    // 允许访问container_和状态栏元素用于层级管理和颜色设置
    using SpiLcdDisplay::container_;
    using SpiLcdDisplay::status_bar_;
    using SpiLcdDisplay::status_label_;
    using SpiLcdDisplay::network_label_;
    using SpiLcdDisplay::battery_label_;
    using SpiLcdDisplay::mute_label_;
    using SpiLcdDisplay::notification_label_;
    
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
            {
                .text_font = &font_puhui_20_4,
                .icon_font = &font_awesome_20_4
            })
    {

        DisplayLockGuard lock(this);
        // 调整状态栏padding以适配圆形屏幕
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.167, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.167, 0);
        
        // 容器背景设置：状态栏白色不透明，消息区域透明以显示图片
        lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
        // 状态栏设置为白色背景，不透明
        lv_obj_set_style_bg_color(status_bar_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_COVER, 0);
        // content_设置为透明，不遮挡图片显示
        lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
        
        // 反转文字颜色逻辑：白色主题用白色字体，黑色主题用黑色字体
        // 获取当前主题并设置相应的文字颜色
        std::string theme = GetTheme();
        lv_color_t text_color = (theme == "dark") ? lv_color_black() : lv_color_white();
        
        // 设置状态栏本身的文字颜色
        lv_obj_set_style_text_color(status_bar_, text_color, 0);
        
        // 设置状态栏所有子元素的文字颜色
        if (status_label_) lv_obj_set_style_text_color(status_label_, text_color, 0);
        if (network_label_) lv_obj_set_style_text_color(network_label_, text_color, 0);
        if (battery_label_) lv_obj_set_style_text_color(battery_label_, text_color, 0);
        if (mute_label_) lv_obj_set_style_text_color(mute_label_, text_color, 0);
        if (notification_label_) lv_obj_set_style_text_color(notification_label_, text_color, 0);
        
        // 设置消息区域（content_）的文字颜色，与状态栏保持一致
        lv_obj_set_style_text_color(content_, text_color, 0);
        
        // 将消息显示在屏幕底部，减少对图片的遮挡
        // content_是垂直布局(LV_FLEX_FLOW_COLUMN)：主轴=垂直，交叉轴=水平
        // 参数1(主轴对齐)=LV_FLEX_ALIGN_END：推到底部
        // 参数2(交叉轴对齐)=LV_FLEX_ALIGN_CENTER：水平居中
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // 添加底部内边距，防止文字贴底或超出圆形屏幕边缘
        // 圆形屏幕底部需要更多空间，避免文字被切割
        lv_obj_set_style_pad_bottom(content_, 35, 0);  // 增加到40px，让字幕容器向上移动
        
        // 将container_（包含状态栏和content_）移到最前面，确保UI在图片之上
        lv_obj_move_foreground(container_);
        
        // 创建字幕圆角矩形容器
        CreateSubtitleContainer();
    }
    
    // 创建字幕容器
    void CreateSubtitleContainer() {
        DisplayLockGuard lock(this);
        
        // 创建圆角矩形容器
        subtitle_container_ = lv_obj_create(content_);
        
        // 计算两行文本的高度（行高 * 2 + 内边距）
        lv_coord_t line_height = font_puhui_20_4.line_height;
        lv_coord_t container_height = line_height * 2 + 24;  // 两行高度 + 上下内边距(12*2)
        
        // 设置容器样式 - 固定高度为两行
        lv_obj_set_size(subtitle_container_, LV_HOR_RES * 0.9, container_height);
        lv_obj_set_style_radius(subtitle_container_, 25, 0);  // 圆角半径25px
        lv_obj_set_style_bg_color(subtitle_container_, lv_color_hex(0x000000), 0);  // 黑色背景
        lv_obj_set_style_bg_opa(subtitle_container_, LV_OPA_50, 0);  // 50%不透明度
        lv_obj_set_style_border_width(subtitle_container_, 2, 0);  // 白色边框宽度2px
        lv_obj_set_style_border_color(subtitle_container_, lv_color_white(), 0);  // 白色边框
        lv_obj_set_style_pad_all(subtitle_container_, 12, 0);  // 内边距12px
        lv_obj_set_scrollbar_mode(subtitle_container_, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
        lv_obj_set_style_clip_corner(subtitle_container_, true, 0);  // 裁剪圆角边缘
        
        // 设置容器为flex布局，用于垂直居中
        lv_obj_set_flex_flow(subtitle_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(subtitle_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // 启用垂直滚动
        lv_obj_set_scroll_dir(subtitle_container_, LV_DIR_VER);
        
        // 创建字幕文本标签
        subtitle_label_ = lv_label_create(subtitle_container_);
        lv_obj_set_width(subtitle_label_, LV_HOR_RES * 0.9 - 24);  // 减去内边距
        lv_obj_set_style_text_font(subtitle_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(subtitle_label_, lv_color_white(), 0);  // 白色文字
        lv_obj_set_style_text_align(subtitle_label_, LV_TEXT_ALIGN_CENTER, 0);  // 居中对齐
        lv_obj_set_style_text_line_space(subtitle_label_, -5, 0);  // 设置行间距为2像素
        lv_label_set_text(subtitle_label_, "");
        
        // 初始隐藏容器
        lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
    }

    // 下载进度UI成员变量
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr;
    lv_obj_t* message_label_ = nullptr;
    lv_obj_t* download_progress_arc_ = nullptr;
    
    // 预加载进度UI成员变量
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_arc_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    lv_obj_t* preload_progress_label_ = nullptr;
    lv_obj_t* preload_percentage_label_ = nullptr;
    
    // 字幕容器和标签
    lv_obj_t* subtitle_container_ = nullptr;
    lv_obj_t* subtitle_label_ = nullptr;
    
    // 用户交互禁用标志
    bool user_interaction_disabled_ = false;

    // 创建下载进度UI
    void CreateDownloadProgressUI() {
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(download_progress_container_);
        
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(download_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(download_progress_container_, 0, 0);

        lv_obj_t* progress_arc = lv_arc_create(download_progress_container_);
        lv_obj_set_size(progress_arc, 120, 120);
        lv_arc_set_rotation(progress_arc, 270);
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        lv_obj_align(progress_arc, LV_ALIGN_CENTER, 0, 0);
        
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
        
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        download_progress_arc_ = progress_arc;

        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(download_progress_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(download_progress_label_, "0%");
        lv_obj_align_to(download_progress_label_, progress_arc, LV_ALIGN_CENTER, 0, 0);

        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(message_label_, lv_pct(80));
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(message_label_, "正在准备下载资源...");
        lv_obj_align_to(message_label_, progress_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        lv_obj_move_foreground(download_progress_container_);
    }

    // 创建预加载进度UI
    void CreatePreloadProgressUI() {
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(preload_progress_container_);
        
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preload_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(preload_progress_container_, 0, 0);
        
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(preload_progress_container_, 20, 0);

        lv_obj_t* progress_arc = lv_arc_create(preload_progress_container_);
        lv_obj_set_size(progress_arc, 80, 80);
        lv_arc_set_rotation(progress_arc, 270);
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x3A3A3C), LV_PART_MAIN);
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
        
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        preload_progress_arc_ = progress_arc;

        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(preload_message_label_, "设备正在预热中...");
        
        preload_progress_label_ = nullptr;
        preload_percentage_label_ = nullptr;
        
        lv_obj_move_foreground(preload_progress_container_);
    }

    // 更新下载进度UI
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        DisplayLockGuard lock(this);
        
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        
        if (download_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            if (download_progress_arc_) {
                lv_arc_set_value(download_progress_arc_, progress);
                
                if (progress < 30) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
                } else if (progress < 70) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FFB3), LV_PART_INDICATOR);
                } else {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FF7F), LV_PART_INDICATOR);
                }
            }
            
            if (download_progress_label_) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            
            if (message && message_label_ != nullptr) {
                if (strstr(message, "下载") != nullptr) {
                    if (progress == 100) {
                        lv_label_set_text(message_label_, "下载完成");
                    } else {
                        lv_label_set_text(message_label_, "正在下载资源");
                    }
                } else if (strstr(message, "删除") != nullptr) {
                    lv_label_set_text(message_label_, "正在清理旧文件");
                } else if (strstr(message, "准备") != nullptr) {
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
            SetIdle(false);
        } else {
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            SetIdle(true);
        }
    }

    // 显示下载进度
    void ShowDownloadProgress(bool show, int progress = 0, const char* message = nullptr) {
        if (!show || !message) {
            UpdateDownloadProgressUI(false, 0, nullptr);
            return;
        }
        UpdateDownloadProgressUI(true, progress, message);
    }

    // 更新预加载进度UI
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        DisplayLockGuard lock(this);
        
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
        }
        
        if (preload_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            int progress = (total > 0) ? (current * 100 / total) : 0;
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            if (preload_progress_arc_) {
                lv_arc_set_value(preload_progress_arc_, progress);
            }
            
            if (preload_message_label_ && message) {
                lv_label_set_text(preload_message_label_, message);
            }
            
            lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(preload_progress_container_);
            SetIdle(false);
        } else {
            lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            SetIdle(true);
        }
    }
    
    // 重写SetChatMessage方法以在圆角矩形容器中显示字幕
    void SetChatMessage(const char* role, const char* content) override {
        DisplayLockGuard lock(this);
        
        if (subtitle_container_ == nullptr || subtitle_label_ == nullptr) {
            return;
        }
        
        // 如果内容为空，隐藏容器
        if (content == nullptr || strlen(content) == 0) {
            lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        
        // 设置文本为换行模式
        lv_label_set_long_mode(subtitle_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(subtitle_label_, content);
        
        // 计算文本实际需要的高度来判断行数
        lv_coord_t label_width = LV_HOR_RES * 0.9 - 24;  // 标签宽度
        lv_coord_t text_width = lv_txt_get_width(content, strlen(content), &font_puhui_20_4, 0);
        
        // 估算行数：文本宽度 / 标签宽度，向上取整
        int estimated_lines = (text_width + label_width - 1) / label_width + 1;
        
        if (estimated_lines <= 2) {
            // 1-2行文本：禁用滚动，文本居中或正常显示
            lv_obj_remove_flag(subtitle_container_, LV_OBJ_FLAG_SCROLLABLE);
            // 恢复居中对齐
            lv_obj_set_flex_align(subtitle_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            // flex布局会自动处理垂直居中
        } else {
            // 超过两行：启用纵向滚动
            lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_SCROLLABLE);
            // 设置为顶部对齐，确保第一句话从容器顶部开始显示
            lv_obj_set_flex_align(subtitle_container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            // 使用LVGL的滚动动画实现自动向上滚动
            // 计算需要滚动的总高度
            lv_coord_t content_height = lv_obj_get_height(subtitle_label_);
            lv_coord_t container_content_height = lv_obj_get_content_height(subtitle_container_);
            lv_coord_t scroll_height = content_height - container_content_height;
            
            if (scroll_height > 0) {
                // 先滚动到顶部（显示消息开头）
                lv_obj_scroll_to_y(subtitle_container_, 0, LV_ANIM_OFF);
                
                // 延迟后开始向下滚动到底部
                lv_anim_t scroll_anim;
                lv_anim_init(&scroll_anim);
                lv_anim_set_var(&scroll_anim, subtitle_container_);
                lv_anim_set_exec_cb(&scroll_anim, [](void* obj, int32_t value) {
                    lv_obj_scroll_to_y((lv_obj_t*)obj, value, LV_ANIM_OFF);
                });
                
                lv_anim_set_values(&scroll_anim, 0, scroll_height);  // 从顶部滚动到底部（向下滚动）
                lv_anim_set_time(&scroll_anim, scroll_height * 50);  // 50ms每像素
                lv_anim_set_delay(&scroll_anim, 1000);  // 延迟1秒开始
                lv_anim_set_playback_time(&scroll_anim, scroll_height * 50);  // 返回时间
                lv_anim_set_playback_delay(&scroll_anim, 1000);  // 返回前延迟1秒
                lv_anim_set_repeat_count(&scroll_anim, LV_ANIM_REPEAT_INFINITE);  // 无限循环
                lv_anim_start(&scroll_anim);
            }
        }
        
        // 显示容器
        lv_obj_clear_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
        
        // 确保容器在前景层
        lv_obj_move_foreground(container_);
    }
};

class JiuchuanDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button pwr_button_;
    Button wifi_button;
    Button cmd_button;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;
    
    // 图片播放任务句柄
    TaskHandle_t image_task_handle_ = nullptr;
    
    // 图片资源API URL配置
    static const char* API_URL;
    static const char* VERSION_URL;

    // 音量映射函数：将内部音量(0-80)映射为显示音量(0-100%)
    int MapVolumeForDisplay(int internal_volume) {
        // 确保输入在有效范围内
        if (internal_volume < 0) internal_volume = 0;
        if (internal_volume > 80) internal_volume = 80;
        
        // 将0-80映射到0-100
        // 公式: 显示音量 = (内部音量 / 80) * 100
        return (internal_volume * 100) / 80;
    }
    
    void InitializePowerManager() {
        power_manager_ = new PowerManager(PWR_ADC_GPIO);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        #ifndef __USER_GPIO_PWRDOWN__
        RTC_DATA_ATTR static bool long_press_occurred = false;
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) {
            ESP_LOGI(TAG, "Wake up by EXT0");
            const int64_t start = esp_timer_get_time();
            ESP_LOGI(TAG, "esp_sleep_get_wakeup_cause");
            while (gpio_get_level(PWR_BUTTON_GPIO) == 0) {
                if (esp_timer_get_time() - start > 3000000) {
                    long_press_occurred = true;
                    break;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            
            if (long_press_occurred) {
                ESP_LOGI(TAG, "Long press wakeup");
                long_press_occurred = false;
            } else {
                ESP_LOGI(TAG, "Short press, return to sleep");
                ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0));
                ESP_ERROR_CHECK(rtc_gpio_pullup_en(PWR_BUTTON_GPIO));  // 内部上拉
                ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(PWR_BUTTON_GPIO));
                esp_deep_sleep_start();
            }
        }
        #endif
        // 参数: cpu_max_freq, seconds_to_clock, seconds_to_dim, seconds_to_shutdown
        // 30秒进入时钟模式，60秒降低亮度到10%，5分钟降低亮度到1%（不关机）
        power_save_timer_ = new PowerSaveTimer(-1, 30, 60, (60*5));
        // power_save_timer_ = new PowerSaveTimer(-1, 5, 10, 20);//test
        
        // 设置进入时钟模式的回调（30秒后触发）
        power_save_timer_->OnEnterClockMode([this]() {
            ESP_LOGI(TAG, "Entering weather clock mode");
            display_->SetChatMessage("system", "");  // 清空聊天消息
            display_->SetClockMode(true);            // 启用天气时钟模式
        });
        
        // 设置退出时钟模式的回调
        power_save_timer_->OnExitClockMode([this]() {
            ESP_LOGI(TAG, "Exiting weather clock mode");
            display_->SetClockMode(false);           // 禁用天气时钟模式
            display_->SetChatMessage("system", "");  // 清空聊天消息
            display_->SetEmotion("neutral");         // 设置中性表情
        });
        
        // 设置进入降低亮度模式的回调（60秒后触发）
        power_save_timer_->OnEnterDimMode([this]() {
            ESP_LOGI(TAG, "Dimming backlight to 10%%");
            GetBacklight()->SetBrightness(10);  // 降低亮度到10%
        });
        
        // 设置退出降低亮度模式的回调
        power_save_timer_->OnExitDimMode([this]() {
            ESP_LOGI(TAG, "Restoring backlight brightness");
            GetBacklight()->RestoreBrightness();  // 恢复正常亮度
        });
        
        power_save_timer_->OnEnterSleepMode([this]() {
            SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            SetPowerSaveMode(false);
        });
        
        // 5分钟后进入极低亮度模式（1%），不关机
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Entering ultra-low brightness mode (1%%)");
            GetBacklight()->SetBrightness(1);  // 降低亮度到1%
        });
        
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

    }

    // 优化：添加音频质量优化方法
    void OptimizeAudioSettings() {
        auto codec = GetAudioCodec();
        if (codec) {
            // 根据环境自适应调整增益，使用整数存储，转换为浮点数
            Settings settings("audio", false);
            int gain_int = settings.GetInt("input_gain", 48);  // 默认48dB
            float custom_gain = static_cast<float>(gain_int);
            codec->SetInputGain(custom_gain);
            ESP_LOGI(TAG, "音频设置已优化：输入增益 %.1fdB", custom_gain);
        }
    }

    // 初始化图片资源管理器
    void InitializeImageResources() {
        auto& image_manager = ImageResourceManager::GetInstance();
        
        esp_err_t result = image_manager.Initialize();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "图片资源管理器初始化失败");
        }
        // 开机阶段同步静默全量加载
        image_manager.PreloadRemainingImagesSilent(0);
    }
    
    // 静态任务包装函数，用于xTaskCreate
    static void CheckImageResourcesTask(void* param) {
        JiuchuanDevBoard* board = static_cast<JiuchuanDevBoard*>(param);
        board->CheckImageResources();
        vTaskDelete(NULL);
    }
    
    // 检查并更新图片资源
    void CheckImageResources() {
        ESP_LOGI(TAG, "开始检查图片资源...");
        ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 等待WiFi连接
        auto& wifi = WifiStation::GetInstance();
        int wifi_wait_count = 0;
        while (!wifi.IsConnected() && wifi_wait_count < 30) {
            ESP_LOGI(TAG, "等待WiFi连接以检查图片资源... (%d/30)", wifi_wait_count + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
            wifi_wait_count++;
        }
        
        if (!wifi.IsConnected()) {
            ESP_LOGE(TAG, "WiFi连接超时，图片资源检查任务退出");
            return;
        }
        
        ESP_LOGI(TAG, "WiFi已连接，立即开始资源检查...");
        
        // 取消并等待预加载完成
        ESP_LOGI(TAG, "取消并等待预加载完成...");
        image_manager.CancelPreload();
        image_manager.WaitForPreloadToFinish(1000);
        ESP_LOGI(TAG, "预加载处理完成");
        
        // 检查并更新所有资源
        ESP_LOGI(TAG, "开始调用CheckAndUpdateAllResources");
        ESP_LOGI(TAG, "API_URL: %s", API_URL);
        ESP_LOGI(TAG, "VERSION_URL: %s", VERSION_URL);
        
        esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(API_URL, VERSION_URL);
        
        ESP_LOGI(TAG, "CheckAndUpdateAllResources调用完成，结果: %s (%d)", 
                esp_err_to_name(all_resources_result), all_resources_result);
        
        bool has_updates = false;
        bool has_errors = false;
        
        if (all_resources_result == ESP_OK) {
            ESP_LOGI(TAG, "图片资源更新完成");
            has_updates = true;
        } else if (all_resources_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需更新");
            
            // 资源无需更新，设备就绪，播放开机成功提示音
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                ESP_LOGI(TAG, "设备就绪，播放开机成功提示音");
                app.PlaySound(Lang::Sounds::P3_SUCCESS);
            }
        } else {
            ESP_LOGE(TAG, "图片资源检查/下载失败");
            has_errors = true;
        }
        
        // 仅当有实际下载更新且无严重错误时才重启
        if (has_updates && !has_errors) {
            ESP_LOGI(TAG, "图片资源有更新，2秒后重启设备...");
            for (int i = 2; i > 0; i--) {
                ESP_LOGI(TAG, "将在 %d 秒后重启...", i);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            esp_restart();
        } else if (has_errors) {
            ESP_LOGW(TAG, "图片资源下载存在错误，设备继续运行但可能缺少部分图片");
        } else {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需重启");
        }
    }
    
    // 启动图片资源检查
    void StartImageResourceCheck() {
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 设置下载进度回调函数
        image_manager.SetDownloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                int percent = (total > 0) ? (current * 100 / total) : 0;
                customDisplay->ShowDownloadProgress(message != nullptr, percent, message);
            }
        });
        
        // 设置预加载进度回调函数
        image_manager.SetPreloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                customDisplay->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        
        // 设置图片资源检查回调
        auto& app = Application::GetInstance();
        app.SetImageResourceCallback([this]() {
            ESP_LOGI(TAG, "OTA检查完成，开始检查图片资源");
            BaseType_t task_result = xTaskCreate(
                CheckImageResourcesTask,  // 使用静态函数
                "img_resource_check", 
                8192, 
                this,  // 传递this指针作为参数
                3, 
                NULL
            );
            
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "图片资源检查任务创建失败，错误码: %d", task_result);
                ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
            } else {
                ESP_LOGI(TAG, "图片资源检查任务创建成功");
            }
        });
    }
    
    // 图片循环显示任务实现
    static void ImageSlideshowTask(void* arg) {
        JiuchuanDevBoard* board = static_cast<JiuchuanDevBoard*>(arg);
        Display* display = board->GetDisplay();
        auto& app = Application::GetInstance();
        auto& image_manager = ImageResourceManager::GetInstance();
        
        ESP_LOGI(TAG, "图片播放任务启动");
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 设置图片显示参数
        int imgWidth = 240;
        int imgHeight = 240;
        
        // 创建图像描述符 - 兼容服务端RGB565格式
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
        
        // 创建一个图像容器
        lv_obj_t* img_container = nullptr;
        lv_obj_t* img_obj = nullptr;
        
        {
            DisplayLockGuard lock(display);
            
            // 创建图像容器（放在背景层，不使用move_foreground）
            img_container = lv_obj_create(lv_scr_act());
            lv_obj_remove_style_all(img_container);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_center(img_container);
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            // 不调用move_foreground，让content_在图片之上
            
            // 创建图像对象
            img_obj = lv_img_create(img_container);
            
            // 设置图片缩放为110% (256 * 1.1 = 282)
            lv_img_set_zoom(img_obj, 282);
            
            // 居中对齐，并向下偏移2.5个像素
            lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 3);
            
            // 确保UI容器（包含文字消息）在图片容器之上
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
            if (customDisplay && customDisplay->container_) {
                lv_obj_move_foreground(customDisplay->container_);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 尝试从资源管理器获取logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "已从资源管理器获取logo图片");
        }
        
        // 立即尝试显示静态图片或第一张图片
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
        
        // 当前索引和方向控制
        int currentIndex = 0;
        bool directionForward = true;
        const uint8_t* currentImage = nullptr;
        
        // 状态跟踪变量
        bool lastWasStaticMode = false;
        const uint8_t* lastStaticImage = nullptr;
        
        // 主循环
        TickType_t lastUpdateTime = xTaskGetTickCount();
        const TickType_t cycleInterval = pdMS_TO_TICKS(150);
        
        bool isAudioPlaying = false;
        bool wasAudioPlaying = false;
        DeviceState previousState = app.GetDeviceState();
        bool pendingAnimationStart = false;
        TickType_t stateChangeTime = 0;
        
        while (true) {
            const auto& imageArray = image_manager.GetImageArray();
            
            // 如果没有图片资源，等待
            if (imageArray.empty()) {
                static int wait_count = 0;
                wait_count++;
                
                if (wait_count <= 30) {
                    ESP_LOGW(TAG, "图片资源未加载，等待中... (%d/30)", wait_count);
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    continue;
                } else {
                    ESP_LOGE(TAG, "图片资源等待超时");
                    DisplayLockGuard lock(display);
                    if (img_container) {
                        lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    }
                    vTaskDelay(pdMS_TO_TICKS(5000));
                    wait_count = 0;
                    continue;
                }
            }
            
            // 确保currentIndex在有效范围内
            if (currentIndex >= imageArray.size()) {
                currentIndex = 0;
            }
            
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // 检查下载UI是否可见
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
            bool isDownloadUIVisible = false;
            if (customDisplay && customDisplay->download_progress_container_ &&
                !lv_obj_has_flag(customDisplay->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isDownloadUIVisible = true;
            }
            
            // 检查预加载UI是否可见
            bool isPreloadUIVisible = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadUIVisible = true;
            }
            
            // 下载或预加载UI显示时隐藏图片容器
            if (isDownloadUIVisible || isPreloadUIVisible) {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    // 不调用move_foreground，保持图片在背景层
                }
            }
            
            // 检测到状态变为Speaking
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                pendingAnimationStart = true;
                stateChangeTime = currentTime;
                directionForward = true;
                ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
            }
            
            // 如果状态不是Speaking，确保isAudioPlaying为false
            if (currentState != kDeviceStateSpeaking && isAudioPlaying) {
                isAudioPlaying = false;
                ESP_LOGI(TAG, "退出说话状态，停止动画");
            }
            
            // 延迟启动动画
            if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                currentIndex = 1;
                directionForward = true;
                
                if (currentIndex < imageArray.size()) {
                    int actual_image_index = currentIndex + 1;
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "图片 %d 未预加载，正在加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "图片 %d 加载失败", actual_image_index);
                            currentIndex = 0;
                        }
                    }
                    
                    currentImage = imageArray[currentIndex];
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
            
            // 根据显示模式确定是否应该动画
            bool shouldAnimate = isAudioPlaying && g_image_display_mode == iot::MODE_ANIMATED;
            
            // 动画播放逻辑
            if (shouldAnimate && !pendingAnimationStart && (currentTime - lastUpdateTime >= cycleInterval)) {
                // 根据方向更新索引
                if (directionForward) {
                    currentIndex++;
                    if (currentIndex >= imageArray.size() - 1) {
                        directionForward = false;
                    }
                } else {
                    currentIndex--;
                    if (currentIndex <= 0) {
                        directionForward = true;
                        currentIndex = 0;
                    }
                }
                
                // 确保索引在有效范围内
                if (currentIndex >= 0 && currentIndex < imageArray.size()) {
                    int actual_image_index = currentIndex + 1;
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "图片 %d 未预加载，正在加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "图片 %d 加载失败，跳过此帧", actual_image_index);
                            lastUpdateTime = currentTime;
                            continue;
                        }
                    }
                    
                    currentImage = imageArray[currentIndex];
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        img_dsc.data = currentImage;
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                }
                
                lastUpdateTime = currentTime;
            }
            // 处理静态图片显示
            else if ((!isAudioPlaying && wasAudioPlaying) ||
                     (g_image_display_mode == iot::MODE_STATIC && currentIndex != 0) ||
                     (!isAudioPlaying && currentIndex != 0)) {
                
                const uint8_t* staticImage = nullptr;
                bool isStaticMode = false;
                
                if (g_image_display_mode == iot::MODE_STATIC && iot::g_static_image) {
                    staticImage = iot::g_static_image;
                    isStaticMode = true;
                } else if (!imageArray.empty()) {
                    currentIndex = 0;
                    staticImage = imageArray[currentIndex];
                    isStaticMode = false;
                }
                
                if (staticImage) {
                    DisplayLockGuard lock(display);
                    img_dsc.data = staticImage;
                    lv_img_set_src(img_obj, &img_dsc);
                    
                    // 只在状态变化时输出日志
                    if (isStaticMode != lastWasStaticMode || staticImage != lastStaticImage) {
                        ESP_LOGI(TAG, "显示%s图片", isStaticMode ? "logo" : "初始");
                        lastWasStaticMode = isStaticMode;
                        lastStaticImage = staticImage;
                    }
                    
                    pendingAnimationStart = false;
                }
            }
            
            // 更新状态记录
            wasAudioPlaying = isAudioPlaying;
            previousState = currentState;
            
            // 短暂延时
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        vTaskDelete(NULL);
    }
    
    // 启动图片循环显示任务
    void StartImageSlideshow() {
        ESP_LOGI(TAG, "启动图片循环显示任务");
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 8192, this, 1, &image_task_handle_);
    }
    
    // 初始化IoT设备
    void InitializeIot() {
        ESP_LOGI(TAG, "初始化IoT设备");
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));         // 添加扬声器设备（音量控制）
        thing_manager.AddThing(iot::CreateThing("Screen"));          // 添加屏幕设备（亮度和主题）
        thing_manager.AddThing(iot::CreateThing("ImageDisplay"));    // 添加图片显示控制设备（动静态切换）
        thing_manager.AddThing(new iot::MusicPlayerThing());         // 添加音乐播放器控制设备
#if CONFIG_USE_ALARM
        thing_manager.AddThing(iot::CreateThing("AlarmIot"));        // 添加闹钟设备
#endif
        ESP_LOGI(TAG, "IoT设备初始化完成");
    }
    
    // 暂停图片任务以节省CPU
    void SuspendImageTask() {
        if (image_task_handle_ != nullptr) {
            vTaskSuspend(image_task_handle_);
            ESP_LOGI(TAG, "图片轮播任务已暂停");
        }
    }
    
    // 恢复图片任务
    void ResumeImageTask() {
        if (image_task_handle_ != nullptr) {
            vTaskResume(image_task_handle_);
            ESP_LOGI(TAG, "图片轮播任务已恢复");
        }
    }

    void InitializeButtons() {
        static bool pwrbutton_unreleased = false;

        if (gpio_get_level(GPIO_NUM_3) == 1) {
            pwrbutton_unreleased = true;
        }
        // 配置GPIO
        ESP_LOGI(TAG, "Configuring power button GPIO");
        GpioManager::Config(GPIO_NUM_3, GpioManager::GpioMode::INPUT_PULLDOWN);

        boot_button_.OnClick([this]() {
            ESP_LOGI(TAG, "Boot button clicked");
            power_save_timer_->WakeUp();
        });

        // 检查电源按钮初始状态
        ESP_LOGI(TAG, "Power button initial state: %d", GpioManager::GetLevel(PWR_BUTTON_GPIO));

        // 高电平有效长按关机逻辑
        pwr_button_.OnPressDown([this]() {
            pwrbutton_unreleased = false;
        });
        pwr_button_.OnLongPress([this]()
                                {
        ESP_LOGI(TAG, "Power button long press detected (high-active)");

            if (pwrbutton_unreleased){
                ESP_LOGI(TAG, "开机后电源键未松开,取消关机");
                return;
            }
            
            // 高电平有效防抖确认
            for (int i = 0; i < 5; i++) {
                int level = GpioManager::GetLevel(PWR_BUTTON_GPIO);
                ESP_LOGD(TAG, "Debounce check %d: GPIO%d level=%d", i+1, PWR_BUTTON_GPIO, level);
                
                if (level == 0) {
                    ESP_LOGW(TAG, "Power button inactive during confirmation - abort shutdown");
                    return;
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            
            ESP_LOGI(TAG, "Confirmed power button pressed - initiating shutdown");
            power_manager_->SetPowerState(PowerState::SHUTDOWN); });

        //单击切换状态
        pwr_button_.OnClick([this]()
                            {
            // 获取当前应用实例和状态
            auto &app = Application::GetInstance();
            auto current_state = app.GetDeviceState();

            ESP_LOGI(TAG, "当前设备状态: %d", current_state);
            
            if (current_state == kDeviceStateIdle) {
                // 如果当前是待命状态，切换到聆听状态
                ESP_LOGI(TAG, "从待命状态切换到聆听状态");
                app.ToggleChatState(); // 切换到聆听状态
            } else if (current_state == kDeviceStateListening) {
                // 如果当前是聆听状态，切换到待命状态
                ESP_LOGI(TAG, "从聆听状态切换到待命状态");
                app.ToggleChatState(); // 切换到待命状态
            } else if (current_state == kDeviceStateSpeaking) {
                // 如果当前是说话状态，终止说话并切换到待命状态
                ESP_LOGI(TAG, "从说话状态切换到待命状态");
                app.ToggleChatState(); // 终止说话
            } else {
                // 其他状态下只唤醒设备
                ESP_LOGI(TAG, "唤醒设备");
                power_save_timer_->WakeUp();
            } });

        // 电源键双击：重置WiFi
        pwr_button_.OnDoubleClick([this]()
                                    {
            ESP_LOGI(TAG, "Power button double click: 重置WiFi");
            power_save_timer_->WakeUp();
            ResetWifiConfiguration(); });

        // 电源键三击：进入配网模式
        pwr_button_.OnTripleClick([this]()
                                    {
            ESP_LOGI(TAG, "Power button triple click: 进入配网模式");
            power_save_timer_->WakeUp();
            ResetWifiConfiguration(); });

        wifi_button.OnPressDown([this]()
                            {
           ESP_LOGI(TAG, "Volume up button pressed");
            power_save_timer_->WakeUp();

            auto codec = GetAudioCodec();
            int current_vol = codec->output_volume(); // 获取实际当前音量
            current_vol = (current_vol + 8 > 80) ? 80 : current_vol + 8;
            
            codec->SetOutputVolume(current_vol);

            ESP_LOGI(TAG, "Current volume: %d", current_vol);
            int display_volume = MapVolumeForDisplay(current_vol);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(display_volume) + "%");});

        cmd_button.OnPressDown([this]()
                           {
           ESP_LOGI(TAG, "Volume down button pressed");
            power_save_timer_->WakeUp();

            auto codec = GetAudioCodec();
            int current_vol = codec->output_volume(); // 获取实际当前音量
            current_vol = (current_vol - 8 < 0) ? 0 : current_vol - 8;
            
            codec->SetOutputVolume(current_vol);

            ESP_LOGI(TAG, "Current volume: %d", current_vol);
            if (current_vol == 0) {
                GetDisplay()->ShowNotification(Lang::Strings::MUTED);
            } else {
                int display_volume = MapVolumeForDisplay(current_vol);
                GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(display_volume) + "%");
            }});
    }

        void InitializeGC9301isplay()
        {
            // 液晶屏控制IO初始化
            ESP_LOGI(TAG, "test Install panel IO");
            spi_bus_config_t buscfg = {};
            buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
            buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
            buscfg.miso_io_num = GPIO_NUM_NC;
            buscfg.quadwp_io_num = GPIO_NUM_NC;
            buscfg.quadhd_io_num = GPIO_NUM_NC;
            buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
            ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

            // 初始化SPI总线
            esp_lcd_panel_io_spi_config_t io_config = {};
            io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
            io_config.dc_gpio_num = DISPLAY_DC_PIN;
            io_config.spi_mode = 3;
            io_config.pclk_hz = 80 * 1000 * 1000;
            io_config.trans_queue_depth = 10;
            io_config.lcd_cmd_bits = 8;
            io_config.lcd_param_bits = 8;
            esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io);

            // 初始化液晶屏驱动芯片9309
            ESP_LOGI(TAG, "Install LCD driver");
            esp_lcd_panel_dev_config_t panel_config = {};
            panel_config.reset_gpio_num = GPIO_NUM_NC;
            panel_config.rgb_ele_order = LCD_RGB_ENDIAN_BGR;
            panel_config.bits_per_pixel = 16;
            esp_lcd_new_panel_gc9309na(panel_io, &panel_config, &panel);

            esp_lcd_panel_reset(panel);

            esp_lcd_panel_init(panel);
            esp_lcd_panel_invert_color(panel, false);
            esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
            esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            display_ = new CustomLcdDisplay(panel_io, panel,
                                            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    JiuchuanDevBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        pwr_button_(PWR_BUTTON_GPIO,true),
        wifi_button(WIFI_BUTTON_GPIO),
        cmd_button(CMD_BUTTON_GPIO) {

        InitializeI2c();
        OptimizeAudioSettings();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeButtons();
        InitializeGC9301isplay();
        GetBacklight()->RestoreBrightness();
        
        // 初始化IoT设备
        InitializeIot();
        // 初始化图片资源管理器
        InitializeImageResources();
        // 启动图片资源检查
        StartImageResourceCheck();
        // 启动图片循环显示
        StartImageSlideshow();

    }
    
    ~JiuchuanDevBoard() {
        // 停止图片播放任务
        if (image_task_handle_ != nullptr) {
            vTaskDelete(image_task_handle_);
            image_task_handle_ = nullptr;
        }
    }

    Led* GetLed() {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {

        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR);
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

// 定义静态成员变量API URL
#ifdef CONFIG_IMAGE_API_URL
const char* JiuchuanDevBoard::API_URL = CONFIG_IMAGE_API_URL;
#else
const char* JiuchuanDevBoard::API_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif

#ifdef CONFIG_IMAGE_VERSION_URL
const char* JiuchuanDevBoard::VERSION_URL = CONFIG_IMAGE_VERSION_URL;
#else
const char* JiuchuanDevBoard::VERSION_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif

DECLARE_BOARD(JiuchuanDevBoard);
