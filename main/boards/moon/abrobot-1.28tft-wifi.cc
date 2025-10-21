#include "core/lv_obj_pos.h"  // 包含LVGL对象位置相关头文件
#include "font/lv_font.h"     // 包含LVGL字体相关头文件
#include "misc/lv_types.h"    // 包含LVGL类型定义头文件
#include "wifi_board.h"       // 包含WiFi板卡基类头文件
#include "audio_codecs/es8311_audio_codec.h"  // 包含ES8311音频编解码器头文件
#include "display/lcd_display.h"  // 包含LCD显示控制头文件
#include "system_reset.h"      // 包含系统重置功能头文件
#include "application.h"       // 包含应用程序管理头文件
#include "button.h"            // 包含按钮控制头文件
#include "config.h"            // 包含配置管理头文件
#include "iot/thing_manager.h"  // 包含IoT设备管理头文件
#include "lunar_calendar.h"     // 包含农历日期转换头文件
#include "sdkconfig.h"         // 包含SDK配置头文件
#include "power_save_timer.h"  // 包含电源节省定时器头文件

#include <esp_log.h>           // ESP日志系统
#include "i2c_device.h"        // I2C设备控制
#include <driver/i2c_master.h>  // ESP32 I2C主机驱动
#include <driver/ledc.h>        // ESP32 LED控制器驱动
#include <wifi_station.h>       // WiFi站点模式管理
#include <esp_lcd_panel_io.h>   // ESP32 LCD面板IO接口
#include <esp_lcd_panel_ops.h>  // ESP32 LCD面板操作接口
#include <esp_timer.h>          // ESP32定时器
#include "lcd_display.h"        // LCD显示控制
#include <iot_button.h>         // IoT按钮管理
#include <cstring>              // C字符串处理
#include "esp_lcd_gc9a01.h"     // GC9A01 LCD驱动
#include <font_awesome_symbols.h>  // Font Awesome图标符号
#include "assets/lang_config.h"    // 语言配置
#include <esp_http_client.h>      // ESP32 HTTP客户端
#include <cJSON.h>                // JSON解析库
#include "power_manager.h"        // 电源管理
#include "power_save_timer.h"     // 省电定时器
#include <esp_sleep.h>            // ESP32睡眠模式
#include <esp_pm.h>               // ESP32电源管理
#include <esp_wifi.h>             // ESP32 WiFi功能
#include "button.h"               // 按钮控制
#include "settings.h"             // 设置管理
#include "iot_image_display.h"  // 引入图片显示模式定义
#include "image_manager.h"  // 引入图片资源管理器头文件
#include "ui/music_player_ui.h"  // 引入音乐播放器UI组件
#include "ui/mqtt_music_handler.h"  // 引入MQTT音乐控制器
#include "iot/things/music_player.h"  // 引入音乐播放器IoT设备
#define TAG "abrobot-1.28tft-wifi"  // 日志标签

// 在abrobot-1.28tft-wifi.cc文件开头添加外部声明
extern "C" {
    // 图片显示模式
    extern volatile iot::ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;  // 静态图片引用
}

// 声明使用的LVGL字体
LV_FONT_DECLARE(lunar);      // 农历字体
LV_FONT_DECLARE(time70);     // 70像素大小时间字体
LV_FONT_DECLARE(time50);     // 50像素大小时间字体
LV_FONT_DECLARE(time40);     // 40像素大小时间字体
LV_FONT_DECLARE(font_puhui_20_4);    // 普惠20像素字体
LV_FONT_DECLARE(font_awesome_20_4);  // Font Awesome 20像素图标字体
LV_FONT_DECLARE(font_awesome_30_4);  // Font Awesome 30像素图标字体
 


// 暗色主题颜色定义
#define DARK_BACKGROUND_COLOR       lv_color_hex(0)           // 深色背景色（黑色）
#define DARK_TEXT_COLOR             lv_color_black()          // 黑色文本颜色
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0)           // 聊天背景色（黑色）
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)    // 用户气泡颜色（深绿色）
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)    // 助手气泡颜色（深灰色）
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)    // 系统气泡颜色（中灰色）
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)    // 系统文本颜色（浅灰色）
#define DARK_BORDER_COLOR           lv_color_hex(0)           // 边框颜色（黑色）
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)    // 低电量提示颜色（红色）

// 亮色主题颜色定义
#define LIGHT_BACKGROUND_COLOR       lv_color_hex(0)          // 深色背景色（黑色）
#define LIGHT_TEXT_COLOR             lv_color_white()          // 白色文本颜色
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)    // 聊天背景色（浅灰色）
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)    // 用户气泡颜色（微信绿）
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()          // 助手气泡颜色（白色）
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)    // 系统气泡颜色（浅灰色）
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)    // 系统文本颜色（深灰色）
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)    // 边框颜色（浅灰色）
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()          // 低电量提示颜色（黑色）

// 主题颜色结构体定义
struct ThemeColors {
    lv_color_t background;        // 背景色
    lv_color_t text;              // 文本颜色
    lv_color_t chat_background;   // 聊天背景色
    lv_color_t user_bubble;       // 用户气泡颜色
    lv_color_t assistant_bubble;  // 助手气泡颜色
    lv_color_t system_bubble;     // 系统气泡颜色
    lv_color_t system_text;       // 系统文本颜色
    lv_color_t border;            // 边框颜色
    lv_color_t low_battery;       // 低电量提示颜色
};

// 定义暗色主题颜色
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR
};

// 定义亮色主题颜色
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR
};
 
// 当前主题 - 基于默认配置初始化为暗色主题
static ThemeColors current_theme = DARK_THEME;

// 在文件开头添加全局变量，用于安全地传递下载进度状态
static struct {
    bool pending;
    int progress;
    char message[64];
    SemaphoreHandle_t mutex;
} g_download_progress = {false, 0, "", NULL};

