#ifndef MUSIC_PLAYER_UI_H
#define MUSIC_PLAYER_UI_H

#include <lvgl.h>
#include <src/misc/lv_timer_private.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <cJSON.h>
#include <functional>
#include <vector>
#include <memory>
#include <mutex>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音乐播放器UI错误代码定义
 */
typedef enum {
    MUSIC_PLAYER_OK = 0,                    // 成功
    MUSIC_PLAYER_ERR_INVALID_PARAM = -1,    // 无效参数
    MUSIC_PLAYER_ERR_MEMORY_ALLOC = -2,     // 内存分配失败
    MUSIC_PLAYER_ERR_LVGL_INIT = -3,        // LVGL初始化失败
    MUSIC_PLAYER_ERR_TIMER_CREATE = -4,     // 定时器创建失败
    MUSIC_PLAYER_ERR_ALREADY_INIT = -5,     // 已经初始化
    MUSIC_PLAYER_ERR_NOT_INIT = -6,         // 未初始化
    MUSIC_PLAYER_ERR_MQTT_PARSE = -7,       // MQTT消息解析失败
    MUSIC_PLAYER_ERR_ANIMATION_FAIL = -8    // 动画创建失败
} music_player_error_t;



/**
 * @brief 音乐播放器UI配置参数
 */
typedef struct {
    uint32_t display_duration_ms;   // 显示持续时间（毫秒）
    uint32_t auto_close_timeout;    // 自动关闭超时时间（毫秒）
    uint32_t center_radius;         // 中心圆半径
    float icon_scale_factor;        // 图标缩放系数（1.0为原始大小）
    uint32_t rotation_speed_ms;     // 旋转速度（毫秒/圈）
    bool enable_rotation;           // 启用图标旋转动画
    bool enable_performance_monitor; // 启用性能监控
    bool enable_debug_info;         // 启用调试信息
    lv_color_t background_color;    // 背景颜色
    lv_color_t album_border_color;  // 专辑封面边框颜色
} music_player_config_t;

/**
 * @brief 性能监控数据
 */
typedef struct {
    uint32_t frame_count;           // 帧计数
    uint32_t fps_current;           // 当前FPS
    uint32_t fps_average;           // 平均FPS
    uint32_t memory_used;           // 内存使用量
    uint32_t cpu_usage;             // CPU使用率
    uint64_t last_update_time;      // 上次更新时间
} performance_stats_t;

/**
 * @brief MQTT控制消息结构
 */
typedef struct {
    char* action;                   // 动作类型
    uint32_t duration_ms;           // 显示持续时间
    char* album_cover_url;          // 专辑封面URL
    char* song_title;               // 歌曲标题
    char* artist_name;              // 艺术家名称
} mqtt_music_control_t;

#ifdef __cplusplus
}

/**
 * @brief 音乐播放器UI类
 */
class MusicPlayerUI {
public:
    /**
     * @brief 构造函数
     */
    MusicPlayerUI();
    
    /**
     * @brief 析构函数
     */
    ~MusicPlayerUI();

    /**
     * @brief 初始化音乐播放器UI
     * @param parent 父对象
     * @param config 配置参数
     * @return 错误代码
     */
    music_player_error_t Initialize(lv_obj_t* parent, const music_player_config_t* config);

    /**
     * @brief 销毁音乐播放器UI
     * @return 错误代码
     */
    music_player_error_t Destroy();

    /**
     * @brief 显示音乐播放器界面
     * @param duration_ms 显示持续时间（毫秒），0表示永久显示
     * @return 错误代码
     */
    music_player_error_t Show(uint32_t duration_ms = 0);

    /**
     * @brief 隐藏音乐播放器界面
     * @return 错误代码
     */
    music_player_error_t Hide();



    /**
     * @brief 设置专辑封面
     * @param image_data 图像数据
     * @param data_size 数据大小
     * @return 错误代码
     */
    music_player_error_t SetAlbumCover(const uint8_t* image_data, uint32_t data_size);

    /**
     * @brief 设置歌曲信息
     * @param title 歌曲标题
     * @param artist 艺术家名称
     * @return 错误代码
     */
    music_player_error_t SetSongInfo(const char* title, const char* artist);

    /**
     * @brief 处理MQTT控制消息
     * @param json_message JSON消息
     * @return 错误代码
     */
    music_player_error_t HandleMqttMessage(const cJSON* json_message);

    /**
     * @brief 获取性能统计信息
     * @return 性能统计数据
     */
    performance_stats_t GetPerformanceStats() const;

