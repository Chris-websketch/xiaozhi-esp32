#include "music_player_ui.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <new>
#include <src/misc/lv_timer_private.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 声明外部音乐图标
extern const lv_image_dsc_t MusicIcon;

// 从xiaozhi-fonts组件引入字体
extern const lv_font_t font_puhui_20_4;

static const char* TAG = "MusicPlayerUI";

// 黄金分割比例常数
static const float GOLDEN_RATIO = 1.618f;

// 全局实例指针（用于C接口）
MusicPlayerUI* g_music_player_instance = nullptr;

/**
 * @brief 构造函数
 */
MusicPlayerUI::MusicPlayerUI() 
    : initialized_(false)
    , visible_(false)
    , parent_(nullptr)
    , container_(nullptr)
    , album_cover_(nullptr)
    , song_title_label_(nullptr)
    , artist_label_(nullptr)
    , animation_timer_(nullptr)
    , auto_close_timer_(nullptr)
    , icon_rotation_anim_(nullptr)
    , esp_timer_handle_(nullptr)
{
    // 初始化性能统计
    memset(&performance_stats_, 0, sizeof(performance_stats_));
    performance_stats_.last_update_time = esp_timer_get_time();
}

/**
 * @brief 析构函数
 */
MusicPlayerUI::~MusicPlayerUI() {
    if (initialized_) {
        Destroy();
    }
}

/**
 * @brief 初始化音乐播放器UI
 */
music_player_error_t MusicPlayerUI::Initialize(lv_obj_t* parent, const music_player_config_t* config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        ESP_LOGW(TAG, "Music player UI already initialized");
        return MUSIC_PLAYER_ERR_ALREADY_INIT;
    }
    
    if (!parent || !config) {
        ESP_LOGE(TAG, "Invalid parameters for initialization");
        return MUSIC_PLAYER_ERR_INVALID_PARAM;
    }
    
    // 保存配置
    config_ = *config;
    parent_ = parent;
    
    // 创建UI组件
    music_player_error_t ret = CreateUIComponents();
    if (ret != MUSIC_PLAYER_OK) {
        ESP_LOGE(TAG, "Failed to create UI components: %d", ret);
        return ret;
    }
    

    
    // 创建ESP32定时器用于性能监控
    if (config_.enable_performance_monitor) {
        esp_timer_create_args_t timer_args = {
            .callback = EspTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "music_perf_timer"
        };
        
        esp_err_t esp_ret = esp_timer_create(&timer_args, &esp_timer_handle_);
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create ESP timer: %s", esp_err_to_name(esp_ret));
            CleanupResources();
            return MUSIC_PLAYER_ERR_TIMER_CREATE;
        }
        
        // 启动性能监控定时器（每100ms更新一次）
        esp_timer_start_periodic(esp_timer_handle_, 100000); // 100ms
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Music player UI initialized successfully");
    
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 销毁音乐播放器UI
 */
music_player_error_t MusicPlayerUI::Destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    // 隐藏界面
    if (visible_) {
        Hide();
    }
    
    // 清理资源
    CleanupResources();
    
    initialized_ = false;
    ESP_LOGI(TAG, "Music player UI destroyed");
    
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 显示音乐播放器界面
 */