// 自定义LCD显示类，继承自SpiLcdDisplay
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    
    lv_timer_t *idle_timer_ = nullptr;  // 空闲定时器指针，用于自动切换到时钟界面
    
    // 睡眠管理相关
    lv_timer_t* sleep_timer_ = nullptr;  // 睡眠定时器
    bool is_sleeping_ = false;           // 睡眠状态标志
    int normal_brightness_ = 70;         // 正常亮度值
    
    // 浅睡眠状态管理
    bool is_light_sleeping_ = false;     // 浅睡眠状态标志
    
    // 音乐播放器UI相关成员
    MusicPlayerUI* music_player_ui_ = nullptr;  // 音乐播放器UI实例
    bool music_player_active_ = false;          // 音乐播放器激活状态
 
    lv_obj_t * tab1 = nullptr;          // 第一个标签页（主界面）
    lv_obj_t * tab2 = nullptr;          // 第二个标签页（时钟界面）
    lv_obj_t * tab3 = nullptr;          // 第三个标签页（超级省电模式界面）
    lv_obj_t * tabview_ = nullptr;      // 标签视图组件
    
    // tab3超级省电模式的UI元素
    lv_obj_t * tab3_time_label_ = nullptr;      // 超级省电模式的时间标签
    lv_obj_t * tab3_date_label_ = nullptr;      // 超级省电模式的日期标签
    lv_obj_t * tab3_weekday_label_ = nullptr;   // 超级省电模式的星期标签
    lv_obj_t * tab3_mode_label_ = nullptr;      // 超级省电模式的模式提示标签
    lv_obj_t * bg_img = nullptr;        // 背景图像对象
    lv_obj_t * bg_img2 = nullptr;       // 第二背景图像对象
    uint8_t bg_index = 1;               // 当前背景索引（1-4）
    lv_obj_t * bg_switch_btn = nullptr; // 切换背景的按钮
    lv_obj_t * subtitle_container_ = nullptr;  // 字幕容器，固定在底部三分之一区域
    
    // 字幕循环滚动相关
    lv_timer_t* subtitle_scroll_timer_ = nullptr;  // 字幕滚动定时器
    lv_coord_t subtitle_scroll_pos_ = 0;           // 当前滚动位置
    lv_coord_t subtitle_max_scroll_ = 0;           // 最大滚动距离
    bool subtitle_scrolling_ = false;              // 是否正在滚动
 
    // 构造函数：初始化LCD显示
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4,       // 设置文本字体
                        .icon_font = &font_awesome_20_4,     // 设置图标字体
                        // 不再使用表情符号字体
                    }) {
        DisplayLockGuard lock(this);  // 获取显示锁，防止多线程访问冲突
        SetupUI();                    // 设置用户界面
        
        // 初始化音乐播放器UI
        music_player_config_t config = getDefaultMusicPlayerConfig();
        music_player_ui_ = music_player_ui_create(&config);
        if (!music_player_ui_) {
            ESP_LOGE(TAG, "创建音乐播放器UI失败");
        } else {
            // 初始化音乐播放器UI，使用当前活动屏幕作为父对象
            lv_obj_t* screen = lv_screen_active();
            music_player_error_t ret = music_player_ui_->Initialize(screen, &config);
            if (ret != MUSIC_PLAYER_OK) {
                ESP_LOGE(TAG, "初始化音乐播放器UI失败: %d", ret);
                music_player_ui_destroy(music_player_ui_);
                music_player_ui_ = nullptr;
            } else {
                // 设置全局实例指针，供IoT接口使用
                extern MusicPlayerUI* g_music_player_instance;
                g_music_player_instance = music_player_ui_;
                ESP_LOGI(TAG, "音乐播放器UI初始化成功");
            }
        }
        
        // 创建一个用于保护下载进度状态的互斥锁
        if (g_download_progress.mutex == NULL) {
            g_download_progress.mutex = xSemaphoreCreateMutex();
        }
        
        // 创建定时器定期检查并更新下载进度显示
        lv_timer_create([](lv_timer_t* timer) {
            CustomLcdDisplay* display = (CustomLcdDisplay*)lv_timer_get_user_data(timer);
            if (!display) return;
            
            // 检查是否有待更新的进度
            if (g_download_progress.mutex && xSemaphoreTake(g_download_progress.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (g_download_progress.pending) {
                    int progress = g_download_progress.progress;
                    char message[64];
                    strncpy(message, g_download_progress.message, sizeof(message));
                    
                    // 重置标志
                    g_download_progress.pending = false;
                    xSemaphoreGive(g_download_progress.mutex);
                    
                    // 更新UI
                    display->UpdateDownloadProgressUI(true, progress, message);
                } else {
                    xSemaphoreGive(g_download_progress.mutex);
                }
            }
        }, 100, this); // 100ms检查一次
    }
    
    // 字幕循环滚动定时器回调函数（静态成员函数）
    static void SubtitleScrollTimerCallback(lv_timer_t* timer) {
        auto* display = static_cast<CustomLcdDisplay*>(timer->user_data);
        if (!display || !display->subtitle_container_ || !display->chat_message_label_) {
            return;
        }
        
        // 获取当前滚动位置
        lv_coord_t current_scroll = lv_obj_get_scroll_y(display->subtitle_container_);
        
        // 计算下一个滚动位置（每次滚动1像素）
        lv_coord_t next_scroll = current_scroll + 1;
        
        // 如果到达底部，延迟2秒后重新从顶部开始
        if (next_scroll >= display->subtitle_max_scroll_) {
            // 已到达底部，暂停2秒
            if (display->subtitle_scroll_pos_ == 0) {
                display->subtitle_scroll_pos_ = 1;  // 标记已到达底部
                lv_timer_set_period(timer, 2000);   // 延迟2秒
                return;
            } else {
                // 延迟结束，重新从顶部开始
                lv_obj_scroll_to_y(display->subtitle_container_, 0, LV_ANIM_OFF);
                display->subtitle_scroll_pos_ = 0;
                lv_timer_set_period(timer, 30);  // 恢复正常滚动速度（30ms = 约33fps）
                return;
            }
        }
        
        // 正常滚动
        lv_obj_scroll_to_y(display->subtitle_container_, next_scroll, LV_ANIM_OFF);
    }
    
    // 析构函数 - 清理定时器
    ~CustomLcdDisplay() {
        // 清理音乐播放器UI
        if (music_player_ui_) {
            extern MusicPlayerUI* g_music_player_instance;
            g_music_player_instance = nullptr;  // 先清理全局指针
            music_player_ui_destroy(music_player_ui_);
            music_player_ui_ = nullptr;
        }
        
        // 清理字幕滚动定时器
        if (subtitle_scroll_timer_) {
            lv_timer_del(subtitle_scroll_timer_);
            subtitle_scroll_timer_ = nullptr;
        }
        
        // 清理字幕容器
        if (subtitle_container_) {
            lv_obj_del(subtitle_container_);
            subtitle_container_ = nullptr;
        }
        
        if (idle_timer_) {
            lv_timer_del(idle_timer_);
            idle_timer_ = nullptr;
        }
        if (sleep_timer_) {
            lv_timer_del(sleep_timer_);
            sleep_timer_ = nullptr;
        }
    }

    /**
     * @brief 显示音乐播放器界面
     * @param album_cover_path 专辑封面路径（可选）
     * @param title 歌曲标题
     * @param artist 艺术家名称
     * @param duration_ms 显示持续时间（毫秒）
     */
    void ShowMusicPlayer(const char* album_cover_path = nullptr, 
                        const char* title = "未知歌曲", 
                        const char* artist = "未知艺术家",
                        uint32_t duration_ms = 30000) {
        if (!music_player_ui_) {
            ESP_LOGE(TAG, "音乐播放器UI未初始化");
            return;
        }
        
        // 设置歌曲信息
        if (album_cover_path) {
            // TODO: 需要将文件路径转换为图像数据
            // music_player_ui_->SetAlbumCover(album_cover_path);
            ESP_LOGI(TAG, "专辑封面路径: %s (暂未实现文件加载)", album_cover_path);
        }
        music_player_ui_->SetSongInfo(title, artist);
        
        // 显示音乐播放器，传递持续时间参数
        music_player_ui_->Show(duration_ms);
        music_player_active_ = true;
        
        ESP_LOGI(TAG, "音乐播放器UI已显示: %s - %s (持续时间: %lu ms)", title, artist, duration_ms);
    }
    
    /**
     * @brief 隐藏音乐播放器界面
     */
    void HideMusicPlayer() {
        if (music_player_ui_ && music_player_active_) {
            music_player_ui_->Hide();
            music_player_active_ = false;
            ESP_LOGI(TAG, "音乐播放器UI已隐藏");
        }
    }
    
    /**
     * @brief 更新音乐频谱显示
     * @param spectrum_data 频谱数据数组
     * @param spectrum_size 频谱数据大小
     */
    void UpdateMusicSpectrum(const float* spectrum_data, size_t spectrum_size) {
        // 频谱功能已删除
        // if (music_player_ui_ && music_player_active_) {
        //     music_player_ui_->UpdateSpectrum(spectrum_data, spectrum_size);
        // }
    }
    
    /**
     * @brief 检查音乐播放器是否处于激活状态
     * @return true 如果音乐播放器处于激活状态，否则返回false
     */
    bool IsMusicPlayerActive() const {
        return music_player_active_;
    }

    // 设置空闲状态方法，控制是否启用空闲定时器
    void SetIdle(bool status) override 
    {
                // 如果status为false，表示有用户交互，需要停止定时器
        if (status == false)
        {
            // 停止空闲定时器
            if (idle_timer_ != nullptr) {
                lv_timer_del(idle_timer_);  // 删除现有定时器
                idle_timer_ = nullptr;
            }
            
            // 停止睡眠定时器
            if (sleep_timer_ != nullptr) {
                lv_timer_del(sleep_timer_);
                sleep_timer_ = nullptr;
            }
            
            // 如果设备在睡眠状态，恢复亮度
            if (is_sleeping_) {
                static auto& board = Board::GetInstance();
                auto backlight = board.GetBacklight();
                if (backlight) {
                    backlight->SetBrightness(normal_brightness_);
                }
                is_sleeping_ = false;
                ESP_LOGI(TAG, "用户交互唤醒设备，恢复亮度到 %d", normal_brightness_);
            }
            
            // 如果当前在时钟页面，切换回主页面
            if (tabview_ != nullptr) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {  // 当前在时钟页面(tab2, 索引1)
                    ESP_LOGI(TAG, "用户交互唤醒，从时钟页面切换回主页面");
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);  // 切换到tab1（索引0）
                }
            }
            return;
        } 

            // 如果用户交互被禁用（例如在预加载期间），不启用空闲定时器
    if (user_interaction_disabled_) {
        ESP_LOGI(TAG, "用户交互已禁用，暂不启用空闲定时器");
        return;
    }

    // 如果已存在定时器，先删除它
    if (idle_timer_ != nullptr) {
        lv_timer_del(idle_timer_);
        idle_timer_ = nullptr;
    }
    
    // 获取当前设备状态，判断是否应该启用空闲定时器
    auto& app = Application::GetInstance();
    DeviceState currentState = app.GetDeviceState();
    
    // 检查下载UI是否实际可见
    bool download_ui_is_active_and_visible = false;
    if (download_progress_container_ != nullptr &&
        !lv_obj_has_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
        download_ui_is_active_and_visible = true;
    }
    
    // 检查预加载UI是否实际可见
    bool preload_ui_is_active_and_visible = false;
    if (preload_progress_container_ != nullptr &&
        !lv_obj_has_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
        preload_ui_is_active_and_visible = true;
    }
    
    ESP_LOGI(TAG, "SetIdle(true) 状态检查: 设备状态=%d, 下载UI可见=%s, 预加载UI可见=%s", 
            currentState, download_ui_is_active_and_visible ? "是" : "否", 
            preload_ui_is_active_and_visible ? "是" : "否");
    
    if (currentState == kDeviceStateStarting || 
        currentState == kDeviceStateWifiConfiguring ||
        currentState == kDeviceStateActivating ||
        currentState == kDeviceStateUpgrading ||
        download_ui_is_active_and_visible ||
        preload_ui_is_active_and_visible) { 
        ESP_LOGI(TAG, "设备处于启动/配置/激活/升级状态或下载/预加载UI可见，暂不启用空闲定时器");
        return;
    }
        
        // 创建一个定时器，15秒后切换到时钟页面（tab2）
        ESP_LOGI(TAG, "创建空闲定时器，15秒后切换到时钟页面");
        idle_timer_ = lv_timer_create([](lv_timer_t * t) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_timer_get_user_data(t);
            if (!display) return;
            
            // 再次检查当前状态，确保在切换前设备不在特殊状态
            auto& app = Application::GetInstance();
            DeviceState currentState = app.GetDeviceState();
            
            // 检查下载UI是否实际可见
            bool download_ui_active = false;
            if (display->download_progress_container_ != nullptr &&
                !lv_obj_has_flag(display->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                download_ui_active = true;
            }
            
            // 检查预加载UI是否实际可见
            bool preload_ui_active = false;
            if (display->preload_progress_container_ != nullptr &&
                !lv_obj_has_flag(display->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                preload_ui_active = true;
            }
            
            // 如果设备已进入某些特殊状态，取消切换
            if (currentState == kDeviceStateStarting || 
                currentState == kDeviceStateWifiConfiguring ||
                download_ui_active ||
                preload_ui_active ||
                display->user_interaction_disabled_) {
                // 删除定时器但不切换
                ESP_LOGW(TAG, "空闲定时器触发时检测到阻塞条件，取消切换: 状态=%d, 下载UI=%s, 预加载UI=%s, 交互禁用=%s", 
                        currentState, download_ui_active ? "可见" : "隐藏", 
                        preload_ui_active ? "可见" : "隐藏", 
                        display->user_interaction_disabled_ ? "是" : "否");
                lv_timer_del(t);
                display->idle_timer_ = nullptr;
                return;
            }
            
            // 使用成员变量tabview_直接切换到tab2
            if (display->tabview_) {
                ESP_LOGI(TAG, "空闲定时器触发，切换到时钟页面");
                // 在切换标签页前加锁，防止异常
                lv_lock();
                lv_tabview_set_act(display->tabview_, 1, LV_ANIM_OFF);  // 切换到tab2（索引1）
                
                // 确保时钟页面始终在最顶层
                lv_obj_move_foreground(display->tab2);
                
                // 如果有画布，将其移到background以确保不会遮挡时钟
                if (display->GetCanvas() != nullptr) {
                    lv_obj_move_background(display->GetCanvas());
                }
                
                lv_unlock();  // 解锁LVGL
                ESP_LOGI(TAG, "成功切换到时钟页面");
            }
            
            // 完成后删除定时器
            lv_timer_del(t);
            display->idle_timer_ = nullptr;
            
            // 注意：睡眠模式由PowerSaveTimer统一管理
            // PowerSaveTimer会在60秒后调用EnterLightSleepMode
            // 在180秒后调用EnterDeepSleepMode切换到tab3
            ESP_LOGI(TAG, "等待PowerSaveTimer管理后续睡眠流程");
        }, 15000, this);  // 15000ms = 15秒
    }
    
    // 进入睡眠模式
    void EnterSleepMode() {
        if (is_sleeping_) return;  // 已经在睡眠状态
        
        ESP_LOGI(TAG, "进入睡眠模式 - 降低屏幕亮度到1");
        
        // 获取当前亮度作为正常亮度（通过Board实例）
        auto& board = Board::GetInstance();
        auto backlight = board.GetBacklight();
        if (backlight) {
            normal_brightness_ = backlight->brightness();
            backlight->SetBrightness(1);  // 设置亮度为1
        }
        
        is_sleeping_ = true;
        
        // 停止睡眠定时器
        if (sleep_timer_) {
            lv_timer_del(sleep_timer_);
            sleep_timer_ = nullptr;
        }
    }
    
    // 退出睡眠模式（唤醒）- 已废弃，由PowerSaveTimer管理
    void ExitSleepMode() {
        if (!is_sleeping_) return;
        
        ESP_LOGI(TAG, "退出睡眠模式 - 恢复屏幕亮度到 %d", normal_brightness_);
        
        auto& board = Board::GetInstance();
        auto backlight = board.GetBacklight();
        if (backlight) {
            backlight->SetBrightness(normal_brightness_);
        }
        
        is_sleeping_ = false;
        // 注意：不再调用StartSleepTimer，由PowerSaveTimer统一管理
    }
    
    // 启动睡眠定时器 - 已废弃，由PowerSaveTimer统一管理
    void StartSleepTimer() {
        // 注意：此方法已废弃，睡眠模式现在由PowerSaveTimer统一管理
        // PowerSaveTimer会在60秒后调用EnterLightSleepMode
        // 在180秒后调用EnterDeepSleepMode切换到tab3
        ESP_LOGD(TAG, "StartSleepTimer已废弃，由PowerSaveTimer管理");
    }
    
    // 停止睡眠定时器 - 已废弃，由PowerSaveTimer统一管理
    void StopSleepTimer() {
        // 注意：此方法已废弃，睡眠定时器由PowerSaveTimer管理
        ESP_LOGD(TAG, "StopSleepTimer已废弃，由PowerSaveTimer管理");
    }
    
    // 浅睡眠状态管理方法
    void SetLightSleeping(bool sleeping) {
        is_light_sleeping_ = sleeping;
        ESP_LOGD(TAG, "浅睡眠状态更新: %s", sleeping ? "进入" : "退出");
    }
    
    bool IsLightSleeping() const {
        return is_light_sleeping_;
    }

    // 设置聊天消息内容
    void SetChatMessage(const char* role, const char* content) override{
        DisplayLockGuard lock(this);  // 获取显示锁
        if (chat_message_label_ == nullptr) {
            return;  // 如果聊天消息标签不存在，直接返回
        }
        lv_label_set_text(chat_message_label_, content);  // 设置消息文本
        
        // 根据内容决定是否显示字幕容器
        if (subtitle_container_ != nullptr) {
            // 检查字幕是否启用
            if (!subtitle_enabled_) {
                // 字幕已禁用，隐藏容器
                lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            
            // 检查内容是否为空或只包含空白字符
            bool has_content = false;
            if (content != nullptr && strlen(content) > 0) {
                // 检查是否有非空白字符
                std::string content_str(content);
                if (content_str.find_first_not_of(" \t\n\r") != std::string::npos) {
                    has_content = true;
                }
            }
            
            if (has_content) {
                // 有实际内容且字幕已启用，显示容器
                lv_obj_clear_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                
                // 停止之前的滚动定时器
                if (subtitle_scroll_timer_ != nullptr) {
                    lv_timer_del(subtitle_scroll_timer_);
                    subtitle_scroll_timer_ = nullptr;
                    subtitle_scrolling_ = false;
                }
                
                // 强制更新LVGL布局，确保标签高度计算正确
                lv_obj_update_layout(chat_message_label_);
                lv_obj_update_layout(subtitle_container_);
                
                // 获取标签的实际高度和容器的高度
                lv_coord_t label_height = lv_obj_get_height(chat_message_label_);
                lv_coord_t container_height = lv_obj_get_content_height(subtitle_container_);
                
                // 如果标签高度大于容器高度，启动循环滚动
                if (label_height > container_height) {
                    // 计算最大滚动距离
                    subtitle_max_scroll_ = label_height - container_height;
                    subtitle_scroll_pos_ = 0;
                    subtitle_scrolling_ = true;
                    
                    // 先滚动到顶部
                    lv_obj_scroll_to_y(subtitle_container_, 0, LV_ANIM_OFF);
                    
                    // 创建滚动定时器，30ms触发一次（约33fps）
                    subtitle_scroll_timer_ = lv_timer_create(SubtitleScrollTimerCallback, 30, this);
                    
                    ESP_LOGI(TAG, "启动字幕循环滚动: 标签高度=%ld, 容器高度=%ld, 最大滚动=%ld", 
                             (long)label_height, (long)container_height, (long)subtitle_max_scroll_);
                } else {
                    // 内容未超出，直接显示在顶部，不需要滚动
                    lv_obj_scroll_to_y(subtitle_container_, 0, LV_ANIM_OFF);
                    subtitle_scrolling_ = false;
                }
            } else {
                // 内容为空或只有空白字符，隐藏容器并停止滚动
                lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                
                if (subtitle_scroll_timer_ != nullptr) {
                    lv_timer_del(subtitle_scroll_timer_);
                    subtitle_scroll_timer_ = nullptr;
                    subtitle_scrolling_ = false;
                }
            }
        }
        
        // 如果当前处于WiFi配置模式，显示到时钟页面也显示提示
        if (std::string(content).find(Lang::Strings::CONNECT_TO_HOTSPOT) != std::string::npos) {
            // 在时钟页面添加配网提示（已在外层获取锁，无需重复获取）
            lv_obj_t* wifi_hint = lv_label_create(tab3);
            lv_obj_set_size(wifi_hint, LV_HOR_RES * 0.8, LV_SIZE_CONTENT);
            lv_obj_align(wifi_hint, LV_ALIGN_CENTER, 0, -20);
            lv_obj_set_style_text_font(wifi_hint, fonts_.text_font, 0);
            lv_obj_set_style_text_color(wifi_hint, lv_color_hex(0xFF9500), 0); // 使用明亮的橙色
            lv_obj_set_style_text_align(wifi_hint, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(wifi_hint, "请连接热点进行WiFi配置\n设备尚未连接网络");
            lv_obj_set_style_bg_color(wifi_hint, lv_color_hex(0x222222), 0);
            lv_obj_set_style_bg_opa(wifi_hint, LV_OPA_70, 0);
            lv_obj_set_style_radius(wifi_hint, 10, 0);
            lv_obj_set_style_pad_all(wifi_hint, 10, 0);
        }
    }

    // 重写字幕启用/禁用方法，同时控制容器的显示
    void SetSubtitleEnabled(bool enabled) override {
        // 调用基类方法设置状态
        Display::SetSubtitleEnabled(enabled);
        
        DisplayLockGuard lock(this);
        if (subtitle_container_ == nullptr) {
            return;
        }
        
        if (enabled) {
            // 启用字幕 - 容器的显示由SetChatMessage根据内容决定
            // 这里不做任何操作，让SetChatMessage来控制
        } else {
            // 禁用字幕 - 隐藏容器并停止滚动
            lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 停止滚动定时器
            if (subtitle_scroll_timer_ != nullptr) {
                lv_timer_del(subtitle_scroll_timer_);
                subtitle_scroll_timer_ = nullptr;
                subtitle_scrolling_ = false;
            }
        }
        
        ESP_LOGI(TAG, "字幕容器状态已设置为: %s", enabled ? "启用" : "禁用");
    }

    // 设置第一个标签页（主界面）
    void SetupTab1() {
        DisplayLockGuard lock(this);
         
        // 设置tab1的基本样式
        lv_obj_set_style_text_font(tab1, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab1, current_theme.text, 0);
        lv_obj_set_style_bg_color(tab1, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab1, LV_OPA_0, 0);  // 完全透明背景

        /* 创建容器 */
        container_ = lv_obj_create(tab1);
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_0, 0);  // 完全透明背景
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(container_, -7, -7);
        lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        
        // 确保容器在前台，这样图片会显示在其后面
        lv_obj_move_foreground(container_);

        /* 状态栏 */
        status_bar_ = lv_obj_create(container_);
        // 圆形屏幕优化：调整状态栏宽度，留出更多边距
        lv_obj_set_size(status_bar_, LV_HOR_RES - 40, fonts_.text_font->line_height);  // 宽度减少40像素
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // 顶部居中对齐
        lv_obj_set_style_radius(status_bar_, 0, 0);
        lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_0, 0);  // 透明背景
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        
        /* 内容区域 - 使用半透明背景 */
        content_ = lv_obj_create(container_);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(content_, 0, 0);
        lv_obj_set_width(content_, LV_HOR_RES);
        lv_obj_set_style_pad_all(content_, 5, 0);
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_0, 0);  // 完全透明
        lv_obj_set_style_border_width(content_, 0, 0);   // 无边框

        // 新增：限制内容区域高度、启用纵向滚动并隐藏滚动条，防止消息过多挤出状态栏
        lv_obj_set_height(content_, LV_VER_RES - fonts_.text_font->line_height - 10); // 高度为屏幕减去状态栏
        lv_obj_set_scroll_dir(content_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);

        // 创建底部字幕容器 - 固定在屏幕下方，适配圆形屏幕
        subtitle_container_ = lv_obj_create(tab1);
        lv_obj_set_size(subtitle_container_, LV_HOR_RES * 0.85, LV_VER_RES * 0.35);  // 宽度85%，高度35%
        lv_obj_align(subtitle_container_, LV_ALIGN_BOTTOM_MID, 0, 5);  // 底部居中对齐，向下偏移10像素
        lv_obj_set_style_bg_color(subtitle_container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(subtitle_container_, LV_OPA_30, 0);  // 30%透明度，不完全遮挡背景
        lv_obj_set_style_border_width(subtitle_container_, 2, 0);  // 2像素边框
        lv_obj_set_style_border_color(subtitle_container_, lv_color_white(), 0);  // 白色边框
        lv_obj_set_style_pad_all(subtitle_container_, 10, 0);  // 内边距10像素
        lv_obj_set_style_radius(subtitle_container_, 10, 0);  // 10像素圆角
        
        // 启用垂直滚动
        lv_obj_set_scroll_dir(subtitle_container_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(subtitle_container_, LV_SCROLLBAR_MODE_OFF);  // 隐藏滚动条
        
        // 确保字幕容器在最上层，不被其他元素遮挡
        lv_obj_move_foreground(subtitle_container_);
        
        // 在字幕容器内创建消息标签
        chat_message_label_ = lv_label_create(subtitle_container_);
        lv_label_set_text(chat_message_label_, "");
        lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.85 - 20);  // 容器宽度减去左右padding（204-20=184px）
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);  // 自动换行
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_0, 0);  // 完全透明背景
        lv_obj_align(chat_message_label_, LV_ALIGN_TOP_MID, 0, 0);  // 在容器内顶部对齐
        
        // 默认隐藏字幕容器，直到有消息需要显示
        lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);

        /* 配置状态栏 */
        lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);  // 设置水平布局
        lv_obj_set_style_pad_all(status_bar_, 0, 0);  // 设置内边距为0
        lv_obj_set_style_border_width(status_bar_, 0, 0);  // 设置边框宽度为0
        lv_obj_set_style_pad_column(status_bar_, 2, 0);  // 设置列间距为8像素，确保图标之间有清晰分隔
        // 圆形屏幕优化：增加左右内边距，确保WiFi图标在安全区域内
        lv_obj_set_style_pad_left(status_bar_, 65, 0);  // WiFi图标向右移动5像素（从40改为45）
        lv_obj_set_style_pad_right(status_bar_, 10, 0);  // 从2增加到15像素，确保右侧元素不被裁剪

        // WiFi信号强度标签 - 显示在状态栏最左边
        network_label_ = lv_label_create(status_bar_);
        lv_label_set_text(network_label_, "");
        lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
        lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
        lv_obj_set_style_pad_right(network_label_, 1, 0);  // 添加右边距，与其他元素保持间距
        // 通知标签
        notification_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(notification_label_, 1);  // 设置弹性增长系数为1
        lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);  // 设置文本居中对齐
        lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);  // 设置文本颜色
        lv_label_set_text(notification_label_, "");  // 初始化为空文本
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  // 隐藏通知标签

        // 状态标签
        status_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(status_label_, 1);  // 设置弹性增长系数为1
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);  // 设置循环滚动模式
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_LEFT, 0);  // 设置文本左对齐
        lv_obj_set_style_text_color(status_label_, current_theme.text, 0);  // 设置文本颜色
        lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);  // 设置初始化文本
        
        // 静音标签
        mute_label_ = lv_label_create(status_bar_);
        lv_label_set_text(mute_label_, "");  // 初始化为空文本
        lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);  // 设置图标字体
        lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);  // 设置文本颜色

        // 电池标签已移至时钟页面（Tab2），适配圆形屏幕

        // 低电量弹窗
        low_battery_popup_ = lv_obj_create(tab1);
        lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);  // 关闭滚动条
        lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);  // 设置弹窗大小
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);  // 底部居中对齐
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  // 设置背景颜色
        lv_obj_set_style_radius(low_battery_popup_, 10, 0);  // 设置圆角半径为10
        
        // 低电量提示文本
        lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
        lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);  // 设置电池需要充电提示
        lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);  // 设置文本为白色
        lv_obj_center(low_battery_label);  // 居中对齐
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏弹窗
    }

    // 设置第二个标签页（时钟界面）
    void SetupTab2() {
        lv_obj_set_style_text_font(tab3, fonts_.text_font, 0);  // 设置标签页文本字体
        lv_obj_set_style_text_color(tab3, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_set_style_bg_color(tab3, lv_color_black(), 0);  // 设置背景颜色为黑色
        lv_obj_set_style_bg_opa(tab3, LV_OPA_COVER, 0);  // 设置背景不透明度为100%

        // 创建秒钟标签，使用time40字体
        lv_obj_t *second_label = lv_label_create(tab3);
        lv_obj_set_style_text_font(second_label, &time40, 0);  // 设置40像素时间字体
        lv_obj_set_style_text_color(second_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_align(second_label, LV_ALIGN_TOP_MID, 0, 10);  // 顶部居中对齐，偏移10像素
        lv_label_set_text(second_label, "00");  // 初始显示"00"秒
        
        // 创建日期标签
        lv_obj_t *date_label = lv_label_create(tab3);
        lv_obj_set_style_text_font(date_label, fonts_.text_font, 0);  // 设置文本字体
        lv_obj_set_style_text_color(date_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(date_label, "01-01");  // 初始显示"01-01"日期
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, -60, 35);  // 顶部居中对齐，向左偏移60像素，向下偏移35像素
        
        // 创建星期标签
        lv_obj_t *weekday_label = lv_label_create(tab3);
        lv_obj_set_style_text_font(weekday_label, fonts_.text_font, 0);  // 设置文本字体
        lv_obj_set_style_text_color(weekday_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(weekday_label, "星期一");  // 初始显示"星期一"
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 60, 35);  // 顶部居中对齐，向右偏移60像素，向下偏移35像素
       
        // 创建一个容器用于放置时间标签
        lv_obj_t *time_container = lv_obj_create(tab3);
        // 设置容器的样式
        lv_obj_remove_style_all(time_container);  // 移除所有默认样式
        lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 大小根据内容自适应
        lv_obj_set_style_pad_all(time_container, 0, 0);  // 无内边距
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, 0);  // 透明背景
        lv_obj_set_style_border_width(time_container, 0, 0);  // 无边框

        // 设置为水平Flex布局
        lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);  // 水平布局
        lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 居中对齐
        
        // 设置容器位置为屏幕中央
        lv_obj_align(time_container, LV_ALIGN_CENTER, 0, 0);
        
        // 创建小时标签
        lv_obj_t *hour_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(hour_label, &time70, 0);  // 设置70像素时间字体
        lv_obj_set_style_text_color(hour_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_label_set_text(hour_label, "00 :");  // 初始显示"00 :"小时
        
        // 创建分钟标签，使用橙色显示
        lv_obj_t *minute_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(minute_label, &time70, 0);  // 设置70像素时间字体
        lv_obj_set_style_text_color(minute_label, lv_color_hex(0xFFA500), 0);  // 设置文本颜色为橙色
        lv_label_set_text(minute_label, " 00");  // 初始显示" 00"分钟
        
        // 创建农历标签
        lv_obj_t *lunar_label = lv_label_create(tab3);
        lv_obj_set_style_text_font(lunar_label, &lunar, 0);  // 设置农历字体
        lv_obj_set_style_text_color(lunar_label, lv_color_white(), 0);  // 设置文本颜色为白色
        lv_obj_set_width(lunar_label, LV_HOR_RES * 0.8);  // 设置宽度为屏幕宽度的80%
        lv_label_set_long_mode(lunar_label, LV_LABEL_LONG_WRAP);  // 设置自动换行模式
        lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);  // 设置文本居中对齐
        lv_label_set_text(lunar_label, "农历癸卯年正月初一");  // 初始显示农历文本
        lv_obj_align(lunar_label, LV_ALIGN_BOTTOM_MID, 0, -36);  // 底部居中对齐，向上偏移36像素
        
        // 创建电池状态容器 - 适配圆形屏幕，放在农历标签下方
        lv_obj_t* battery_container = lv_obj_create(tab3);
        lv_obj_set_size(battery_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  // 自适应内容大小
        lv_obj_set_style_bg_opa(battery_container, LV_OPA_TRANSP, 0);  // 透明背景
        lv_obj_set_style_border_opa(battery_container, LV_OPA_TRANSP, 0);  // 透明边框
        lv_obj_set_style_pad_all(battery_container, 0, 0);  // 无内边距
        lv_obj_set_flex_flow(battery_container, LV_FLEX_FLOW_ROW);  // 水平布局
        lv_obj_set_flex_align(battery_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 居中对齐
        lv_obj_set_style_pad_column(battery_container, 4, 0);  // 图标和文字间距4像素
        lv_obj_align_to(battery_container, lunar_label, LV_ALIGN_OUT_BOTTOM_MID, -30, 8);  // 农历标签下方，向左偏移10像素，向下偏移8像素
        
        // 创建电池图标标签
        battery_label_ = lv_label_create(battery_container);
        lv_label_set_text(battery_label_, "");  // 初始化为空文本
        lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);  // 设置图标字体
        lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);  // 设置文本颜色为白色
        
        // 创建电池百分比标签
        battery_percentage_label_ = lv_label_create(battery_container);
        lv_label_set_text(battery_percentage_label_, "");  // 初始化为空文本
        lv_obj_set_style_text_font(battery_percentage_label_, &font_puhui_20_4, 0);  // 设置普通字体
        lv_obj_set_style_text_color(battery_percentage_label_, lv_color_white(), 0);  // 设置文本颜色为白色
        
        // 定时器更新时间 - 存储静态引用以在回调中使用
        static lv_obj_t* hour_lbl = hour_label;
        static lv_obj_t* minute_lbl = minute_label;
        static lv_obj_t* second_lbl = second_label;
        static lv_obj_t* date_lbl = date_label;
        //static lv_obj_t* year_lbl = year_label;
        static lv_obj_t* weekday_lbl = weekday_label;
        static lv_obj_t* lunar_lbl = lunar_label;
        
        // 创建定时器每秒更新时间 - 使用lambda捕获this指针
        // 性能优化：降低时钟更新频率到2秒一次，减少CPU占用
        lv_timer_create([](lv_timer_t *t) {
            // 获取CustomLcdDisplay实例
            CustomLcdDisplay* display_instance = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
            if (!display_instance) return;
            
            // 超级省电模式下降低tab3更新频率（1分钟更新一次）
            static int tab3_update_counter = 0;
            bool should_update_tab3 = false;
            
            // 检查当前是否在tab3（超级省电模式页面）
            if (display_instance->tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(display_instance->tabview_);
                if (active_tab == 2) {  // tab3的索引是2
                    // 在tab3时，每30次更新一次（30 * 2秒 = 60秒 = 1分钟）
                    tab3_update_counter++;
                    if (tab3_update_counter >= 30) {
                        should_update_tab3 = true;
                        tab3_update_counter = 0;
                    }
                    // 如果不到更新时间，只更新tab3就返回，不更新tab2
                    if (!should_update_tab3) {
                        return;
                    }
                } else {
                    // 不在tab3时重置计数器
                    tab3_update_counter = 0;
                }
            }
            
            // 浅睡眠模式下仍然需要更新时钟显示，但可以适当减少更新频率
            // 移除浅睡眠状态检查，确保时钟在浅睡眠模式下继续运行
            
            // 检查标签是否有效（不包括battery_lbl，因为它现在是成员变量）
            if (!hour_lbl || !minute_lbl || !second_lbl || 
                !date_lbl || !weekday_lbl || !lunar_lbl) return;
            
            // 获取当前时间和电池状态（不需要锁）
            static auto& board = Board::GetInstance();  // 静态引用，避免重复获取
            auto display = board.GetDisplay();
            if (!display) return;
            
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            
            // 获取地理位置信息并进行时区转换
            static GeoLocationInfo location_cache; // 缓存地理位置信息
            static bool location_initialized = false;
            static bool wifi_was_connected = false;
            
            // 检查WiFi连接状态
            bool wifi_connected = WifiStation::GetInstance().IsConnected();
            
            // 只有在WiFi连接成功后才尝试获取地理位置
            if (wifi_connected && !location_initialized) {
                // WiFi刚连接成功，首次尝试获取地理位置
                if (!wifi_was_connected) {
                    ESP_LOGI("ClockTimer", "WiFi connected, attempting to get geolocation for timezone");
                    location_cache = SystemInfo::GetCountryInfo();
                    
                    if (location_cache.is_valid) {
                        location_initialized = true;
                        ESP_LOGI("ClockTimer", "Clock timezone initialized for country %s (UTC%+d)", 
                                 location_cache.country_code.c_str(), location_cache.timezone_offset);
                    } else {
                        ESP_LOGD("ClockTimer", "Geolocation not available yet, using Beijing time");
                    }
                }
            }
            
            // 更新WiFi连接状态记录
            wifi_was_connected = wifi_connected;
            
            // 如果获取到有效的地理位置信息，进行时区转换
            if (location_cache.is_valid && location_cache.timezone_offset != 8) {
                // 当前时间是北京时间，需要转换为本地时区
                timeinfo = SystemInfo::ConvertFromBeijingTime(timeinfo, location_cache.timezone_offset);
                ESP_LOGD("ClockTimer", "Time converted from Beijing to local timezone UTC%+d", 
                         location_cache.timezone_offset);
            }
            
            // 获取电池状态（不需要锁，用于调试日志）
            int battery_level;
            bool charging, discharging;
            const char* icon = nullptr;
            
            if (board.GetBatteryLevel(battery_level, charging, discharging)) {
                ESP_LOGD("ClockTimer", "电池状态 - 电量: %d%%, 充电: %s, 放电: %s", 
                        battery_level, charging ? "是" : "否", discharging ? "是" : "否");
                
                if (charging) {
                    icon = FONT_AWESOME_BATTERY_CHARGING;
                } else {
                    const char* levels[] = {
                        FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                        FONT_AWESOME_BATTERY_1,     // 20-39%
                        FONT_AWESOME_BATTERY_2,     // 40-59%
                        FONT_AWESOME_BATTERY_3,     // 60-79%
                        FONT_AWESOME_BATTERY_FULL,  // 80-99%
                        FONT_AWESOME_BATTERY_FULL,  // 100%
                    };
                    icon = levels[battery_level / 20];
                }
            }
            
            // 尝试获取显示锁并更新UI - 缩短超时时间，减少阻塞
            {
                DisplayLockGuard lock_guard(display);  // 使用DisplayLockGuard，内部有500ms超时
                
                // 检查锁是否成功获取，失败时跳过更新
                if (!lock_guard.IsLocked()) {
                    ESP_LOGD("ClockTimer", "无法获取显示锁，跳过本次时钟更新");
                    return;
                }
                
                // 格式化时、分、秒
                char hour_str[6];
                char minute_str[3];
                char second_str[3];
                
                sprintf(hour_str, "%02d : ", timeinfo.tm_hour);  // 格式化小时，保持两位数
                sprintf(minute_str, "%02d", timeinfo.tm_min);    // 格式化分钟，保持两位数
                sprintf(second_str, "%02d", timeinfo.tm_sec);    // 格式化秒钟，保持两位数
                
                // 更新时间标签
                lv_label_set_text(hour_lbl, hour_str);    // 更新小时
                lv_label_set_text(minute_lbl, minute_str); // 更新分钟
                lv_label_set_text(second_lbl, second_str); // 更新秒钟
                
                // 格式化年份
                char year_str[12];
                snprintf(year_str, sizeof(year_str), "%d", timeinfo.tm_year + 1900);  // 年份从1900年开始计算
                
                // 更新年份标签（当前被注释）
                //lv_label_set_text(year_lbl, year_str);
                
                // 格式化日期为 MM/DD
                char date_str[25];
                snprintf(date_str, sizeof(date_str), "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);  // 月份从0开始计算
                
                // 获取中文星期
                const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
                
                // 更新日期和星期标签
                lv_label_set_text(date_lbl, date_str);  // 更新日期
                
                // 检查星期是否在有效范围内
                if (timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                    lv_label_set_text(weekday_lbl, weekdays[timeinfo.tm_wday]);  // 更新星期
                }
                
                // 计算并更新农历日期
                std::string lunar_date = LunarCalendar::GetLunarDate(
                    timeinfo.tm_year + 1900,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_mday
                );
                lv_label_set_text(lunar_lbl, lunar_date.c_str());  // 更新农历日期
                
                // 更新电池图标UI - 使用成员变量并添加空指针检查
                if (icon && display_instance->battery_label_) {
                    lv_label_set_text(display_instance->battery_label_, icon);  // 更新电池图标
                    ESP_LOGD("ClockTimer", "电池图标已更新: %s", icon);  // 改为DEBUG级别
                }
                
                // 更新电池百分比UI - 添加百分比显示
                if (display_instance->battery_percentage_label_) {
                    char battery_text[8];
                    snprintf(battery_text, sizeof(battery_text), "%d%%", battery_level);
                    lv_label_set_text(display_instance->battery_percentage_label_, battery_text);  // 更新电池百分比
                    ESP_LOGD("ClockTimer", "电池百分比已更新: %s", battery_text);  // 改为DEBUG级别
                }
                
                // 更新tab3超级省电模式的时间显示
                if (display_instance->tab3_time_label_) {
                    char time_str[8];
                    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    lv_label_set_text(display_instance->tab3_time_label_, time_str);
                }
                
                // 更新tab3的日期显示
                if (display_instance->tab3_date_label_) {
                    char date_str[32];
                    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", 
                             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
                    lv_label_set_text(display_instance->tab3_date_label_, date_str);
                }
                
                // 更新tab3的星期显示
                if (display_instance->tab3_weekday_label_ && timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                    lv_label_set_text(display_instance->tab3_weekday_label_, weekdays[timeinfo.tm_wday]);
                }
            }  // DisplayLockGuard 会自动释放锁
            
        }, 2000, this);  // 性能优化：每2000毫秒更新一次，减少CPU占用

        // 电池状态显示已删除 - 不再显示电量UI
    }

    // 设置用户界面，初始化所有UI组件
    virtual void SetupUI() override {
        DisplayLockGuard lock(this);  // 获取显示锁以防止多线程访问冲突
        Settings settings("display", false);  // 加载显示设置，不自动创建
        current_theme_name_ = settings.GetString("theme", "dark");  // 获取主题名称，默认为暗色
        if (current_theme_name_ == "dark" || current_theme_name_ == "DARK") {
            current_theme = DARK_THEME;  // 设置暗色主题
        } else if (current_theme_name_ == "light" || current_theme_name_ == "LIGHT") {
            current_theme = LIGHT_THEME;  // 设置亮色主题
        }  
        ESP_LOGI(TAG, "SetupUI --------------------------------------");  // 日志输出
        
        // 创建tabview，填充整个屏幕
        lv_obj_t * screen = lv_screen_active();  // 获取当前活动屏幕
        lv_obj_set_style_bg_color(screen, current_theme.background, 0);  // 设置屏幕背景使用主题色
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);  // 确保背景不透明，避免显示白色底层
        tabview_ = lv_tabview_create(lv_scr_act());  // 创建标签视图
        lv_obj_set_size(tabview_, lv_pct(100), lv_pct(100));  // 设置尺寸为100%占满屏幕

        // 隐藏标签栏
        lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);  // 设置标签栏位置在顶部
        lv_tabview_set_tab_bar_size(tabview_, 0);  // 设置标签栏大小为0（隐藏）
        lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview_);  // 获取标签按钮
        lv_obj_add_flag(tab_btns, LV_OBJ_FLAG_HIDDEN);  // 隐藏标签按钮

        // 设置tabview的滚动捕捉模式，确保滑动后停留在固定位置
        lv_obj_t * content = lv_tabview_get_content(tabview_);  // 获取内容区域
        lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_CENTER);  // 设置水平滚动捕捉为中心
        
        // 创建三个页面
        tab1 = lv_tabview_add_tab(tabview_, "Tab1");  // 添加第一个标签页（主界面）- 索引0
        tab2 = lv_tabview_add_tab(tabview_, "Tab2");  // 添加第二个标签页（时钟界面）- 索引1
        tab3 = lv_tabview_add_tab(tabview_, "Tab3");  // 添加第三个标签页（超级省电模式界面）- 索引2

        // 禁用tab1的滚动功能
        lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
        // 隐藏tab1的滚动条
        lv_obj_set_scrollbar_mode(tab1, LV_SCROLLBAR_MODE_OFF);
        
        // 禁用tab2的滚动功能
        lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
        // 隐藏tab2的滚动条
        lv_obj_set_scrollbar_mode(tab2, LV_SCROLLBAR_MODE_OFF);
        
        // 禁用tab3的滚动功能
        lv_obj_clear_flag(tab3, LV_OBJ_FLAG_SCROLLABLE);
        // 隐藏tab3的滚动条
        lv_obj_set_scrollbar_mode(tab3, LV_SCROLLBAR_MODE_OFF);
        
        // 设置tab2为纯黑背景（超级省电模式）
        lv_obj_set_style_bg_color(tab2, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab2, LV_OPA_COVER, 0);
        
        // 创建tab2的UI元素：中心显示大号时间（HH:MM）
        tab3_time_label_ = lv_label_create(tab2);
        lv_obj_set_style_text_font(tab3_time_label_, &time40, 0);  // 使用40像素大字体
        lv_obj_set_style_text_color(tab3_time_label_, lv_color_white(), 0);
        lv_obj_align(tab3_time_label_, LV_ALIGN_CENTER, 0, -30);  // 居中显示，向上偏移30像素
        lv_label_set_text(tab3_time_label_, "00:00");
        
        // 创建tab2的日期标签
        tab3_date_label_ = lv_label_create(tab2);
        lv_obj_set_style_text_font(tab3_date_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_date_label_, lv_color_white(), 0);
        lv_obj_align(tab3_date_label_, LV_ALIGN_CENTER, 0, 15);  // 在时间下方，向下偏移15像素
        lv_label_set_text(tab3_date_label_, "2024-01-01");
        
        // 创建tab2的星期标签
        tab3_weekday_label_ = lv_label_create(tab2);
        lv_obj_set_style_text_font(tab3_weekday_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_weekday_label_, lv_color_white(), 0);
        lv_obj_align(tab3_weekday_label_, LV_ALIGN_CENTER, 0, 40);  // 在日期下方，向下偏移40像素
        lv_label_set_text(tab3_weekday_label_, "星期一");
        
        // 创建tab2的模式提示标签
        tab3_mode_label_ = lv_label_create(tab2);
        lv_obj_set_style_text_font(tab3_mode_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_mode_label_, lv_color_make(100, 100, 100), 0);  // 灰色文字
        lv_obj_align(tab3_mode_label_, LV_ALIGN_BOTTOM_MID, 0, -20);  // 底部居中
        lv_label_set_text(tab3_mode_label_, "超级省电模式");

        // 为两个标签页添加点击事件处理
        lv_obj_add_event_cb(tab1, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保主页画布在最顶层
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_foreground(display->GetCanvas());
            }
            
            // 如果有活动的定时器，删除它
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);  // 设置tab1点击事件回调

        lv_obj_add_event_cb(tab2, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            
            // 确保时钟页面始终在最顶层
            lv_obj_move_foreground(display->tab2);
            
            // 如果有画布，将其移到background以确保不会遮挡时钟
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_background(display->GetCanvas());
            }
            
            // 如果有活动的定时器，删除它
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);  // 设置tab2点击事件回调

        // 初始化两个标签页的内容
        SetupTab1();  // 设置第一个标签页
        SetupTab2();  // 设置第二个标签页

        // 创建中央通知弹窗（需要在screen上创建，不在tabview内）
        // 半透明背景遮罩层
        center_notification_bg_ = lv_obj_create(screen);
        lv_obj_set_size(center_notification_bg_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(center_notification_bg_, 0, 0);
        lv_obj_set_style_bg_color(center_notification_bg_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(center_notification_bg_, LV_OPA_50, 0);
        lv_obj_set_style_border_width(center_notification_bg_, 0, 0);
        lv_obj_set_scrollbar_mode(center_notification_bg_, LV_SCROLLBAR_MODE_OFF);

        // 弹窗容器（固定白底黑字，不受主题影响）
        center_notification_popup_ = lv_obj_create(center_notification_bg_);
        lv_obj_set_size(center_notification_popup_, LV_HOR_RES * 0.85, LV_SIZE_CONTENT);
        lv_obj_align(center_notification_popup_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(center_notification_popup_, lv_color_white(), 0);
        lv_obj_set_style_radius(center_notification_popup_, 15, 0);
        lv_obj_set_style_pad_all(center_notification_popup_, 20, 0);
        lv_obj_set_style_border_width(center_notification_popup_, 2, 0);
        lv_obj_set_style_border_color(center_notification_popup_, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_shadow_width(center_notification_popup_, 20, 0);
        lv_obj_set_style_shadow_color(center_notification_popup_, lv_color_black(), 0);
        lv_obj_set_style_shadow_opa(center_notification_popup_, LV_OPA_30, 0);
        lv_obj_set_scrollbar_mode(center_notification_popup_, LV_SCROLLBAR_MODE_OFF);

        // 文本标签（固定黑色）
        center_notification_label_ = lv_label_create(center_notification_popup_);
        lv_label_set_long_mode(center_notification_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(center_notification_label_, LV_HOR_RES * 0.85 - 40);
        lv_obj_set_style_text_align(center_notification_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(center_notification_label_, lv_color_black(), 0);
        lv_label_set_text(center_notification_label_, "");
        lv_obj_center(center_notification_label_);

        // 初始隐藏
        lv_obj_add_flag(center_notification_bg_, LV_OBJ_FLAG_HIDDEN);
        
        ESP_LOGI(TAG, "中央通知弹窗已创建");
    }

    // 设置界面主题
    virtual void SetTheme(const std::string& theme_name) override {
        DisplayLockGuard lock(this);  // 获取显示锁

        current_theme = DARK_THEME;  // 默认设为暗色主题

        if (theme_name == "dark" || theme_name == "DARK") {
            current_theme = DARK_THEME;  // 设置暗色主题
        } else if (theme_name == "light" || theme_name == "LIGHT") {
            current_theme = LIGHT_THEME;  // 设置亮色主题
        } else {
            // 无效的主题名称，记录错误
            ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
            return;
        }
        
        // 获取当前活动屏幕
        lv_obj_t* screen = lv_screen_active();
        
        // 更新屏幕颜色 - 确保背景不透明
        lv_obj_set_style_bg_color(screen, current_theme.background, 0);  // 设置背景颜色
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);  // 设置背景不透明，避免显示白色底层
        lv_obj_set_style_text_color(screen, current_theme.text, 0);      // 设置文本颜色
        
        // 更新容器颜色 - 容器可以透明以显示背景图片
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, current_theme.background, 0);  // 设置背景颜色
            lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);  // 容器透明以显示背景图片
            lv_obj_set_style_border_color(container_, current_theme.border, 0);  // 设置边框颜色
        }
        
        // 更新状态栏颜色 - 状态栏可以透明以显示背景图片
        if (status_bar_ != nullptr) {
            lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);  // 设置背景颜色
            lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // 状态栏透明以显示背景图片
            lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);      // 设置文本颜色
            
            // 更新状态栏元素
            if (network_label_ != nullptr) {
                lv_obj_set_style_text_color(network_label_, current_theme.text, 0);  // 设置网络标签文本颜色
            }
            if (status_label_ != nullptr) {
                lv_obj_set_style_text_color(status_label_, current_theme.text, 0);  // 设置状态标签文本颜色
            }
            if (notification_label_ != nullptr) {
                lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);  // 设置通知标签文本颜色
            }
            if (mute_label_ != nullptr) {
                lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);  // 设置静音标签文本颜色
            }
            // 电池标签颜色更新 - 现在在时钟页面(Tab2)中
            if (battery_label_ != nullptr) {
                lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);  // 时钟页面使用白色
            }
            // 电池百分比标签颜色更新 - 时钟页面使用白色
            if (battery_percentage_label_ != nullptr) {
                lv_obj_set_style_text_color(battery_percentage_label_, lv_color_white(), 0);  // 时钟页面使用白色
            }
        }
        
        // 更新内容区域颜色
        if (content_ != nullptr) {
            lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);  // 设置背景颜色
            lv_obj_set_style_border_color(content_, current_theme.border, 0);       // 设置边框颜色
            
            // 简单UI模式 - 只更新主聊天消息
            if (chat_message_label_ != nullptr) {
                lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);  // 设置文本颜色
            }
        }
        
        // 更新低电量弹窗
        if (low_battery_popup_ != nullptr) {
            lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  // 设置背景颜色
        }

        // 无错误发生，保存主题到设置
        current_theme_name_ = theme_name;  // 更新当前主题名称
        Settings settings("display", true);  // 打开设置，自动创建
        settings.SetString("theme", theme_name);  // 保存主题设置
    }
    
    // 显示或隐藏下载进度条
    void ShowDownloadProgress(bool show, int progress = 0, const char* message = nullptr) {
        if (!show || !message) {
            // 隐藏UI
            UpdateDownloadProgressUI(false, 0, nullptr);
            return;
        }

        // 显示新的圆形进度条UI
        UpdateDownloadProgressUI(true, progress, message);
    }
    