    /**
     * @brief 设置关闭回调函数
     * @param callback 回调函数
     */
    void SetCloseCallback(std::function<void()> callback);

    /**
     * @brief 检查是否已初始化
     * @return true表示已初始化
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * @brief 检查是否正在显示
     * @return true表示正在显示
     */
    bool IsVisible() const { return visible_; }
    
    /**
     * @brief 设置图标旋转速度
     * @param speed_ms 旋转一圈所需的时间（毫秒）
     */
    void SetRotationSpeed(uint32_t speed_ms);
    
    /**
     * @brief 检查图标是否正在旋转
     * @return true表示正在旋转
     */
    bool IsIconRotating() const;

    // 配置和状态 (public以便C接口访问)
    music_player_config_t config_;

private:
    // 初始化状态
    bool initialized_;
    bool visible_;
    
    // LVGL对象
    lv_obj_t* parent_;
    lv_obj_t* container_;
    lv_obj_t* album_cover_;
    lv_obj_t* song_title_label_;
    lv_obj_t* artist_label_;

    
    // 动画对象
    lv_timer_t* animation_timer_;
    lv_timer_t* auto_close_timer_;
    lv_anim_t* icon_rotation_anim_;  // 图标旋转动画
    
    performance_stats_t performance_stats_;

    
    // 回调函数
    std::function<void()> close_callback_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    // ESP32定时器句柄
    esp_timer_handle_t esp_timer_handle_;
    
    /**
     * @brief 创建UI组件
     */
    music_player_error_t CreateUIComponents();
    
    /**
     * @brief 创建图标旋转动画
     */
    void CreateIconRotationAnimation();
    
    /**
     * @brief 启动图标旋转动画
     */
    void StartIconRotation();
    
    /**
     * @brief 停止图标旋转动画
     */
    void StopIconRotation();
    

    
    /**
     * @brief 更新性能统计
     */
    void UpdatePerformanceStats();
    
    /**
     * @brief 清理资源
     */
    void CleanupResources();
    
    /**
     * @brief 动画定时器回调
     */
    static void AnimationTimerCallback(lv_timer_t* timer);
    
    /**
     * @brief 自动关闭定时器回调
     */
    static void AutoCloseTimerCallback(lv_timer_t* timer);
    
    /**
     * @brief ESP32定时器回调
     */
    static void EspTimerCallback(void* arg);
    
    /**
     * @brief 图标旋转动画回调
     */
    static void IconRotationCallback(void* obj, int32_t value);
    

    
    /**
     * @brief 应用黄金分割比例间距
     */
    uint32_t ApplyGoldenRatioSpacing(uint32_t base_spacing) const;
};

// C接口包装函数
extern "C" {

/**
 * @brief 创建音乐播放器UI实例（C接口）
 * @param config 配置参数
 * @return 音乐播放器UI实例指针，失败返回NULL
 */
MusicPlayerUI* music_player_ui_create(const music_player_config_t* config);

/**
 * @brief 销毁音乐播放器UI实例（C接口）
 * @param ui 音乐播放器UI实例指针
 */
void music_player_ui_destroy(MusicPlayerUI* ui);



/**
 * @brief 显示音乐播放器界面（C接口）
 * @param duration_ms 显示持续时间
 * @return 错误代码
 */
music_player_error_t showMusicPlayer(uint32_t duration_ms);

/**
 * @brief 隐藏音乐播放器界面（C接口）
 * @return 错误代码
 */
music_player_error_t hideMusicPlayer(void);

/**
 * @brief 处理MQTT消息（C接口）
 * @param json_str JSON字符串
 * @return 错误代码
 */
music_player_error_t handleMqttMusicMessage(const char* json_str);

/**
 * @brief 获取默认配置（C接口）
 * @return 默认配置参数
 */
music_player_config_t getDefaultMusicPlayerConfig(void);

/**
 * @brief 设置音乐图标旋转速度（C接口）
 * @param speed_ms 旋转一圈所需的时间（毫秒）
 * @return 错误代码
 */
music_player_error_t setMusicIconRotationSpeed(uint32_t speed_ms);

/**
 * @brief 检查音乐图标是否正在旋转（C接口）
 * @return true表示正在旋转
 */
bool isMusicIconRotating(void);

// 全局变量声明
extern MusicPlayerUI* g_music_player_instance;

#endif // __cplusplus

}

#endif // MUSIC_PLAYER_UI_H