music_player_error_t MusicPlayerUI::Show(uint32_t duration_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (visible_) {
        ESP_LOGW(TAG, "Music player UI already visible");
        return MUSIC_PLAYER_OK;
    }
    
    // 显示容器，使用更安全的方式
    if (container_) {
        lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
        // 让出CPU时间片，避免看门狗超时
        taskYIELD();
    }
    
    // 启动图标旋转动画
    if (config_.enable_rotation) {
        StartIconRotation();
    }
    
    // 启动动画定时器
    if (animation_timer_) {
        lv_timer_resume(animation_timer_);
    }
    
    // 设置自动关闭定时器
    uint32_t close_duration = duration_ms > 0 ? duration_ms : config_.auto_close_timeout;
    if (close_duration > 0) {
        if (auto_close_timer_) {
            lv_timer_del(auto_close_timer_);
        }
        auto_close_timer_ = lv_timer_create(AutoCloseTimerCallback, close_duration, this);
        lv_timer_set_repeat_count(auto_close_timer_, 1);
    }
    
    visible_ = true;
    
    // 最后再让出一次CPU时间，确保UI完全渲染
    taskYIELD();
    
    ESP_LOGI(TAG, "Music player UI shown, auto close in %lu ms", close_duration);
    
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 隐藏音乐播放器界面
 */
music_player_error_t MusicPlayerUI::Hide() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (!visible_) {
        return MUSIC_PLAYER_OK;
    }
    
    // 隐藏容器
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    // 停止图标旋转动画
    StopIconRotation();
    
    // 暂停动画定时器
    if (animation_timer_) {
        lv_timer_pause(animation_timer_);
    }
    
    // 删除自动关闭定时器
    if (auto_close_timer_) {
        lv_timer_del(auto_close_timer_);
        auto_close_timer_ = nullptr;
    }
    
    visible_ = false;
    
    // 调用关闭回调
    if (close_callback_) {
        close_callback_();
    }
    
    ESP_LOGI(TAG, "Music player UI hidden");
    
    return MUSIC_PLAYER_OK;
}



/**
 * @brief 设置专辑封面
 */
music_player_error_t MusicPlayerUI::SetAlbumCover(const uint8_t* image_data, uint32_t data_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (!image_data || data_size == 0) {
        // 如果没有提供图像数据，使用默认的音乐图标
        if (album_cover_) {
            lv_image_set_src(album_cover_, &MusicIcon);
        }
        ESP_LOGI(TAG, "Using default music icon");
        return MUSIC_PLAYER_OK;
    }
    
    // TODO: 实现自定义图像解码和设置
    // 这里需要根据实际的图像格式进行解码
    ESP_LOGI(TAG, "Custom album cover feature not yet implemented, using default music icon");
    
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 设置歌曲信息
 */
music_player_error_t MusicPlayerUI::SetSongInfo(const char* title, const char* artist) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (title && song_title_label_) {
        lv_label_set_text(song_title_label_, title);
    }
    
    if (artist && artist_label_) {
        lv_label_set_text(artist_label_, artist);
    }
    
    ESP_LOGI(TAG, "Song info updated: %s - %s", title ? title : "Unknown", artist ? artist : "Unknown");
    
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 处理MQTT控制消息
 */
music_player_error_t MusicPlayerUI::HandleMqttMessage(const cJSON* json_message) {
    if (!json_message) {
        return MUSIC_PLAYER_ERR_INVALID_PARAM;
    }
    
    // 解析action字段
    cJSON* action_json = cJSON_GetObjectItem(json_message, "action");
    if (!action_json || !cJSON_IsString(action_json)) {
        ESP_LOGE(TAG, "Missing or invalid action field in MQTT message");
        return MUSIC_PLAYER_ERR_MQTT_PARSE;
    }
    
    const char* action = cJSON_GetStringValue(action_json);
    ESP_LOGI(TAG, "Received MQTT action: %s", action);
    
    if (strcmp(action, "show") == 0) {
        // 解析显示持续时间
        uint32_t duration_ms = config_.display_duration_ms;
        cJSON* duration_json = cJSON_GetObjectItem(json_message, "duration_ms");
        if (duration_json && cJSON_IsNumber(duration_json)) {
            duration_ms = (uint32_t)cJSON_GetNumberValue(duration_json);
        }
        
        // 解析歌曲信息
        cJSON* title_json = cJSON_GetObjectItem(json_message, "song_title");
        cJSON* artist_json = cJSON_GetObjectItem(json_message, "artist_name");
        
        if (title_json && cJSON_IsString(title_json) && 
            artist_json && cJSON_IsString(artist_json)) {
            SetSongInfo(cJSON_GetStringValue(title_json), cJSON_GetStringValue(artist_json));
        }
        

        
        return Show(duration_ms);
        
    } else if (strcmp(action, "hide") == 0) {
        return Hide();
        

        
    } else {
        ESP_LOGW(TAG, "Unknown MQTT action: %s", action);
        return MUSIC_PLAYER_ERR_MQTT_PARSE;
    }
    
    // 不应该到达这里，但为了避免编译警告
    return MUSIC_PLAYER_ERR_MQTT_PARSE;
}

/**
 * @brief 获取性能统计信息
 */
performance_stats_t MusicPlayerUI::GetPerformanceStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return performance_stats_;
}