public:
    // 修改成员变量，删除进度条相关变量
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr; // 百分比标签
    lv_obj_t* message_label_ = nullptr;          // 状态消息标签
    lv_obj_t* download_progress_arc_ = nullptr;  // 圆形进度条
    
    // 添加预加载UI相关变量
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_label_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    lv_obj_t* preload_progress_arc_ = nullptr;
    lv_obj_t* preload_percentage_label_ = nullptr;
    
    // 用户交互禁用状态标志
    bool user_interaction_disabled_ = false;
    
    // 更新预加载进度UI
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        // 使用DisplayLockGuard管理锁
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
            DisableUserInteraction(); // 禁用用户交互
        }
        
        // 如果容器仍不存在，直接返回
        if (preload_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            // 更新圆形进度条 - 极简版本，只显示进度
            if (preload_progress_arc_ && total > 0) {
                int progress_value = (current * 100) / total;
                if (progress_value > 100) progress_value = 100;
                if (progress_value < 0) progress_value = 0;
                lv_arc_set_value(preload_progress_arc_, progress_value);
                
                // 保持简约的蓝色，不做复杂的颜色变化
                lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
            }
            
            // 保持状态文字不变，简约显示
            if (preload_message_label_ != nullptr) {
                lv_label_set_text(preload_message_label_, "设备正在预热中...");
            }
            
            // 确保容器可见
            lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 确保在最顶层显示
            lv_obj_move_foreground(preload_progress_container_);
            
            // 如果当前在时钟页面，切换回主页面
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            // 隐藏容器
            ESP_LOGI(TAG, "预加载完成，隐藏新版预加载UI容器");
            if (preload_progress_container_) {
                lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI(TAG, "新版预加载UI容器已隐藏");
            } else {
                ESP_LOGI(TAG, "新版预加载UI容器为空，无需隐藏");
            }
            
            // 重新启用用户交互
            ESP_LOGI(TAG, "预加载完成，准备重新启用用户交互");
            EnableUserInteraction();
        }
    }
    
private:
    
    // 创建下载进度UI
    void CreateDownloadProgressUI() {
        // 创建主容器 - 白色背景
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(download_progress_container_);
        
        // 设置白色不透明背景
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_COVER, 0);  // 完全不透明
        lv_obj_set_style_border_width(download_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(download_progress_container_, 0, 0);

        // 创建圆形进度条 - 放在屏幕正中心
        lv_obj_t* progress_arc = lv_arc_create(download_progress_container_);
        lv_obj_set_size(progress_arc, 120, 120);
        lv_arc_set_rotation(progress_arc, 270); // 从顶部开始
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        
        // 将进度条居中在屏幕中心
        lv_obj_align(progress_arc, LV_ALIGN_CENTER, 0, 0);
        
        // 设置进度条样式
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN); // 深灰色背景轨道
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x00D4FF), LV_PART_INDICATOR); // 亮蓝色进度
        
        // 隐藏把手，保持简约
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        // 保存进度条引用
        download_progress_arc_ = progress_arc;

        // 在进度条中心显示百分比
        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_black(), 0);  // 黑色字体配白色背景
        lv_obj_set_style_text_align(download_progress_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(download_progress_label_, "0%");
        // 将百分比标签定位到进度条中心
        lv_obj_align_to(download_progress_label_, progress_arc, LV_ALIGN_CENTER, 0, 0);

        // 状态文字 - 放在进度条下方
        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_black(), 0);  // 黑色字体配白色背景
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(message_label_, lv_pct(80));
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(message_label_, "正在准备下载资源...");
        // 将状态文字放在进度条下方
        lv_obj_align_to(message_label_, progress_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        // 确保UI在最顶层
        lv_obj_move_foreground(download_progress_container_);
    }

    // 创建预加载进度UI
    void CreatePreloadProgressUI() {
        // 创建主容器 - 极简设计，只包含进度条和基本文字
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(preload_progress_container_);
        
        // 设置透明背景，让背景图片可见
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preload_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(preload_progress_container_, 0, 0);
        
        // 设置垂直布局，居中对齐
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(preload_progress_container_, 20, 0);

        // 创建圆形进度条 - 稍大一些，更显眼
        lv_obj_t* progress_arc = lv_arc_create(preload_progress_container_);
        lv_obj_set_size(progress_arc, 80, 80);
        lv_arc_set_rotation(progress_arc, 270); // 从顶部开始
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        
        // 设置进度条样式 - 现代简约风格
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x3A3A3C), LV_PART_MAIN); // 背景轨道
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x007AFF), LV_PART_INDICATOR); // 进度颜色
        
        // 隐藏把手，保持简约
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        // 保存进度条引用
        preload_progress_arc_ = progress_arc;

        // 只保留一个状态提示文字
        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(preload_message_label_, "设备正在预热中...");
        
        // 清空其他不需要的UI元素引用
        preload_progress_label_ = nullptr;
        preload_percentage_label_ = nullptr;
        
        // 确保UI在最顶层
        lv_obj_move_foreground(preload_progress_container_);
    }

    // 禁用用户交互
    void DisableUserInteraction() {
        user_interaction_disabled_ = true;
        ESP_LOGI(TAG, "用户交互已禁用");
        
        // 禁用空闲定时器，防止自动切换页面
        ESP_LOGI(TAG, "调用 SetIdle(false) 禁用空闲定时器");
        SetIdle(false);
    }
    
    // 启用用户交互
    void EnableUserInteraction() {
        user_interaction_disabled_ = false;
        ESP_LOGI(TAG, "用户交互已启用");
        
        // 检查是否需要播放联网成功提示音
        auto& wifi_station = WifiStation::GetInstance();
        auto& app = Application::GetInstance();
        if (wifi_station.IsConnected() && app.GetDeviceState() == kDeviceStateIdle) {
            ESP_LOGI(TAG, "设备预热完成，播放联网成功提示音");
            app.PlaySound(Lang::Sounds::P3_SUCCESS);
        }
        
        // 重新启用空闲定时器
        ESP_LOGI(TAG, "调用 SetIdle(true) 重新启用空闲定时器");
        SetIdle(true);
    }

    // 添加新方法直接更新UI，只在主线程中调用
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        // 使用DisplayLockGuard管理锁
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        
        // 如果容器仍不存在，直接返回
        if (download_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            // 确保进度值在0-100范围内
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            // 更新圆形进度条
            if (download_progress_arc_) {
                lv_arc_set_value(download_progress_arc_, progress);
                
                // 根据进度调整颜色 - 增加视觉反馈
                if (progress < 30) {
                    // 开始阶段 - 亮蓝色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
                } else if (progress < 70) {
                    // 中间阶段 - 青色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FFB3), LV_PART_INDICATOR);
                } else {
                    // 接近完成 - 绿色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FF7F), LV_PART_INDICATOR);
                }
            }
            
            // 更新中心百分比显示
            if (download_progress_label_) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            
            // 精简消息显示
            if (message && message_label_ != nullptr) {
                // 简化消息内容，只显示关键信息
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
                    // 保持原始消息，但限制长度
                    char simplified_msg[64];
                    strncpy(simplified_msg, message, sizeof(simplified_msg) - 1);
                    simplified_msg[sizeof(simplified_msg) - 1] = '\0';
                    lv_label_set_text(message_label_, simplified_msg);
                }
            }
            
            // 确保容器可见
            lv_obj_clear_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 确保在最顶层显示
            lv_obj_move_foreground(download_progress_container_);
            
            // 禁用空闲定时器
            SetIdle(false);
            
            // 如果当前在时钟页面，切换回主页面
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            // 隐藏容器
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            // 重新启用空闲定时器
            SetIdle(true);
        }
    }
};

// 自定义板卡类，继承自WifiBoard
class CustomBoard : public WifiBoard {
private:

    i2c_master_bus_handle_t codec_i2c_bus_;  // 编解码器I2C总线句柄
    CustomLcdDisplay* display_;              // LCD显示对象指针
    Button boot_btn;                         // 启动按钮
 
    esp_lcd_panel_io_handle_t io_handle = nullptr;  // LCD面板IO句柄
    esp_lcd_panel_handle_t panel = nullptr;        // LCD面板句柄

    // 图片显示任务句柄
    TaskHandle_t image_task_handle_ = nullptr;

    // 电源管理器实例
    PowerManager* power_manager_ = nullptr;
    
    // 3级省电定时器实例
    PowerSaveTimer* power_save_timer_ = nullptr;
    
    // 添加浅睡眠状态标志
    bool is_light_sleeping_ = false;
    
    // 超级省电模式状态标志
    bool is_in_super_power_save_ = false;
    
    // 闹钟提前唤醒状态标志
    bool is_alarm_pre_wake_active_ = false;
    
    // 闹钟监听定时器句柄
    lv_timer_t* alarm_monitor_timer_ = nullptr;
    
    // 音乐播放器MQTT控制器
    MqttMusicHandler* mqtt_music_handler_ = nullptr;
    
    // 屏幕旋转状态管理
    bool is_screen_rotated_ = false;
    esp_timer_handle_t rotation_check_timer_ = nullptr;  // 旋转状态检查定时器
    
    // 将URL定义为静态变量 - 现在只需要一个API URL
    static const char* API_URL;
    static const char* VERSION_URL;