/**
 * @brief 设置关闭回调函数
 */
void MusicPlayerUI::SetCloseCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    close_callback_ = callback;
}

/**
 * @brief 创建UI组件
 */
music_player_error_t MusicPlayerUI::CreateUIComponents() {
    // 创建主容器
    container_ = lv_obj_create(parent_);
    if (!container_) {
        ESP_LOGE(TAG, "Failed to create main container");
        return MUSIC_PLAYER_ERR_LVGL_INIT;
    }
    
    // 设置容器样式
    lv_obj_set_size(container_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(container_, config_.background_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(container_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container_, 0, LV_PART_MAIN);
    // 禁用滚动功能并隐藏滚动条
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN); // 初始隐藏
    
    // 创建专辑封面（圆形音乐图标）
    album_cover_ = lv_image_create(container_);
    if (!album_cover_) {
        ESP_LOGE(TAG, "Failed to create album cover");
        return MUSIC_PLAYER_ERR_LVGL_INIT;
    }
    
    // 设置音乐图标
    lv_image_set_src(album_cover_, &MusicIcon);
    
    // 根据配置的缩放系数计算图标尺寸
    uint32_t cover_size = (uint32_t)(config_.center_radius * config_.icon_scale_factor);
    lv_obj_set_size(album_cover_, cover_size, cover_size);
    // 将图标稍微上移，为下方文字留出空间
    lv_obj_align(album_cover_, LV_ALIGN_CENTER, 0, -10);
    
    // 设置图像缩放以适配圆形区域
    // 原图200x200，目标尺寸为cover_size x cover_size
    uint32_t scale_factor = (cover_size * 256) / 200; // 按比例缩放到目标尺寸
    lv_image_set_scale(album_cover_, scale_factor);
    
    // 设置旋转锚点为图片中心
    lv_obj_set_style_transform_pivot_x(album_cover_, cover_size / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(album_cover_, cover_size / 2, LV_PART_MAIN);
    
    ESP_LOGI(TAG, "Music icon: original=200x200, target=%lux%lu, scale_factor=%lu", 
             cover_size, cover_size, scale_factor);
    
    // 设置圆形裁剪
    lv_obj_set_style_radius(album_cover_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(album_cover_, true, LV_PART_MAIN);
    
    // 设置边框
    lv_obj_set_style_border_color(album_cover_, config_.album_border_color, LV_PART_MAIN);
    lv_obj_set_style_border_width(album_cover_, 2, LV_PART_MAIN);
    
    // 简化阴影效果，减少渲染复杂度
    lv_obj_set_style_shadow_color(album_cover_, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(album_cover_, 4, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(album_cover_, LV_OPA_20, LV_PART_MAIN);
    
    // 创建歌曲标题标签
    song_title_label_ = lv_label_create(container_);
    if (!song_title_label_) {
        ESP_LOGE(TAG, "Failed to create song title label");
        return MUSIC_PLAYER_ERR_LVGL_INIT;
    }
    
    lv_label_set_text(song_title_label_, "Unknown Song");
    // 浅色主题：使用深色文字
    lv_obj_set_style_text_color(song_title_label_, lv_color_hex(0x333333), LV_PART_MAIN);
    // 使用xiaozhi-fonts组件的普惠字体
    lv_obj_set_style_text_font(song_title_label_, &font_puhui_20_4, LV_PART_MAIN);
    // 设置文字居中对齐
    lv_obj_set_style_text_align(song_title_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    // 启用自动换行以适应圆屏
    lv_label_set_long_mode(song_title_label_, LV_LABEL_LONG_WRAP);
    // 根据缩小的图标调整文字区域宽度，使用更大的显示宽度
    lv_obj_set_width(song_title_label_, 180); // 固定180像素宽度，适合240px圆屏
    lv_obj_align_to(song_title_label_, album_cover_, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
    
    // 创建艺术家标签
    artist_label_ = lv_label_create(container_);
    if (!artist_label_) {
        ESP_LOGE(TAG, "Failed to create artist label");
        return MUSIC_PLAYER_ERR_LVGL_INIT;
    }
    
    lv_label_set_text(artist_label_, "Unknown Artist");
    // 浅色主题：使用中等深度的灰色文字
    lv_obj_set_style_text_color(artist_label_, lv_color_hex(0x666666), LV_PART_MAIN);
    // 使用统一的普惠20像素字体
    lv_obj_set_style_text_font(artist_label_, &font_puhui_20_4, LV_PART_MAIN);
    // 设置文字居中对齐
    lv_obj_set_style_text_align(artist_label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    // 启用自动换行以适应圆屏
    lv_label_set_long_mode(artist_label_, LV_LABEL_LONG_WRAP);
    // 使用相同的宽度确保对齐一致
    lv_obj_set_width(artist_label_, 180); // 固定180像素宽度
    // 将艺术家标签放在歌曲标题下方
    lv_obj_align_to(artist_label_, song_title_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    

    
    // 创建图标旋转动画
    CreateIconRotationAnimation();
    
    ESP_LOGI(TAG, "UI components created successfully");
    return MUSIC_PLAYER_OK;
}



/**
 * @brief 更新性能统计
 */
void MusicPlayerUI::UpdatePerformanceStats() {
    uint64_t current_time = esp_timer_get_time();
    uint64_t time_diff = current_time - performance_stats_.last_update_time;
    
    if (time_diff > 0) {
        performance_stats_.frame_count++;
        
        // 计算当前FPS（每秒更新10次）
        if (time_diff >= 100000) { // 100ms
            performance_stats_.fps_current = (uint32_t)(1000000 / time_diff);
            performance_stats_.last_update_time = current_time;
            
            // 计算平均FPS
            static uint32_t fps_sum = 0;
            static uint32_t fps_count = 0;
            fps_sum += performance_stats_.fps_current;
            fps_count++;
            performance_stats_.fps_average = fps_sum / fps_count;
            
            // 重置计数器防止溢出
            if (fps_count > 1000) {
                fps_sum /= 2;
                fps_count /= 2;
            }
        }
        
        // 获取内存使用情况
        performance_stats_.memory_used = heap_caps_get_total_size(MALLOC_CAP_8BIT) - 
                                       heap_caps_get_free_size(MALLOC_CAP_8BIT);
        
        // CPU使用率（简化计算）
        performance_stats_.cpu_usage = (uint32_t)((100 * time_diff) / 100000); // 简化的CPU使用率
    }
}

/**
 * @brief 清理资源
 */
void MusicPlayerUI::CleanupResources() {
    // 删除定时器
    if (animation_timer_) {
        lv_timer_del(animation_timer_);
        animation_timer_ = nullptr;
    }
    
    if (auto_close_timer_) {
        lv_timer_del(auto_close_timer_);
        auto_close_timer_ = nullptr;
    }
    
    // 删除旋转动画
    if (icon_rotation_anim_) {
        lv_anim_del(icon_rotation_anim_, nullptr);
        delete icon_rotation_anim_;
        icon_rotation_anim_ = nullptr;
    }
    
    // 删除ESP32定时器
    if (esp_timer_handle_) {
        esp_timer_stop(esp_timer_handle_);
        esp_timer_delete(esp_timer_handle_);
        esp_timer_handle_ = nullptr;
    }
    

    
    // 清理LVGL对象
    if (container_) {
        lv_obj_del(container_);
        container_ = nullptr;
        album_cover_ = nullptr;
        song_title_label_ = nullptr;
        artist_label_ = nullptr;
    }
    

    
    ESP_LOGI(TAG, "Resources cleaned up");
}

/**
 * @brief 动画定时器回调
 */
void MusicPlayerUI::AnimationTimerCallback(lv_timer_t* timer) {
    MusicPlayerUI* instance = static_cast<MusicPlayerUI*>(timer->user_data);
    if (instance && instance->config_.enable_performance_monitor) {
        instance->UpdatePerformanceStats();
    }
}

/**
 * @brief 自动关闭定时器回调
 */
void MusicPlayerUI::AutoCloseTimerCallback(lv_timer_t* timer) {
    MusicPlayerUI* instance = static_cast<MusicPlayerUI*>(timer->user_data);
    if (instance) {
        ESP_LOGI(TAG, "Auto close timer triggered");
        instance->Hide();
    }
}

/**
 * @brief ESP32定时器回调
 */
void MusicPlayerUI::EspTimerCallback(void* arg) {
    MusicPlayerUI* instance = static_cast<MusicPlayerUI*>(arg);
    if (instance) {
        instance->UpdatePerformanceStats();
    }
}

/**
 * @brief 图标旋转动画回调
 */
void MusicPlayerUI::IconRotationCallback(void* obj, int32_t value) {
    if (obj) {
        lv_obj_set_style_transform_angle(static_cast<lv_obj_t*>(obj), value, LV_PART_MAIN);
    }
}



/**
 * @brief 应用黄金分割比例间距
 */
uint32_t MusicPlayerUI::ApplyGoldenRatioSpacing(uint32_t base_spacing) const {
    return (uint32_t)(base_spacing * GOLDEN_RATIO);
}

/**
 * @brief 创建图标旋转动画
 */
void MusicPlayerUI::CreateIconRotationAnimation() {
    if (!album_cover_ || icon_rotation_anim_) {
        return; // 已经创建或对象无效
    }
    
    icon_rotation_anim_ = new lv_anim_t();
    lv_anim_init(icon_rotation_anim_);
    
    // 设置动画目标对象
    lv_anim_set_var(icon_rotation_anim_, album_cover_);
    
    // 设置动画回调函数
    lv_anim_set_exec_cb(icon_rotation_anim_, IconRotationCallback);
    
    // 设置动画参数
    lv_anim_set_values(icon_rotation_anim_, 0, 3600); // 从0度到360度 (LVGL使用1/10度单位)
    lv_anim_set_time(icon_rotation_anim_, config_.rotation_speed_ms);
    lv_anim_set_repeat_count(icon_rotation_anim_, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(icon_rotation_anim_, lv_anim_path_linear); // 匀速旋转
    
    ESP_LOGI(TAG, "Icon rotation animation created (speed: %lu ms/rotation)", config_.rotation_speed_ms);
}

/**
 * @brief 启动图标旋转动画
 */
void MusicPlayerUI::StartIconRotation() {
    if (icon_rotation_anim_ && album_cover_) {
        lv_anim_start(icon_rotation_anim_);
        ESP_LOGI(TAG, "Icon rotation animation started (speed: %lu ms/rotation)", config_.rotation_speed_ms);
    } else {
        ESP_LOGW(TAG, "Cannot start rotation: animation or album cover not ready");
    }
}

/**
 * @brief 停止图标旋转动画
 */
void MusicPlayerUI::StopIconRotation() {
    if (icon_rotation_anim_ && album_cover_) {
        lv_anim_del(album_cover_, nullptr); // 删除对象上的所有动画
        // 重置图标角度
        lv_obj_set_style_transform_angle(album_cover_, 0, LV_PART_MAIN);
        ESP_LOGI(TAG, "Icon rotation animation stopped");
    }
}

/**
 * @brief 设置图标旋转速度
 */
void MusicPlayerUI::SetRotationSpeed(uint32_t speed_ms) {
    config_.rotation_speed_ms = speed_ms;
    
    // 如果动画正在运行，重新启动以应用新速度
    if (IsIconRotating()) {
        StopIconRotation();
        StartIconRotation();
        ESP_LOGI(TAG, "Icon rotation speed updated to %lu ms/rotation", speed_ms);
    }
}

/**
 * @brief 检查图标是否正在旋转
 */
bool MusicPlayerUI::IsIconRotating() const {
    if (album_cover_) {
        return lv_anim_count_running() > 0 && lv_anim_get(album_cover_, nullptr) != nullptr;
    }
    return false;
}

// C接口实现
extern "C" {

/**
 * @brief 创建音乐播放器UI实例（C接口）
 */
MusicPlayerUI* music_player_ui_create(const music_player_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return nullptr;
    }
    
    MusicPlayerUI* ui = new MusicPlayerUI();
    ESP_LOGI(TAG, "MusicPlayerUI instance created successfully");
    return ui;
}

/**
 * @brief 销毁音乐播放器UI实例（C接口）
 */
void music_player_ui_destroy(MusicPlayerUI* ui) {
    if (!ui) {
        ESP_LOGW(TAG, "Attempting to destroy null MusicPlayerUI instance");
        return;
    }
    
    // 确保先销毁UI资源
    ui->Destroy();
    
    // 删除实例
    delete ui;
    ESP_LOGI(TAG, "MusicPlayerUI instance destroyed successfully");
}



/**
 * @brief 显示音乐播放器界面（C接口）
 */
music_player_error_t showMusicPlayer(uint32_t duration_ms) {
    if (!g_music_player_instance) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    return g_music_player_instance->Show(duration_ms);
}

/**
 * @brief 隐藏音乐播放器界面（C接口）
 */
music_player_error_t hideMusicPlayer(void) {
    if (!g_music_player_instance) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    return g_music_player_instance->Hide();
}

/**
 * @brief 处理MQTT消息（C接口）
 */
music_player_error_t handleMqttMusicMessage(const char* json_str) {
    if (!g_music_player_instance) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (!json_str) {
        return MUSIC_PLAYER_ERR_INVALID_PARAM;
    }
    
    cJSON* json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", json_str);
        return MUSIC_PLAYER_ERR_MQTT_PARSE;
    }
    
    music_player_error_t ret = g_music_player_instance->HandleMqttMessage(json);
    cJSON_Delete(json);
    
    return ret;
}

/**
 * @brief 获取默认配置（C接口）
 */
music_player_config_t getDefaultMusicPlayerConfig(void) {
    music_player_config_t config = {};
    
    // 基本配置
    config.display_duration_ms = 30000;        // 30秒
    config.auto_close_timeout = 60000;         // 60秒
    config.enable_performance_monitor = true;
    config.enable_debug_info = false;
    // 浅色主题配色
    config.background_color = lv_color_hex(0xF5F5F5);  // 浅灰色背景
    config.album_border_color = lv_color_hex(0x666666); // 深灰色边框
    
    // 中心圆配置
    config.center_radius = 60;
    // 缩放系数: 1.0=60px, 1.2=72px, 1.5=90px, 2.0=120px
    config.icon_scale_factor = 1.2f; // 默认1.2倍 (适合1.28寸圆屏)
    config.rotation_speed_ms = 4000; // 4秒转一圈 (慢速优雅旋转)
    config.enable_rotation = true;   // 启用旋转动画
    
    return config;
}

/**
 * @brief 设置音乐图标旋转速度（C接口）
 */
music_player_error_t setMusicIconRotationSpeed(uint32_t speed_ms) {
    if (!g_music_player_instance) {
        return MUSIC_PLAYER_ERR_NOT_INIT;
    }
    
    if (speed_ms < 500 || speed_ms > 60000) { // 限制在0.5秒到60秒之间
        ESP_LOGW(TAG, "Invalid rotation speed: %lu ms, using default", speed_ms);
        speed_ms = 4000; // 使用默认值
    }
    
    // 现在可以调用公有方法
    g_music_player_instance->SetRotationSpeed(speed_ms);
    return MUSIC_PLAYER_OK;
}

/**
 * @brief 检查音乐图标是否正在旋转（C接口）
 */
bool isMusicIconRotating(void) {
    if (!g_music_player_instance) {
        return false;
    }
    
    // 检查图标是否正在旋转
    return g_music_player_instance->IsIconRotating();
}

} // extern "C"