    // 初始化编解码器I2C总线
    void InitializeCodecI2c() {
        // 初始化I2C外设
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,                    // 使用I2C0端口
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,    // SDA引脚
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,    // SCL引脚
            .clk_source = I2C_CLK_SRC_DEFAULT,        // 默认时钟源
            .glitch_ignore_cnt = 7,                   // 抗干扰计数
            .intr_priority = 0,                       // 中断优先级
            .trans_queue_depth = 0,                   // 传输队列深度
            .flags = {
                .enable_internal_pullup = 1,          // 启用内部上拉电阻
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  // 创建I2C主机总线
    }
 
    // 初始化SPI总线
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));    // 初始化SPI总线
    }

    // 初始化LCD显示器
    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Init GC9A01 display");
        
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));  // 创建SPI面板IO

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    // 复位引脚
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           // RGB字节序
        panel_config.bits_per_pixel = 16;                       // 每像素位数
 
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel));  // 创建GC9A01面板
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));  // 重置面板
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));  // 初始化面板
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  // 反转颜色
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));            // 是否交换XY坐标
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));  // 设置镜像
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); // 开启显示
        
        // 创建自定义LCD显示对象
        display_ = new CustomLcdDisplay(io_handle, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    
    // 屏幕旋转控制方法
    void RotateScreen(bool rotate_90_degrees) {
        if (panel == nullptr) {
            ESP_LOGE(TAG, "LCD panel未初始化，无法旋转屏幕");
            return;
        }
        
        // 暂停图片轮播任务，避免在旋转时产生冲突
        if (image_task_handle_ != nullptr) {
            vTaskSuspend(image_task_handle_);
            ESP_LOGI(TAG, "已暂停图片轮播任务");
        }
        
        // 等待一小段时间，确保任务完全暂停
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // 获取LVGL显示锁，避免与UI更新冲突
        if (display_) {
            DisplayLockGuard lock(display_);
            
            if (rotate_90_degrees) {
                // 顺时针旋转90度
                ESP_LOGI(TAG, "旋转屏幕90度");
                ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
                
                // 同步更新LVGL显示设备旋转
                lv_display_t* lv_disp = display_->GetLvDisplay();
                if (lv_disp) {
                    lv_display_set_rotation(lv_disp, LV_DISPLAY_ROTATION_270);
                    ESP_LOGI(TAG, "LVGL显示旋转已更新为270度");
                }
            } else {
                // 恢复正常角度
                ESP_LOGI(TAG, "恢复屏幕到正常角度");
                ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
                
                // 恢复LVGL显示设备旋转
                lv_display_t* lv_disp = display_->GetLvDisplay();
                if (lv_disp) {
                    lv_display_set_rotation(lv_disp, LV_DISPLAY_ROTATION_0);
                    ESP_LOGI(TAG, "LVGL显示旋转已恢复为0度");
                }
            }
            
            is_screen_rotated_ = rotate_90_degrees;
            
            // 管理定时检查定时器
            if (rotate_90_degrees) {
                // 启动定时器，每2秒检查一次USB连接状态
                if (rotation_check_timer_ == nullptr) {
                    esp_timer_create_args_t timer_args = {
                        .callback = [](void* arg) {
                            CustomBoard* self = static_cast<CustomBoard*>(arg);
                            // 检查USB是否仍然连接
                            if (self->power_manager_ && self->is_screen_rotated_) {
                                if (!self->power_manager_->IsUsbConnected()) {
                                    ESP_LOGI(TAG, "USB已断开，自动恢复屏幕");
                                    self->RotateScreen(false);
                                    if (self->display_) {
                                        self->display_->ShowCenterNotification("充电底座已断开\n屏幕已旋转显示", 3000);
                                    }
                                }
                            }
                        },
                        .arg = this,
                        .dispatch_method = ESP_TIMER_TASK,
                        .name = "rotation_check",
                        .skip_unhandled_events = true
                    };
                    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &rotation_check_timer_));
                    ESP_ERROR_CHECK(esp_timer_start_periodic(rotation_check_timer_, 2000000));  // 2秒检查一次
                    ESP_LOGI(TAG, "已启动旋转状态检查定时器");
                }
            } else {
                // 停止并删除定时器
                if (rotation_check_timer_ != nullptr) {
                    esp_timer_stop(rotation_check_timer_);
                    esp_timer_delete(rotation_check_timer_);
                    rotation_check_timer_ = nullptr;
                    ESP_LOGI(TAG, "已停止旋转状态检查定时器");
                }
            }
        }
        
        // 恢复图片轮播任务
        if (image_task_handle_ != nullptr) {
            vTaskResume(image_task_handle_);
            ESP_LOGI(TAG, "已恢复图片轮播任务");
        }
    }
    
    // 自定义按钮初始化
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                // 重置启动按钮引脚                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   // 设置为输入模式
    }

    // 按钮初始化
    void InitializeButtons() {
        boot_btn.OnClick([this]() {
            // 检查是否处于超级省电模式
            if (is_in_super_power_save_) {
                ESP_LOGI(TAG, "从超级省电模式唤醒设备");
                
                // 清除超级省电模式标志
                is_in_super_power_save_ = false;
                
                // 重新启用省电定时器
                power_save_timer_->SetEnabled(true);
                power_save_timer_->WakeUp();
                ESP_LOGI(TAG, "省电定时器已重新启用");
                
                // 恢复CPU频率到正常状态
                esp_pm_config_t pm_config = {
                    .max_freq_mhz = 160,     // 恢复到最大频率160MHz
                    .min_freq_mhz = 40,      // 最低频率40MHz
                    .light_sleep_enable = false,  // 禁用轻睡眠
                };
                esp_pm_configure(&pm_config);
                ESP_LOGI(TAG, "CPU频率已恢复到160MHz");
                
                // 恢复系统定时器
                if (power_manager_) {
                    power_manager_->StartTimer();
                }
                
                auto& app_timer = Application::GetInstance();
                app_timer.StartClockTimer();
                
                // 恢复屏幕亮度
                auto backlight = GetBacklight();
                if (backlight) {
                    backlight->RestoreBrightness();
                    ESP_LOGI(TAG, "屏幕亮度已恢复");
                }
                
                // 恢复音频处理系统
                auto& app_restore = Application::GetInstance();
                app_restore.ResumeAudioProcessing();
                ESP_LOGI(TAG, "音频处理系统已恢复");
                
                // 重新连接WiFi
                auto& wifi_station = WifiStation::GetInstance();
                ESP_LOGI(TAG, "正在重新初始化WiFi...");
                wifi_station.Start();
                
                // 等待WiFi完全初始化并连接，最多等待15秒
                int wifi_wait_count = 0;
                const int max_wifi_wait = 150; // 15秒 (150 * 100ms)
                
                ESP_LOGI(TAG, "等待WiFi完全初始化和连接...");
                while (!IsWifiFullyConnected() && wifi_wait_count < max_wifi_wait) {
                    vTaskDelay(pdMS_TO_TICKS(100));  // 等待100ms
                    wifi_wait_count++;
                    
                    // 每2秒打印一次等待状态
                    if (wifi_wait_count % 20 == 0) {
                        ESP_LOGI(TAG, "等待WiFi连接... (%d/%d 秒)", wifi_wait_count/10, max_wifi_wait/10);
                    }
                }
                
                // 检查WiFi是否真正可用
                if (IsWifiFullyConnected()) {
                    ESP_LOGI(TAG, "WiFi完全连接成功，禁用WiFi省电模式");
                    SetPowerSaveMode(false);
                    ESP_LOGI(TAG, "WiFi省电模式已禁用");
                    
                    // 启动MQTT通知服务
                    auto& app_mqtt = Application::GetInstance();
                    ESP_LOGI(TAG, "WiFi已完全连接，重新启动MQTT通知服务...");
                    app_mqtt.StartMqttNotifier();
                } else {
                    ESP_LOGW(TAG, "WiFi连接超时或驱动未完全初始化，跳过省电模式设置和MQTT连接");
                }
                
                // 恢复图片轮播任务
                ResumeImageTask();
                ESP_LOGI(TAG, "图片轮播任务已恢复");
                
                // 延迟禁用WiFi省电模式，等待WiFi完全启动后设置
                // 移至WiFi连接成功后进行
                
                // 切换回主页面（tab1）
                if (display_) {
                    CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
                    if (customDisplay->tabview_) {
                        DisplayLockGuard lock(display_);
                        ESP_LOGI(TAG, "从超级省电模式唤醒：切换回主页面（tab1）");
                        lv_tabview_set_act(customDisplay->tabview_, 0, LV_ANIM_OFF);  // 切换到tab1（索引0）
                    }
                }
                
                ESP_LOGI(TAG, "从超级省电模式完全恢复到正常状态");
                return; // 从超级省电模式唤醒时只做恢复操作，不执行其他功能
            }
            
            // 正常模式下的按钮处理
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
                ESP_LOGI(TAG, "用户交互，唤醒省电定时器");
            }
            
            // 检查用户交互是否被禁用
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略按钮点击");
                return;
            }
            
            auto& app = Application::GetInstance();
            // 如果设备正在启动且WiFi未连接，则重置WiFi配置
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  // 重置WiFi配置
            }
            app.ToggleChatState();  // 切换聊天状态
            
        });
        
        // 长按boot按键：在充电时旋转屏幕
        boot_btn.OnLongPress([this]() {
            ESP_LOGI(TAG, "检测到长按boot按键");
            
            // 检查是否处于超级省电模式
            if (is_in_super_power_save_) {
                ESP_LOGW(TAG, "设备处于超级省电模式，忽略屏幕旋转操作");
                return;
            }
            
            // 检查用户交互是否被禁用
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略屏幕旋转操作");
                return;
            }
            
            // 检查USB是否插入（直接检测USB连接状态）
            bool usb_connected = power_manager_ ? power_manager_->IsUsbConnected() : false;
            ESP_LOGI(TAG, "长按按键触发 - USB连接状态检查: %s", usb_connected ? "已连接" : "未连接");
            
            if (!usb_connected) {
                ESP_LOGW(TAG, "USB未插入，屏幕旋转功能仅在连接充电底座时可用");
                if (display_) {
                    display_->ShowCenterNotification("请连接官方充电底座", 3000);
                }
                return;
            }
            
            // 唤醒省电定时器
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
            }
            
            // 切换旋转状态
            bool new_rotation_state = !is_screen_rotated_;
            RotateScreen(new_rotation_state);
            
            // 显示操作反馈
            if (display_) {
                const char* msg = new_rotation_state ? "屏幕已旋转" : "屏幕已旋转";
                display_->ShowCenterNotification(msg, 3000);
            }
            
            ESP_LOGI(TAG, "屏幕旋转状态: %s", new_rotation_state ? "已旋转90度" : "正常");
        });
 
    }

    // 初始化IoT设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));         // 添加扬声器设备
        thing_manager.AddThing(iot::CreateThing("Screen"));          // 添加屏幕设备
        thing_manager.AddThing(iot::CreateThing("RotateDisplay"));   // 添加旋转显示设备
        thing_manager.AddThing(iot::CreateThing("ImageDisplay"));    // 添加图片显示控制设备
        thing_manager.AddThing(iot::CreateThing("SubtitleControl")); // 添加字幕控制设备
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
        
#ifdef DEBUG_CLEAR_CORRUPTED_FILES
        // 调试模式：自动清理损坏的文件
        ESP_LOGI(TAG, "调试模式：清理所有图片文件");
        image_manager.ClearAllImageFiles();
#endif
        
        esp_err_t result = image_manager.Initialize();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "图片资源管理器初始化失败");
        }
        // 开机阶段同步静默全量加载（不限时，不触发UI遮罩）
        image_manager.PreloadRemainingImagesSilent(0);
    }

    // 初始化电源管理器
    void InitializePowerManager() {
        power_manager_ = new PowerManager(CHARGING_STATUS_PIN, USB_DETECT_PIN);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            ESP_LOGI(TAG, "充电状态变化: %s", is_charging ? "充电中" : "未充电");
            // USB断开检测由定时器处理，这里只记录充电状态变化
        });
        power_manager_->OnLowBatteryStatusChanged([this](bool is_low_battery) {
            ESP_LOGI(TAG, "低电量状态变化: %s", is_low_battery ? "低电量" : "正常电量");
            if (is_low_battery && display_) {
                // 显示低电量警告
                display_->ShowNotification("电量不足，请及时充电", 5000);
            }
        });
        ESP_LOGI(TAG, "电源管理器初始化完成");
    }

    // 自定义的省电条件检查
    bool CanEnterPowerSaveMode() {
        // 1. 首先检查Application的基本条件
        auto& app = Application::GetInstance();
        if (!app.CanEnterSleepMode()) {
            return false;
        }
        
        // 2. 检查设备是否处于激活或升级状态
        DeviceState currentState = app.GetDeviceState();
        if (currentState == kDeviceStateActivating || currentState == kDeviceStateUpgrading) {
            ESP_LOGD(TAG, "设备处于激活/升级状态，不进入节能模式");
            return false;
        }
        
        // 3. 检查是否正在充电或插着电源（充电/插电时不进入节能模式）
        int battery_level;
        bool charging, discharging;
        if (GetBatteryLevel(battery_level, charging, discharging)) {
            // 正在充电时不进入节能模式
            if (charging) {
                ESP_LOGD(TAG, "设备正在充电，不进入节能模式");
                return false;
            }
            // 电量很高时，很可能插着电源（不管充电芯片是否工作）
            if (battery_level >= 95) {
                ESP_LOGD(TAG, "设备电量很高(>=95)，很可能插着电源，不进入节能模式");
                return false;
            }
        }
        
        // 4. 检查是否有下载UI可见（图片下载时不进入节能模式）
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->download_progress_container_ && 
                !lv_obj_has_flag(customDisplay->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                ESP_LOGD(TAG, "正在下载图片，不进入节能模式");
                return false;
            }
            
            // 5. 检查是否有预加载UI可见
            if (customDisplay->preload_progress_container_ && 
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                ESP_LOGD(TAG, "正在预加载图片，不进入节能模式");
                return false;
            }
            
            // 6. 检查用户交互是否被禁用（通常表示系统忙碌）
            if (customDisplay->user_interaction_disabled_) {
                ESP_LOGD(TAG, "用户交互被禁用，系统忙碌，不进入节能模式");
                return false;
            }
        }
        
        // 7. 检查是否有活动闹钟即将在1分钟内响起
#if CONFIG_USE_ALARM
        if (app.alarm_m_ != nullptr) {
            time_t now = time(NULL);
            auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
            if (next_alarm.has_value()) {
                int seconds_to_alarm = (int)(next_alarm->time - now);
                if (seconds_to_alarm > 0 && seconds_to_alarm <= 60) {
                    ESP_LOGD(TAG, "闹钟 '%s' 将在 %d 秒内响起，不进入超级省电模式", 
                             next_alarm->name.c_str(), seconds_to_alarm);
                    return false;
                }
                ESP_LOGI(TAG, "有活动闹钟 '%s'，但距离响起还有 %d 秒，仍可进入超级省电模式（将保留闹钟功能）", 
                         next_alarm->name.c_str(), seconds_to_alarm);
                // 闹钟时间还早，可以进入超级省电模式，但会保留闹钟功能
            }
        }
#endif
        
        ESP_LOGD(TAG, "系统空闲，允许进入节能模式");
        return true;
    }

    // 初始化3级省电定时器
    void InitializePowerSaveTimer() {
        // 创建3级省电定时器：60秒后进入浅睡眠，180秒后进入深度睡眠
        power_save_timer_ = new PowerSaveTimer(160, 60, 180);
        
        // 第二级：60秒后进入浅睡眠模式
        power_save_timer_->OnEnterSleepMode([this]() {
            // 检查自定义的省电条件
            if (!CanEnterPowerSaveMode()) {
                ESP_LOGI(TAG, "系统忙碌，取消进入浅睡眠模式");
                power_save_timer_->WakeUp();  // 重置定时器
                return;
            }
            ESP_LOGI(TAG, "60秒后进入浅睡眠模式");
            EnterLightSleepMode();
        });
        
        // 退出浅睡眠模式（用户交互唤醒时）
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "退出浅睡眠模式");
            ExitLightSleepMode();
        });
        
        // 第三级：180秒后进入深度睡眠模式
        power_save_timer_->OnShutdownRequest([this]() {
            // 检查自定义的省电条件
            if (!CanEnterPowerSaveMode()) {
                ESP_LOGI(TAG, "系统忙碌，取消进入超级省电模式");
                power_save_timer_->WakeUp();  // 重置定时器
                return;
            }
            ESP_LOGI(TAG, "180秒后进入超级省电模式");
            EnterDeepSleepMode();
        });
        
        // 启用省电定时器
        power_save_timer_->SetEnabled(true);
        ESP_LOGI(TAG, "3级省电定时器初始化完成 - 60秒浅睡眠, 180秒超级省电");
    }

    // 初始化音乐播放器MQTT控制器
    void InitializeMqttMusicHandler() {
        if (!mqtt_music_handler_) {
            try {
                // 获取应用程序配置
                auto& app = Application::GetInstance();
                const auto& device_config = app.GetDeviceConfig();
                
                // 创建MQTT音乐控制器实例
                mqtt_music_handler_ = new MqttMusicHandler();
                
                // 设置MQTT连接参数
                mqtt_music_handler_->SetBrokerHost(device_config.mqtt_host);
                mqtt_music_handler_->SetBrokerPort(device_config.mqtt_port);
                mqtt_music_handler_->SetUsername(device_config.mqtt_username);
                mqtt_music_handler_->SetPassword(device_config.mqtt_password);
                mqtt_music_handler_->SetClientId(device_config.device_id);
                
                // 设置音乐命令回调函数
                mqtt_music_handler_->SetMusicCommandCallback([this](const char* command, const char* params) {
                    if (display_) {
                        HandleMusicCommand(command, params);
                    }
                });
                
                // 启动MQTT连接
                if (mqtt_music_handler_->Connect()) {
                    ESP_LOGI(TAG, "MQTT音乐控制器初始化成功");
                } else {
                    ESP_LOGE(TAG, "MQTT音乐控制器连接失败");
                }
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "MQTT音乐控制器初始化异常: %s", e.what());
                if (mqtt_music_handler_) {
                    delete mqtt_music_handler_;
                    mqtt_music_handler_ = nullptr;
                }
            }
        }
    }
    
    // 处理音乐控制命令
    void HandleMusicCommand(const char* command, const char* params) {
        if (!display_) return;
        
        ESP_LOGI(TAG, "收到音乐控制命令: %s, 参数: %s", command, params ? params : "无");
        
        if (strcmp(command, "show") == 0) {
            // 解析参数并显示音乐播放器
            const char* title = "未知歌曲";
            const char* artist = "未知艺术家";
            const char* album_cover = nullptr;
            uint32_t duration_ms = 30000;
            
            if (params) {
                // 简单的参数解析（实际应用中可以使用JSON解析）
                cJSON* json = cJSON_Parse(params);
                if (json) {
                    cJSON* title_item = cJSON_GetObjectItem(json, "title");
                    if (title_item && cJSON_IsString(title_item)) {
                        title = title_item->valuestring;
                    }
                    
                    cJSON* artist_item = cJSON_GetObjectItem(json, "artist");
                    if (artist_item && cJSON_IsString(artist_item)) {
                        artist = artist_item->valuestring;
                    }
                    
                    cJSON* cover_item = cJSON_GetObjectItem(json, "album_cover");
                    if (cover_item && cJSON_IsString(cover_item)) {
                        album_cover = cover_item->valuestring;
                    }
                    
                    cJSON* duration_item = cJSON_GetObjectItem(json, "duration_ms");
                    if (duration_item && cJSON_IsNumber(duration_item)) {
                        duration_ms = (uint32_t)duration_item->valuedouble;
                    }
                    
                    cJSON_Delete(json);
                }
            }
            
            display_->ShowMusicPlayer(album_cover, title, artist, duration_ms);
            
        } else if (strcmp(command, "hide") == 0) {
            display_->HideMusicPlayer();
            
        } else if (strcmp(command, "spectrum") == 0 && params) {
            // 解析频谱数据并更新显示
            cJSON* json = cJSON_Parse(params);
            if (json) {
                cJSON* spectrum_array = cJSON_GetObjectItem(json, "spectrum");
                if (spectrum_array && cJSON_IsArray(spectrum_array)) {
                    int array_size = cJSON_GetArraySize(spectrum_array);
                    if (array_size > 0 && array_size <= 32) {
                        float spectrum_data[32] = {0};
                        for (int i = 0; i < array_size; i++) {
                            cJSON* item = cJSON_GetArrayItem(spectrum_array, i);
                            if (item && cJSON_IsNumber(item)) {
                                spectrum_data[i] = (float)item->valuedouble;
                            }
                        }
                        display_->UpdateMusicSpectrum(spectrum_data, array_size);
                    }
                }
                cJSON_Delete(json);
            }
        }
    }
    
    // 动态调整闹钟检测频率（根据下次闹钟时间智能调整）
    void AdjustAlarmCheckFrequency() {
#if CONFIG_USE_ALARM
        if (!alarm_monitor_timer_) return;
        
        auto& app = Application::GetInstance();
        if (app.alarm_m_ == nullptr) return;
        
        time_t now = time(NULL);
        auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
        
        uint32_t new_period_ms = 2000; // 默认2秒
        
        if (!next_alarm.has_value()) {
            // 无闹钟：停止检查（设置很长的间隔以节省功耗）
            new_period_ms = 60000; // 60秒
            ESP_LOGD(TAG, "无活动闹钟，检测频率降至60秒");
        } else {
            time_t alarm_time = next_alarm->time;
            int seconds_to_alarm = (int)(alarm_time - now);
            
            if (seconds_to_alarm <= 0) {
                // 闹钟已过期，使用默认频率
                new_period_ms = 2000;
            } else if (seconds_to_alarm > 7200) {
                // 闹钟超过2小时：每5分钟检查一次
                new_period_ms = 300000;  // 5分钟
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：5分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 3600) {
                // 闹钟1-2小时：每2分钟检查一次
                new_period_ms = 120000;  // 2分钟
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：2分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 1800) {
                // 闹钟30-60分钟：每1分钟检查一次
                new_period_ms = 60000;   // 1分钟
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：1分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 600) {
                // 闹钟10-30分钟：每30秒检查一次
                new_period_ms = 30000;   // 30秒
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：30秒", seconds_to_alarm);
            } else if (seconds_to_alarm > 300) {
                // 闹钟5-10分钟：每10秒检查一次
                new_period_ms = 10000;   // 10秒
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：10秒", seconds_to_alarm);
            } else {
                // 闹钟少于5分钟：每5秒检查一次（最频繁）
                new_period_ms = 5000;    // 5秒
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：5秒", seconds_to_alarm);
            }
        }
        
        // 直接更新定时器周期（LVGL没有get_period API）
        lv_timer_set_period(alarm_monitor_timer_, new_period_ms);
        ESP_LOGI(TAG, "闹钟检测频率已调整为：%lu ms", (unsigned long)new_period_ms);
#endif
    }
    
    // 初始化闹钟监听器
    void InitializeAlarmMonitor() {
#if CONFIG_USE_ALARM
        ESP_LOGI(TAG, "初始化智能动态闹钟监听器");
        
        // 创建定时器，初始每2秒检查一次，后续会动态调整
        alarm_monitor_timer_ = lv_timer_create([](lv_timer_t *t) {
            CustomBoard* board = static_cast<CustomBoard*>(lv_timer_get_user_data(t));
            if (!board) return;
            
            auto& app = Application::GetInstance();
            if (app.alarm_m_ == nullptr) return;
            
            // 获取当前时间
            time_t now = time(NULL);
            
            // 检查是否有闹钟正在响
            if (app.alarm_m_->IsRing()) {
                ESP_LOGI(TAG, "检测到闹钟触发");
                
                // 如果当前处于超级省电模式，立即唤醒
                if (board->IsInSuperPowerSaveMode()) {
                    ESP_LOGI(TAG, "闹钟触发：从超级省电模式唤醒设备");
                    board->WakeFromSuperPowerSaveMode();
                }
                
                // 清除提前唤醒标志（闹钟已触发）
                board->is_alarm_pre_wake_active_ = false;
                
                // 清除闹钟标志（避免重复处理）
                app.alarm_m_->ClearRing();
                
                // 闹钟触发后重新调整检测频率
                board->AdjustAlarmCheckFrequency();
                return;
            }
            
            // 检查是否有闹钟即将在1分钟内响起（仅在超级省电模式下检查）
            if (board->IsInSuperPowerSaveMode() && !board->is_alarm_pre_wake_active_) {
                auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
                if (next_alarm.has_value()) {
                    time_t alarm_time = next_alarm->time;
                    // 计算闹钟剩余时间
                    int seconds_to_alarm = (int)(alarm_time - now);
                    
                    // 如果闹钟在60秒内响起，提前唤醒设备
                    if (seconds_to_alarm > 0 && seconds_to_alarm <= 60) {
                        ESP_LOGI(TAG, "闹钟 '%s' 将在 %d 秒后触发，提前唤醒设备", 
                                 next_alarm->name.c_str(), seconds_to_alarm);
                        
                        // 设置提前唤醒标志，避免重复唤醒
                        board->is_alarm_pre_wake_active_ = true;
                        
                        // 从超级省电模式唤醒设备
                        board->WakeFromSuperPowerSaveMode();
                        
                        // 显示提前唤醒提示
                        auto display = board->GetDisplay();
                        if (display) {
                            char message[128];
                            snprintf(message, sizeof(message), 
                                    "闹钟 '%s' 即将响起\n设备提前唤醒准备中", 
                                    next_alarm->name.c_str());
                            display->SetChatMessage("system", message);
                        }
                    }
                }
            }
            
            // 如果设备已唤醒且不在超级省电模式，重置提前唤醒标志
            if (!board->IsInSuperPowerSaveMode() && board->is_alarm_pre_wake_active_) {
                // 检查是否还有即将响起的闹钟
                auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
                if (!next_alarm.has_value()) {
                    // 没有即将响起的闹钟，重置标志
                    board->is_alarm_pre_wake_active_ = false;
                } else {
                    time_t alarm_time = next_alarm->time;
                    int seconds_to_alarm = (int)(alarm_time - now);
                    // 如果闹钟时间已过或还有很久，重置标志
                    if (seconds_to_alarm <= 0 || seconds_to_alarm > 120) {
                        board->is_alarm_pre_wake_active_ = false;
                    }
                }
            }
            
            // 每次检查后动态调整下次检测频率
            board->AdjustAlarmCheckFrequency();
        }, 2000, this);  // 初始每2000毫秒检查一次
        
        // 立即执行一次频率调整
        AdjustAlarmCheckFrequency();
        
        ESP_LOGI(TAG, "智能动态闹钟监听器初始化完成");
#else
        ESP_LOGI(TAG, "闹钟功能未启用，跳过闹钟监听器初始化");
#endif
    }

    // 进入浅睡眠模式 - 降低功耗但保持基本功能
    void EnterLightSleepMode() {
        ESP_LOGI(TAG, "进入浅睡眠模式 - 适度降低功耗");
        
        // 设置浅睡眠状态标志
        is_light_sleeping_ = true;
        
        // 同步状态到CustomLcdDisplay
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            customDisplay->SetLightSleeping(true);
        }
        
        // 1. 降低屏幕亮度到较低水平
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->SetBrightness(10);  // 设置为较低亮度10
            ESP_LOGI(TAG, "屏幕亮度已降至10");
        }
        
        // 2. 不暂停图片任务，保持时钟正常运行
        // 图片任务在时钟页面时处于空闲状态，功耗影响很小
        ESP_LOGI(TAG, "保持图片任务运行以确保时钟正常显示");
        
        // 3. 启用WiFi省电模式（保持连接但降低功耗）
        SetPowerSaveMode(true);
        ESP_LOGI(TAG, "WiFi省电模式已启用");
        
        ESP_LOGI(TAG, "浅睡眠模式激活完成 - 时钟继续正常运行");
    }
    
    // 退出浅睡眠模式 - 恢复正常功能
    void ExitLightSleepMode() {
        ESP_LOGI(TAG, "退出浅睡眠模式 - 恢复正常功能");
        
        // 清除浅睡眠状态标志
        is_light_sleeping_ = false;
        
        // 同步状态到CustomLcdDisplay
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            customDisplay->SetLightSleeping(false);
        }
        
        // 1. 恢复屏幕亮度
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->RestoreBrightness();  // 恢复到保存的亮度
            ESP_LOGI(TAG, "屏幕亮度已恢复");
        }
        
        // 2. 图片任务已经在运行，无需恢复
        ESP_LOGI(TAG, "图片任务保持运行状态");
        
        // 3. 禁用WiFi省电模式
        SetPowerSaveMode(false);
        ESP_LOGI(TAG, "WiFi省电模式已禁用");
        
        ESP_LOGI(TAG, "浅睡眠模式退出完成");
    }

    // 暂停UI相关的LVGL定时器
    void PauseUiTimers() {
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        if (!customDisplay) return;
        
        if (customDisplay->idle_timer_) {
            lv_timer_pause(customDisplay->idle_timer_);
            ESP_LOGI(TAG, "idle_timer已暂停");
        }
        if (customDisplay->sleep_timer_) {
            lv_timer_pause(customDisplay->sleep_timer_);
            ESP_LOGI(TAG, "sleep_timer已暂停");
        }
    }
    
    // 恢复UI相关的LVGL定时器
    void ResumeUiTimers() {
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        if (!customDisplay) return;
        
        if (customDisplay->idle_timer_) {
            lv_timer_resume(customDisplay->idle_timer_);
            ESP_LOGI(TAG, "idle_timer已恢复");
        }
        if (customDisplay->sleep_timer_) {
            lv_timer_resume(customDisplay->sleep_timer_);
            ESP_LOGI(TAG, "sleep_timer已恢复");
        }
    }
    
    // 进入超级省电模式 - 关闭大部分功能，保持最低亮度显示和按键唤醒
    void EnterDeepSleepMode() {
        ESP_LOGI(TAG, "进入超级省电模式 - 检查闹钟状态");
        
        // 获取Application实例，供整个函数使用
        auto& app = Application::GetInstance();
        
        // 检查是否有活动闹钟
        bool has_active_alarm = false;
#if CONFIG_USE_ALARM
        if (app.alarm_m_ != nullptr) {
            time_t now = time(NULL);
            auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
            if (next_alarm.has_value()) {
                has_active_alarm = true;
                ESP_LOGI(TAG, "检测到活动闹钟 '%s'，将保留闹钟功能", next_alarm->name.c_str());
            }
        }
#endif
        
        // 0. 首先停止省电定时器，防止重复调用
        if (power_save_timer_) {
            power_save_timer_->SetEnabled(false);
            ESP_LOGI(TAG, "省电定时器已停止，防止重复进入超级省电模式");
        }
        
        // 设置超级省电模式标志
        is_in_super_power_save_ = true;
        
        // 1. 显示省电提示信息（在当前页面显示）
        if (display_) {
            if (has_active_alarm) {
                display_->SetChatMessage("system", "进入超级省电模式\n闹钟功能保持活跃\n按键唤醒设备");
            } else {
                display_->SetChatMessage("system", "进入超级省电模式\n按键唤醒设备");
            }
            vTaskDelay(pdMS_TO_TICKS(3000));  // 显示3秒让用户看到
        }
        
        // 2. 切换到超级省电模式专用页面（tab3）
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->tabview_) {
                DisplayLockGuard lock(display_);
                ESP_LOGI(TAG, "超级省电模式：切换到超级省电模式页面（tab3）");
                lv_tabview_set_act(customDisplay->tabview_, 2, LV_ANIM_OFF);  // 切换到tab3（索引2）
            }
        }
        
        // 2. 停止所有图片相关任务
        SuspendImageTask();
        ESP_LOGI(TAG, "图片轮播任务已停止");
        
        // 2.5. 停止屏幕旋转检查定时器（如果存在）
        if (rotation_check_timer_ != nullptr) {
            esp_timer_stop(rotation_check_timer_);
            esp_timer_delete(rotation_check_timer_);
            rotation_check_timer_ = nullptr;
            ESP_LOGI(TAG, "屏幕旋转检查定时器已停止并删除");
        }
        
        // 2.6. 暂停UI相关的LVGL定时器（idle_timer和sleep_timer）
        PauseUiTimers();
        
        // 3. 根据是否有活动闹钟决定音频系统的处理方式
        if (!has_active_alarm) {
            // 如果没有活动闹钟，完全暂停音频系统
            app.PauseAudioProcessing();
            auto codec = GetAudioCodec();
            if (codec) {
                codec->EnableInput(false);
                codec->EnableOutput(false);
            }
            ESP_LOGI(TAG, "无活动闹钟，完全关闭音频系统");
        } else {
            // 有活动闹钟时，只暂停输入，保留输出用于闹钟播放
            auto codec = GetAudioCodec();
            if (codec) {
                codec->EnableInput(false);  // 关闭输入
                // 保留输出功能用于闹钟播放
            }
            ESP_LOGI(TAG, "有活动闹钟，保留音频输出功能");
        }
        
        // 4. 先关闭MQTT连接，避免WiFi断开后的连接错误
        if (app.protocol_) {
            ESP_LOGI(TAG, "正在关闭MQTT协议连接...");
            // 关闭音频通道会同时清理MQTT连接
            app.protocol_->CloseAudioChannel();
            ESP_LOGI(TAG, "MQTT协议连接已关闭");
        }
        
        // 关闭音乐播放器MQTT控制器
        if (mqtt_music_handler_) {
            ESP_LOGI(TAG, "正在断开音乐播放器MQTT连接...");
            mqtt_music_handler_->Disconnect();
            ESP_LOGI(TAG, "音乐播放器MQTT连接已断开");
        }
        
        // 5. 关闭MQTT通知服务
        ESP_LOGI(TAG, "正在停止MQTT通知服务...");
        app.StopMqttNotifier();
        ESP_LOGI(TAG, "MQTT通知服务已停止");
        
        // 6. 关闭WiFi（闹钟触发时会重新连接）
        auto& wifi_station = WifiStation::GetInstance();
        if (wifi_station.IsConnected()) {
            wifi_station.Stop();
            ESP_LOGI(TAG, "WiFi已断开（闹钟触发时将重新连接）");
        } else {
            ESP_LOGI(TAG, "WiFi已经处于断开状态");
        }
        
        // 7. 设置屏幕亮度为1%（保持最低亮度显示）
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->SetBrightness(1);  // 设置为最低亮度1%
            ESP_LOGI(TAG, "屏幕亮度设置为1%%");
        }
        
        // 8. 停止非必要的系统定时器以减少CPU唤醒
        // 停止PowerManager的电池检测定时器
        if (power_manager_) {
            power_manager_->StopTimer();
        }
        
        // 停止Application的时钟定时器（内存监控）
        auto& app_timer = Application::GetInstance();
        app_timer.StopClockTimer();
        
        // 8.5. 根据闹钟状态调整LVGL定时器
        if (!has_active_alarm) {
            // 无活动闹钟：暂停alarm_monitor_timer_以节省更多功耗
            if (alarm_monitor_timer_) {
                lv_timer_pause(alarm_monitor_timer_);
                ESP_LOGI(TAG, "无活动闹钟，已暂停alarm_monitor_timer_");
            }
        }
        // 有活动闹钟时：保持alarm_monitor_timer_运行，它有自己的动态调整机制
        
        // 延长其他LVGL定时器的刷新间隔以降低功耗
        ESP_LOGI(TAG, "正在延长LVGL刷新间隔以降低功耗...");
        lv_timer_t* timer = lv_timer_get_next(NULL);
        int timer_count = 0;
        while (timer != NULL) {
            // 排除闹钟监听定时器（它有自己的动态调整机制或已被暂停）
            if (timer != alarm_monitor_timer_) {
                lv_timer_set_period(timer, 500);  // 延长到500ms
                ESP_LOGD(TAG, "LVGL定时器 %d: 设置为 500 ms", timer_count);
            }
            timer = lv_timer_get_next(timer);
            timer_count++;
        }
        ESP_LOGI(TAG, "已调整 %d 个LVGL定时器的刷新间隔", timer_count);
        
        // 9. 根据闹钟状态设置不同的电源配置
        esp_pm_config_t pm_config;
        if (!has_active_alarm) {
            // 无活动闹钟：启用轻睡眠以最大化省电
            pm_config.max_freq_mhz = 40;
            pm_config.min_freq_mhz = 40;
            pm_config.light_sleep_enable = true;  // 启用轻睡眠，功耗可降至1-3mA
            esp_pm_configure(&pm_config);
            ESP_LOGI(TAG, "CPU频率降至40MHz，轻睡眠已启用（无活动闹钟，最大化省电）");
        } else {
            // 有活动闹钟：保守策略，禁用轻睡眠确保闹钟可靠性
            pm_config.max_freq_mhz = 40;
            pm_config.min_freq_mhz = 40;
            pm_config.light_sleep_enable = false;  // 禁用轻睡眠，保持定时器精确
            esp_pm_configure(&pm_config);
            ESP_LOGI(TAG, "CPU频率降至40MHz，轻睡眠已禁用（有活动闹钟，保证可靠性）");
        }
        
        // 10. 设置超级省电标志，让系统知道当前处于最低功耗模式
        ESP_LOGI(TAG, "超级省电模式激活完成 - 闹钟功能%s", 
                 has_active_alarm ? "保持活跃" : "已关闭");
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

    // 检查图片资源更新
    void CheckImageResources() {
        ESP_LOGI(TAG, "图片资源检查任务开始执行");
        ESP_LOGI(TAG, "当前任务可用堆栈: %u 字节", uxTaskGetStackHighWaterMark(NULL));
        ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 等待WiFi连接
        auto& wifi = WifiStation::GetInstance();
        int wifi_wait_count = 0;
        while (!wifi.IsConnected() && wifi_wait_count < 30) { // 最多等待60秒
            ESP_LOGI(TAG, "等待WiFi连接以检查图片资源... (%d/30)", wifi_wait_count + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));  // 从3秒减少到2秒
            wifi_wait_count++;
        }
        
        if (!wifi.IsConnected()) {
            ESP_LOGE(TAG, "WiFi连接超时，图片资源检查任务退出");
            return;
        }
        
        ESP_LOGI(TAG, "WiFi已连接，立即开始资源检查...");
        
        // 并发保护：在资源检查前取消并等待预载结束，避免读/删并发
        ESP_LOGI(TAG, "取消并等待预加载完成...");
        image_manager.CancelPreload();
        image_manager.WaitForPreloadToFinish(1000);
        ESP_LOGI(TAG, "预加载处理完成");

        // 一次性检查并更新所有资源（动画图片和logo）
        ESP_LOGI(TAG, "开始调用CheckAndUpdateAllResources");
        ESP_LOGI(TAG, "API_URL: %s", API_URL);
        ESP_LOGI(TAG, "VERSION_URL: %s", VERSION_URL);
        ESP_LOGI(TAG, "调用前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        
        esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(API_URL, VERSION_URL);
        
        ESP_LOGI(TAG, "CheckAndUpdateAllResources调用完成，结果: %s (%d)", 
                esp_err_to_name(all_resources_result), all_resources_result);
        ESP_LOGI(TAG, "调用后可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        
        // 处理一次性资源检查结果
        bool has_updates = false;
        bool has_errors = false;
        
        if (all_resources_result == ESP_OK) {
            ESP_LOGI(TAG, "图片资源更新完成（一次API请求完成所有下载）");
            has_updates = true;
        } else if (all_resources_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需更新");
            
            // 资源无需更新，设备就绪，播放开机成功提示音
            auto& app = Application::GetInstance();
            DeviceState current_state = app.GetDeviceState();
            // 支持从Starting或Idle状态触发（兼容不同初始化场景）
            if (current_state == kDeviceStateStarting || current_state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "设备就绪，播放开机成功提示音（当前状态: %d）", current_state);
                
                // 显式调用SetDeviceState来触发空闲定时器的创建
                // 这会自动调用display->SetIdle(true)并设置状态栏为"待命"
                ESP_LOGI(TAG, "调用SetDeviceState(kDeviceStateIdle)以启用空闲定时器");
                app.SetDeviceState(kDeviceStateIdle);
                
                app.PlaySound(Lang::Sounds::P3_SUCCESS);
            }
        } else {
            ESP_LOGE(TAG, "图片资源检查/下载失败");
            has_errors = true;
        }
        
        // 更新静态logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已设置");
        } else {
            ESP_LOGW(TAG, "未能获取logo图片，将使用默认显示");
        }
        
        // 仅当有实际下载更新且无严重错误时才重启
        if (has_updates && !has_errors) {
            ESP_LOGI(TAG, "图片资源有更新，2秒后重启设备...");  // 从3秒减少到2秒
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
        
        // 取消播放阶段的异步预加载，采用开机静默全量加载策略
    }

    // 启动图片循环显示任务
    void StartImageSlideshow() {
        // 设置图片资源管理器的进度回调
        auto& image_manager = ImageResourceManager::GetInstance();
        
        // 设置下载进度回调函数，更新UI进度条
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        
        image_manager.SetDownloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                // 计算正确的百分比并传递
                int percent = (total > 0) ? (current * 100 / total) : 0;
                
                // 简化：直接调用显示方法
                customDisplay->ShowDownloadProgress(message != nullptr, percent, message);
            }
        });
        
        // 设置预加载进度回调函数，更新预加载UI进度
        image_manager.SetPreloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                // 使用预加载专用的UI更新方法
                customDisplay->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        
        // 启动图片轮播任务
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 8192, this, 1, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
        
        // 设置图片资源检查回调，等待OTA检查完成后执行
        auto& app = Application::GetInstance();
        app.SetImageResourceCallback([this]() {
            ESP_LOGI(TAG, "OTA检查完成，开始检查图片资源");
            // 创建后台任务执行图片资源检查，减少堆栈大小并检查返回值
            BaseType_t task_result = xTaskCreate([](void* param) {
                CustomBoard* board = static_cast<CustomBoard*>(param);
                board->CheckImageResources();
                vTaskDelete(NULL);
            }, "img_resource_check", 8192, this, 3, NULL);  // 从16384减少到8192
            
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "图片资源检查任务创建失败，错误码: %d", task_result);
                ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
            } else {
                ESP_LOGI(TAG, "图片资源检查任务创建成功");
            }
        });
    }

    // 添加帮助函数用于创建回调参数
    template<typename T, typename... Args>
    static T* malloc_struct(Args... args) {
        T* result = (T*)malloc(sizeof(T));
        if (result) {
            *result = {args...};
        }
        return result;
    }

    // 图片循环显示任务实现
    static void ImageSlideshowTask(void* arg) {
        CustomBoard* board = static_cast<CustomBoard*>(arg);
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
        
        // Application实例已在上面获取
        
        // 获取CustomLcdDisplay实例
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
        
        // 设置图片显示参数
        int imgWidth = 240;
        int imgHeight = 240;
        
        // 创建图像描述符 - 兼容服务端RGB565格式
        lv_image_dsc_t img_dsc = {
            .header = {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_RGB565,        // 匹配服务端RGB565格式
                .flags = 0,
                .w = (uint32_t)imgWidth,             // 240像素宽度
                .h = (uint32_t)imgHeight,            // 240像素高度
                .stride = (uint32_t)(imgWidth * 2),  // 每行字节数 (240*2=480)
                .reserved_2 = 0,
            },
            .data_size = (uint32_t)(imgWidth * imgHeight * 2),  // 115200字节总大小
            .data = NULL,  // 会在更新时设置
            .reserved = NULL
        };
        
        // 创建一个图像容器，放在tab1上
        lv_obj_t* img_container = nullptr;
        lv_obj_t* img_obj = nullptr;
        
        {
            DisplayLockGuard lock(display);
            
            // 创建图像容器
            img_container = lv_obj_create(customDisplay->tab1);
            lv_obj_remove_style_all(img_container);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_center(img_container);
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            lv_obj_move_foreground(img_container);  // 确保显示在最前面
            
            // 创建图像对象
            img_obj = lv_img_create(img_container);
            lv_obj_center(img_obj);
            lv_obj_move_foreground(img_obj);
        }
        
        // 优化：减少初始化等待时间，加快图片显示
        vTaskDelay(pdMS_TO_TICKS(100));  // 从500ms减少到100ms
        
        // 尝试从资源管理器获取logo图片
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "已从资源管理器快速获取logo图片");
        } else {
            ESP_LOGW(TAG, "暂无logo图片，等待下载...");
        }
        
        // 设备启动时提前加载所有表情包（避免切换时阻塞和看门狗超时）
        ESP_LOGI(TAG, "预加载表情包资源...");
        iot::LoadAllEmoticons();
        
        // 验证加载结果
        int loaded_count = 0;
        for (int i = 0; i < 7; i++) {
            if (iot::g_emoticon_images[i] != nullptr) {
                loaded_count++;
            }
        }
        ESP_LOGI(TAG, "表情包预加载完成: %d/7", loaded_count);
        
        // 立即尝试显示图片（根据模式）
        if (g_image_display_mode == iot::MODE_STATIC && g_static_image) {
            // 如果有静态图片（logo），使用它
            DisplayLockGuard lock(display);
            img_dsc.data = g_static_image;
            lv_img_set_src(img_obj, &img_dsc);
            ESP_LOGI(TAG, "开机立即显示logo图片");
        } else if (g_image_display_mode == iot::MODE_EMOTICON) {
            // 表情包模式：显示当前表情
            iot::EmotionType current_emotion = iot::g_current_emotion;
            if (current_emotion < iot::EMOTION_UNKNOWN && iot::g_emoticon_images[current_emotion]) {
                DisplayLockGuard lock(display);
                img_dsc.data = iot::g_emoticon_images[current_emotion];
                lv_img_set_src(img_obj, &img_dsc);
                ESP_LOGI(TAG, "开机立即显示表情包：表情类型 %d", current_emotion);
            } else {
                ESP_LOGW(TAG, "表情包数据无效: emotion=%d, valid=%d, ptr=%p", 
                    current_emotion,
                    (current_emotion < iot::EMOTION_UNKNOWN),
                    iot::g_emoticon_images[current_emotion]);
            }
        } else {
            // 否则尝试使用资源管理器中的图片
            const auto& imageArray = image_manager.GetImageArray();
            if (!imageArray.empty()) {
                const uint8_t* currentImage = imageArray[0];
                if (currentImage) {
                    DisplayLockGuard lock(display);
                    img_dsc.data = currentImage;
                    lv_img_set_src(img_obj, &img_dsc);
                    ESP_LOGI(TAG, "开机立即显示存储的图片");
                } else {
                    ESP_LOGW(TAG, "图片数据为空");
                }
            } else {
                ESP_LOGW(TAG, "图片数组为空");
            }
        }
        
        // 等待预加载完成（如果正在预加载）
        ESP_LOGI(TAG, "优化检查：快速检查预加载状态...");
        int preload_check_count = 0;
        while (preload_check_count < 50) { // 从100减少到50，从最多10秒减少到5秒
            bool isPreloadActive = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadActive = true;
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
        
        // 资源检查现在由OTA完成后的回调触发，这里不再需要定时器检查
        
        // 当前索引和方向控制
        int currentIndex = 0;
        bool directionForward = true;  // 动画方向：true为正向，false为反向
        const uint8_t* currentImage = nullptr;
        
        // 添加状态跟踪变量，避免重复日志输出
        bool lastWasStaticMode = false;
        const uint8_t* lastStaticImage = nullptr;
        
        // 主循环
        TickType_t lastUpdateTime = xTaskGetTickCount();  // 记录上次更新时间
        const TickType_t cycleInterval = pdMS_TO_TICKS(150);  // 优化：从200ms调整到150ms，提高动画流畅度
        
        // 循环变量定义
        bool isAudioPlaying = false;       
        bool wasAudioPlaying = false;      
        DeviceState previousState = app.GetDeviceState();  
        bool pendingAnimationStart = false;  
        TickType_t stateChangeTime = 0;      
        
        while (true) {
            // 获取图片数组
            const auto& imageArray = image_manager.GetImageArray();
            
            // 资源检查现在由OTA完成后的回调处理，不再需要定时器
            
            // 如果没有图片资源，等待一段时间后重试
            if (imageArray.empty()) {
                static int wait_count = 0;
                wait_count++;
                
                if (wait_count <= 30) {  // 从60减少到30，等待时间从5分钟减少到2.5分钟
                    ESP_LOGW(TAG, "图片资源未加载，优化等待策略... (%d/30)", wait_count);
                    vTaskDelay(pdMS_TO_TICKS(3000));  // 从5秒减少到3秒
                    continue;
                } else {
                    ESP_LOGE(TAG, "图片资源等待超时，显示黑屏");
                    // 隐藏图像容器，显示黑屏
                    DisplayLockGuard lock(display);
                    if (img_container) {
                        lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    }
                    vTaskDelay(pdMS_TO_TICKS(5000));  // 从10秒减少到5秒后重新检查
                    wait_count = 0;  // 重置计数器
                    continue;
                }
            }
            
            // 确保currentIndex在有效范围内
            if (currentIndex >= imageArray.size()) {
                currentIndex = 0;
            }
            
            // 获取当前设备状态
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            
            // **已移除旧的严格音频保护代码，采用新的智能分级保护**
            
            // **升级：智能分级音频保护机制 - 替代过度严格的保护**
            
            // 获取音频活动级别（0=空闲, 1=待机, 2=活跃, 3=关键）
            auto audioLevel = app.GetAudioActivityLevel();
            
            // 根据音频级别确定图片播放策略
            bool shouldPauseCompletely = false;
            TickType_t dynamicCycleInterval = cycleInterval; // 默认120ms
            
            switch (audioLevel) {
                case Application::AUDIO_IDLE:
                    // 完全空闲：正常播放
                    dynamicCycleInterval = cycleInterval; // 120ms
                    shouldPauseCompletely = false;
                    break;
                    
                case Application::AUDIO_STANDBY:
                    // 待机状态：降低帧率播放
                    dynamicCycleInterval = pdMS_TO_TICKS(150); // 降到200ms
                    shouldPauseCompletely = false;
                    break;
                    
                case Application::AUDIO_ACTIVE:
                    // 活跃状态：大幅降低帧率
                    dynamicCycleInterval = pdMS_TO_TICKS(200); // 300ms
                    shouldPauseCompletely = false;
                    break;
                    
                case Application::AUDIO_CRITICAL:
                    // 关键状态：完全暂停
                    shouldPauseCompletely = true;
                    break;
            }
            
            // 根据显示模式确定是否应该动画
            bool shouldAnimate = isAudioPlaying && g_image_display_mode == iot::MODE_ANIMATED;
            
            // **智能保护：只在关键音频处理时才完全暂停**
            if (shouldPauseCompletely && shouldAnimate) {
                // 详细日志记录暂停原因（仅在必要时输出）
                static TickType_t lastLogTime = 0;
                TickType_t now = xTaskGetTickCount();
                if (now - lastLogTime > pdMS_TO_TICKS(5000)) { // 5秒输出一次日志
                    ESP_LOGI(TAG, "🔒 关键音频保护激活: 级别=%d, 完全暂停图片播放", audioLevel);
                    lastLogTime = now;
                }
                
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            
            // **智能日志：记录当前保护策略**
            {
                static Application::AudioActivityLevel lastLoggedLevel = Application::AUDIO_IDLE;
                static TickType_t lastLogTime = 0;
                TickType_t now = xTaskGetTickCount();
                
                if ((audioLevel != lastLoggedLevel || now - lastLogTime > pdMS_TO_TICKS(10000)) && shouldAnimate) {
                    const char* levelNames[] = {"空闲", "待机", "活跃", "关键"};
                    ESP_LOGI(TAG, "🎬 图片播放策略: 音频级别=%d(%s), 帧间隔=%dms", 
                            audioLevel, levelNames[audioLevel], (int)(dynamicCycleInterval * portTICK_PERIOD_MS));
                    lastLoggedLevel = audioLevel;
                    lastLogTime = now;
                }
            }
            
            // 原来的音频检测代码已被强力保护机制替代
            
            // 检查当前是否在时钟页面（tab2）
            bool isClockTabActive = false;
            if (customDisplay && customDisplay->tabview_) {
                int active_tab = lv_tabview_get_tab_act(customDisplay->tabview_);
                isClockTabActive = (active_tab == 1);
            }
            
            // 检查预加载UI是否可见
            bool isPreloadUIVisible = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadUIVisible = true;
            }
            
            // 时钟页面或预加载UI显示时的处理逻辑
            if (isClockTabActive || isPreloadUIVisible) {
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                // 主界面显示处理
                DisplayLockGuard lock(display);
                if (img_container) {
                    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
                    lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
                    lv_obj_move_to_index(img_container, 0);
                    
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        lv_obj_center(img_obj);
                        lv_obj_move_foreground(img_obj);
                    }
                }
            }
            
            // 检测到状态变为Speaking
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                // 只在动画模式下才准备启动动画
                if (g_image_display_mode == iot::MODE_ANIMATED) {
                    pendingAnimationStart = true;
                    stateChangeTime = currentTime;
                    directionForward = true;  // 重置方向为正向
                    ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
                } else {
                    ESP_LOGI(TAG, "检测到音频状态改变，当前为非动画模式，不启动动画");
                }
            }
            
            // 如果状态不是Speaking，确保isAudioPlaying为false
            if (currentState != kDeviceStateSpeaking && isAudioPlaying) {
                isAudioPlaying = false;
                ESP_LOGI(TAG, "退出说话状态，停止动画");
            }
            
            // 延迟启动动画，等待音频实际开始播放（再次确认为动画模式）
            if (pendingAnimationStart && g_image_display_mode == iot::MODE_ANIMATED && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                currentIndex = 1;  // 从第二帧开始
                directionForward = true;  // 确保方向为正向
                
                if (currentIndex < imageArray.size()) {
                    // 检查图片是否已加载（应该已经在预加载阶段完成）
                    int actual_image_index = currentIndex + 1;  // 转换为1基索引
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画启动：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画启动：图片 %d 紧急加载失败，使用第一张图片", actual_image_index);
                            currentIndex = 0;  // 回退到第一张图片
                        }
                    } else {
                        ESP_LOGI(TAG, "动画启动：图片 %d 已预加载，开始流畅播放", actual_image_index);
                    }
                    
                    currentImage = imageArray[currentIndex];
                    
                    // **优化：预先准备数据，减少锁持有时间 - 方案3实施**
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                        if (img_obj) {
                            img_dsc.data = currentImage;  // 直接使用原始图像数据
                            lv_img_set_src(img_obj, &img_dsc);
                        }
                    }
                    
                    ESP_LOGI(TAG, "开始播放动画，与音频同步");
                    
                    lastUpdateTime = currentTime;
                    isAudioPlaying = true;         
                    pendingAnimationStart = false; 
                }
            }
            
            // **升级动画播放逻辑 - 智能分级保护**
            if (shouldAnimate && !pendingAnimationStart && !shouldPauseCompletely && (currentTime - lastUpdateTime >= dynamicCycleInterval)) {
                // 根据方向更新索引
                if (directionForward) {
                    currentIndex++;
                    // 如果到达末尾，切换方向
                    if (currentIndex >= imageArray.size() - 1) {
                        directionForward = false;
                    }
                } else {
                    currentIndex--;
                    // 如果回到开始，切换方向
                    if (currentIndex <= 0) {
                        directionForward = true;
                        currentIndex = 0;  // 确保不会出现负索引
                    }
                }
                
                // 确保索引在有效范围内
                if (currentIndex >= 0 && currentIndex < imageArray.size()) {
                    // 检查图片是否已加载（应该已经在预加载阶段完成）
                    int actual_image_index = currentIndex + 1;  // 转换为1基索引
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画播放：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画播放：图片 %d 紧急加载失败，跳过此帧", actual_image_index);
                            lastUpdateTime = currentTime;
                            continue;  // 跳过这一帧，继续下一帧
                        }
                    }
                    
                    currentImage = imageArray[currentIndex];
                    
                    // **优化：预先准备数据，减少锁持有时间 - 方案3实施**
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                        if (img_obj) {
                            img_dsc.data = currentImage;  // 直接使用原始图像数据
                            lv_img_set_src(img_obj, &img_dsc);
                        }
                    }
                }
                
                lastUpdateTime = currentTime;
            }
            // 处理静态图片或表情包显示
            else if ((!isAudioPlaying && wasAudioPlaying) || 
                     (g_image_display_mode == iot::MODE_STATIC) || 
                     (g_image_display_mode == iot::MODE_EMOTICON) ||
                     (!isAudioPlaying && currentIndex != 0)) {
                
                const uint8_t* staticImage = nullptr;
                bool isStaticMode = false;
                bool isEmoticonMode = false;
                
                if (g_image_display_mode == iot::MODE_STATIC && iot::g_static_image) {
                    staticImage = iot::g_static_image;
                    isStaticMode = true;
                } else if (g_image_display_mode == iot::MODE_EMOTICON) {
                    // 表情包模式：显示当前表情
                    iot::EmotionType current_emotion = iot::g_current_emotion;
                    if (current_emotion < iot::EMOTION_UNKNOWN && iot::g_emoticon_images[current_emotion]) {
                        staticImage = iot::g_emoticon_images[current_emotion];
                        isEmoticonMode = true;
                    } else {
                        // 诊断日志
                        static TickType_t lastDiagTime = 0;
                        TickType_t now = xTaskGetTickCount();
                        if (now - lastDiagTime > pdMS_TO_TICKS(5000)) {
                            ESP_LOGW(TAG, "表情包无法显示: emotion=%d, valid=%d, ptr=%p", 
                                current_emotion, 
                                (current_emotion < iot::EMOTION_UNKNOWN),
                                iot::g_emoticon_images[current_emotion]);
                            lastDiagTime = now;
                        }
                    }
                } else if (!imageArray.empty()) {
                    currentIndex = 0;
                    staticImage = imageArray[currentIndex];
                    isStaticMode = false;
                }
                
                // **优化：预先准备数据，减少锁持有时间 - 方案3实施**
                if (staticImage) {
                    DisplayLockGuard lock(display);
                    lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                    if (img_obj) {
                        img_dsc.data = staticImage;  // 直接使用原始图像数据
                        lv_img_set_src(img_obj, &img_dsc);
                    }
                    
                    // 只在状态变化时输出日志，避免刷屏
                    if (isStaticMode != lastWasStaticMode || staticImage != lastStaticImage) {
                        const char* mode_name = isEmoticonMode ? "表情包" : (isStaticMode ? "logo" : "初始");
                        ESP_LOGI(TAG, "显示%s图片", mode_name);
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
        
        // 资源检查现在由OTA完成后的回调处理，不再使用定时器
        vTaskDelete(NULL);
    }

public:
    // 安全唤醒省电定时器的方法
    void SafeWakeUpPowerSaveTimer() {
        if (power_save_timer_) {
            power_save_timer_->WakeUp();
            ESP_LOGI(TAG, "安全唤醒省电定时器");
        }
    }
    
    // 获取浅睡眠状态
    bool IsLightSleeping() const {
        return is_light_sleeping_;
    }
    
    // 音乐播放器UI控制方法

    
    // 检查是否处于超级省电模式
    bool IsInSuperPowerSaveMode() const {
        return is_in_super_power_save_;
    }
    
    /**
     * @brief 可靠的WiFi连接状态检查，确保WiFi驱动已初始化且真正连接
     * @return true如果WiFi驱动已初始化且连接成功
     */
    bool IsWifiFullyConnected() {
        // 首先检查WiFi驱动是否已初始化
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGD(TAG, "WiFi驱动未初始化");
            return false;
        }
        
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "WiFi状态检查失败: %s", esp_err_to_name(err));
            return false;
        }
        
        // 检查WiFi是否真正连接
        auto& wifi_station = WifiStation::GetInstance();
        bool is_connected = wifi_station.IsConnected();
        
        // 额外检查：尝试获取IP地址
        std::string ip = wifi_station.GetIpAddress();
        bool has_valid_ip = !ip.empty() && ip != "0.0.0.0";
        
        ESP_LOGD(TAG, "WiFi状态检查: 驱动已初始化=%s, 连接状态=%s, IP地址=%s", 
                 err == ESP_OK ? "是" : "否",
                 is_connected ? "已连接" : "未连接", 
                 ip.c_str());
                 
        return is_connected && has_valid_ip;
    }
    
    // 从超级省电模式唤醒（由闹钟触发）
    void WakeFromSuperPowerSaveMode() {
        if (!is_in_super_power_save_) {
            return; // 不在超级省电模式，无需唤醒
        }
        
        ESP_LOGI(TAG, "从超级省电模式唤醒");
        
        // 清除超级省电模式标志
        is_in_super_power_save_ = false;
        
        // 恢复LVGL刷新间隔到正常频率
        ESP_LOGI(TAG, "正在恢复LVGL刷新间隔...");
        lv_timer_t* timer = lv_timer_get_next(NULL);
        int timer_count = 0;
        while (timer != NULL) {
            // LVGL没有get_period API，直接恢复所有定时器到5ms
            // 但要排除闹钟监听定时器（它有自己的动态调整机制）
            if (timer != alarm_monitor_timer_) {
                // 恢复到常见的LVGL刷新频率
                lv_timer_set_period(timer, 5);
                ESP_LOGD(TAG, "LVGL定时器 %d: 恢复为 5 ms", timer_count);
            }
            timer = lv_timer_get_next(timer);
            timer_count++;
        }
        ESP_LOGI(TAG, "已恢复 %d 个LVGL定时器的刷新间隔", timer_count);
        
        // 恢复UI相关的LVGL定时器（idle_timer和sleep_timer）
        ResumeUiTimers();
        
        // 恢复alarm_monitor_timer_并重新调整频率
        if (alarm_monitor_timer_) {
            lv_timer_resume(alarm_monitor_timer_);
            ESP_LOGI(TAG, "alarm_monitor_timer_已恢复");
            // 立即调整检测频率以适应当前闹钟状态
            AdjustAlarmCheckFrequency();
        }
        
        // 恢复CPU频率
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 40,
            .light_sleep_enable = false,
        };
        esp_pm_configure(&pm_config);
        ESP_LOGI(TAG, "CPU频率已恢复到160MHz");
        
        // 恢复系统定时器
        if (power_manager_) {
            power_manager_->StartTimer();
        }
        
        auto& app_timer = Application::GetInstance();
        app_timer.StartClockTimer();
        
        // 恢复屏幕亮度
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->RestoreBrightness();
            ESP_LOGI(TAG, "屏幕亮度已恢复");
        }
        
        // 恢复音频处理系统
        auto& app = Application::GetInstance();
        app.ResumeAudioProcessing();
        
        // 恢复音频编解码器
        auto codec = GetAudioCodec();
        if (codec) {
            codec->EnableInput(true);
            codec->EnableOutput(true);
            ESP_LOGI(TAG, "音频系统已恢复");
        }
        
        // 重新连接WiFi
        auto& wifi_station = WifiStation::GetInstance();
        ESP_LOGI(TAG, "正在重新初始化WiFi...");
        wifi_station.Start();
        
        // 等待WiFi完全初始化并连接，最多等待15秒
        int wifi_wait_count = 0;
        const int max_wifi_wait = 150; // 15秒 (150 * 100ms)
        
        ESP_LOGI(TAG, "等待WiFi完全初始化和连接...");
        while (!IsWifiFullyConnected() && wifi_wait_count < max_wifi_wait) {
            vTaskDelay(pdMS_TO_TICKS(100));  // 等待100ms
            wifi_wait_count++;
            
            // 每2秒打印一次等待状态
            if (wifi_wait_count % 20 == 0) {
                ESP_LOGI(TAG, "等待WiFi连接... (%d/%d 秒)", wifi_wait_count/10, max_wifi_wait/10);
            }
        }
        
        // 检查WiFi是否真正可用并启动MQTT服务
        if (IsWifiFullyConnected()) {
            ESP_LOGI(TAG, "WiFi完全连接成功，重新初始化MQTT连接...");
            
            // 重新启动MQTT通知服务
            ESP_LOGI(TAG, "重新启动MQTT通知服务...");
            app.StartMqttNotifier();
            
            // 重新连接音乐播放器MQTT控制器
            if (mqtt_music_handler_) {
                ESP_LOGI(TAG, "重新连接音乐播放器MQTT...");
                if (mqtt_music_handler_->Connect()) {
                    ESP_LOGI(TAG, "音乐播放器MQTT重连成功");
                } else {
                    ESP_LOGW(TAG, "音乐播放器MQTT重连失败");
                }
            }
        } else {
            ESP_LOGW(TAG, "WiFi连接超时或驱动未完全初始化，跳过MQTT连接");
        }
        
        // 恢复图片轮播任务
        ResumeImageTask();
        ESP_LOGI(TAG, "图片轮播任务已恢复");
        
        // 重新启用省电定时器
        if (power_save_timer_) {
            power_save_timer_->SetEnabled(true);
            power_save_timer_->WakeUp();
            ESP_LOGI(TAG, "省电定时器已重新启用");
        }
        
        // 等待WiFi初始化完成后禁用省电模式
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等待WiFi初始化
        ESP_LOGI(TAG, "禁用WiFi省电模式");
        SetPowerSaveMode(false);
        
        // 切换回主页面（tab1）
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->tabview_) {
                DisplayLockGuard lock(display_);
                ESP_LOGI(TAG, "从超级省电模式唤醒：切换回主页面（tab1）");
                lv_tabview_set_act(customDisplay->tabview_, 0, LV_ANIM_OFF);  // 切换到tab1（索引0）
            }
        }
        
        ESP_LOGI(TAG, "从超级省电模式完全恢复 - 闹钟触发唤醒完成");
    }

    // 构造函数
    CustomBoard() : boot_btn(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();        // 初始化编解码器I2C总线
        InitializeSpi();             // 初始化SPI总线
        InitializeLcdDisplay();      // 初始化LCD显示器
        // 延迟1秒点亮背光，避免瞬间强光/等待屏幕就绪
        vTaskDelay(pdMS_TO_TICKS(1500));
        GetBacklight()->RestoreBrightness();
        InitializeButtons();         // 初始化按钮
        InitializeIot();             // 初始化IoT设备
        InitializeImageResources();  // 初始化图片资源管理器
        InitializePowerManager();    // 初始化电源管理器
        InitializePowerSaveTimer();  // 初始化3级省电定时器
        
        // 初始化闹钟监听器
        InitializeAlarmMonitor();
        
        // 初始化MQTT音乐控制器
        InitializeMqttMusicHandler();
        
        // 显示初始化欢迎信息
        ShowWelcomeMessage();
        
        // 优化：启动音频设置优化
        OptimizeAudioSettings();
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }
    
    // 显示欢迎信息
    void ShowWelcomeMessage() {
        if (!display_) return;
        
        // 获取WiFi状态
        auto& wifi_station = WifiStation::GetInstance();
        
        // 检查WiFi连接状态
        if (!wifi_station.IsConnected()) {
            // 显示配网提示
            display_->SetChatMessage("system", "欢迎使用独众AI伴侣\n设备连接网络中\n");
            
            // 将此消息也添加到通知区域，确保用户能看到
            display_->ShowNotification("请配置网络连接", 0);
        } else {
            // 已连接网络，显示正常欢迎信息
            display_->SetChatMessage("system", "欢迎使用独众AI伴侣\n正在初始化...");
        }
    }

    // LED功能已完全移除

    // 获取音频编解码器对象
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  // 创建ES8311音频编解码器对象
        return &audio_codec;
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

    // 获取显示器对象
    virtual Display* GetDisplay() override {
        return display_;  // 返回LCD显示对象
    }
    
    // 获取背光控制对象
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  // 创建PWM背光控制对象
        return &backlight;
    }

    // 获取电池电量信息
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!power_manager_) {
            return false;  // 如果电源管理器未初始化，返回false
        }
        
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        level = power_manager_->GetBatteryLevel();
        
        ESP_LOGD(TAG, "电池状态 - 电量: %d%%, 充电: %s, 放电: %s", 
                level, charging ? "是" : "否", discharging ? "是" : "否");
        
        return true;
    }

    // 重写获取网络状态图标方法，安全处理WiFi关闭的情况
    virtual const char* GetNetworkStateIcon() override {
        // 在超级省电模式下，WiFi已关闭，直接返回WiFi关闭图标
        // 避免调用任何WiFi相关的API，防止ESP_ERROR_CHECK失败
        
        // 检查WiFi驱动是否已初始化（更安全的方法）
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            // WiFi未初始化（已在超级省电模式中关闭）
            return FONT_AWESOME_WIFI_OFF;
        }
        
        // WiFi已初始化，可以安全调用连接检查
        auto& wifi_station = WifiStation::GetInstance();
        if (!wifi_station.IsConnected()) {
            return FONT_AWESOME_WIFI_OFF;  // WiFi未连接时显示关闭图标
        }
        
        // WiFi已连接，调用父类方法获取详细状态
        return WifiBoard::GetNetworkStateIcon();
    }

    // 设置省电模式
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled && power_save_timer_) {
            power_save_timer_->WakeUp();  // 唤醒省电定时器
        }
        
        // 首先检查WiFi驱动是否已初始化
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            // WiFi驱动未初始化，安全跳过
            ESP_LOGW(TAG, "WiFi驱动未初始化，跳过省电模式设置 (enabled=%s)", enabled ? "true" : "false");
            return;
        }
        
        // 检查应用状态，避免在WiFi未完全启动时调用WiFi功能
        auto& app = Application::GetInstance();
        DeviceState currentState = app.GetDeviceState();
        
        // 只有在设备完全启动后才调用WiFi省电模式
        if (currentState == kDeviceStateIdle || currentState == kDeviceStateListening || 
            currentState == kDeviceStateConnecting || currentState == kDeviceStateSpeaking) {
            // 设备已完全启动，可以安全调用WiFi功能
            WifiBoard::SetPowerSaveMode(enabled);
        } else {
            ESP_LOGW(TAG, "设备未完全启动(状态:%d)，跳过WiFi省电模式设置", (int)currentState);
        }
    }

    // 析构函数
    ~CustomBoard() {
        // 如果任务在运行中，停止它
        if (image_task_handle_ != nullptr) {
            vTaskDelete(image_task_handle_);  // 删除图片显示任务
            image_task_handle_ = nullptr;
        }
        
        // 清理3级省电定时器
        if (power_save_timer_ != nullptr) {
            delete power_save_timer_;
            power_save_timer_ = nullptr;
        }
        
        // 清理电源管理器
        if (power_manager_ != nullptr) {
            delete power_manager_;
            power_manager_ = nullptr;
        }
        
        // 清理MQTT音乐控制器
        if (mqtt_music_handler_ != nullptr) {
            delete mqtt_music_handler_;
            mqtt_music_handler_ = nullptr;
        }
    }
};

// 将URL定义为静态变量 - 现在只需要一个API URL
#ifdef CONFIG_IMAGE_API_URL
const char* CustomBoard::API_URL = CONFIG_IMAGE_API_URL;
#else
const char* CustomBoard::API_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif

#ifdef CONFIG_IMAGE_VERSION_URL
const char* CustomBoard::VERSION_URL = CONFIG_IMAGE_VERSION_URL;
#else
const char* CustomBoard::VERSION_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif

// 声明自定义板卡类为当前使用的板卡
DECLARE_BOARD(CustomBoard);
