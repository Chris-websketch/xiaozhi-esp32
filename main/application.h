#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>

#include <string>
#include <mutex>
#include <list>

#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>

#include "protocol.h"
#include "ota.h"
#include "background_task.h"
#include "memory/memory_manager.h"
#include "system_info.h"

/**
 * @brief 设备配置结构
 * 包含MQTT连接所需的配置信息
 */
struct DeviceConfig {
    std::string mqtt_host;      ///< MQTT服务器地址
    int mqtt_port;              ///< MQTT服务器端口
    std::string mqtt_username;  ///< MQTT用户名
    std::string mqtt_password;  ///< MQTT密码
    std::string device_id;      ///< 设备ID（用作MQTT客户端ID）
};
#if CONFIG_USE_ALARM
//test
#include "AlarmClock.h"
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
#include "wake_word_detect.h"
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
#include "audio_processor.h"
#endif
#include "audio_processing/local_intent_detector.h"

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)

enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateFatalError
};

#define OPUS_FRAME_DURATION_MS 60  // 恢复原始值确保与服务器兼容

class Application {
public:
    static Application& GetInstance() {
        static Application instance;
        return instance;
    }
    // 删除拷贝构造函数和赋值运算符
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Start();
    DeviceState GetDeviceState() const { return device_state_; }
    bool IsVoiceDetected() const { return voice_detected_; }
    void Schedule(std::function<void()> callback);
    void SetDeviceState(DeviceState state);
    void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
    void DismissAlert();
    void AbortSpeaking(AbortReason reason);
    void ToggleChatState();
    void StartListening(bool skip_wake_message = false);
    void StopListening();
    // 快速结束监听：立即切到 idle，后台可选关闭音频通道，提升跟手性
    void StopListeningFast(bool close_channel_after = false);
    void UpdateIotStates();
    void Reboot();
    void WakeWordInvoke(const std::string& wake_word);
    void PlaySound(const std::string_view& sound);
    bool CanEnterSleepMode();
    Protocol& GetProtocol() { return *protocol_; }
    Ota& GetOta() { return ota_; }
    
    // 声波配网需要访问音频数据的公共接口
    void ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
    
    // 图片资源管理相关方法
    void SetImageResourceCallback(std::function<void()> callback);
    bool IsOtaCheckCompleted() const { return ota_check_completed_; }
    
    // 图片下载模式控制方法
    void PauseAudioProcessing();  // 暂停音频处理
    void ResumeAudioProcessing(); // 恢复音频处理
    
    // 超级省电模式控制方法
    void StopClockTimer();        // 停止时钟定时器（用于超级省电模式）
    void StartClockTimer();       // 启动时钟定时器（从超级省电模式恢复）

    // 检查音频队列是否为空（用于判断开机提示音是否播放完成）
    bool IsAudioQueueEmpty() const;

    // 闹钟预处理：停止音频录制并丢弃已收集的音频数据
    void DiscardPendingAudioForAlarm();

    // 发送闹钟消息的辅助函数
    void SendAlarmMessage();
    
    // 执行本地检测的意图
    void ExecuteLocalIntent(const intent::IntentResult& result);
    
    /**
     * @brief 停止MQTT通知服务，用于省电模式
     */
    void StopMqttNotifier();
    
    /**
     * @brief 启动MQTT通知服务，用于从省电模式恢复
     */
    void StartMqttNotifier();
    
    /**
     * @brief 通过MQTT uplink主题立即上报IoT设备状态
     */
    void TriggerMqttUplink();
    
    /**
     * @brief 获取设备配置信息
     * @return DeviceConfig 设备配置结构，包含MQTT连接参数
     */
    const DeviceConfig& GetDeviceConfig() const;

    // 内存状态跟踪变量（用于减少重复日志输出）
    mutable ImageResource::MemoryStatus last_memory_status_ = ImageResource::MemoryStatus::GOOD;
    mutable float last_pool_utilization_ = 0.0f;
    mutable bool last_pool_pressure_state_ = false;
    
    // 设备配置缓存
    mutable DeviceConfig device_config_;
    mutable bool device_config_loaded_ = false;

    // MQTT 通知回调处理
    void OnMqttNotification(const cJSON* root);

    // **新增：强力音频保护机制**
    bool IsAudioActivityHigh() const;
    bool IsAudioProcessingCritical() const;
    void SetAudioPriorityMode(bool enabled);
    int GetAudioPerformanceScore() const;
    
    // **新增：智能分级音频保护**
    enum AudioActivityLevel {
        AUDIO_IDLE = 0,        // 完全空闲，允许正常图片播放
        AUDIO_STANDBY = 1,     // 待机状态，允许低帧率播放  
        AUDIO_ACTIVE = 2,      // 活跃状态，需要降低图片优先级
        AUDIO_CRITICAL = 3     // 关键状态，完全暂停图片播放
    };
    
    AudioActivityLevel GetAudioActivityLevel() const;
    bool IsRealAudioProcessing() const;
#if CONFIG_USE_ALARM
    //test
    AlarmManager* alarm_m_ = nullptr;
    std::list<std::vector<uint8_t>> audio_decode_queue_;
    std::unique_ptr<Protocol> protocol_;

    // 闹钟预处理标志位 - 避免重复处理即将触发的闹钟
    bool alarm_pre_processing_active_ = false;
    
    // 闹钟前奏音频相关标志位
    bool alarm_prelude_playing_ = false;        // 是否正在播放闹钟前奏音频
    time_t alarm_prelude_start_time_ = 0;       // 闹钟前奏音频开始播放的时间
    std::string pending_alarm_name_;            // 待触发的闹钟名称
#endif
private:
    Application();
    ~Application();

#if CONFIG_USE_WAKE_WORD_DETECT
    WakeWordDetect wake_word_detect_;
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    AudioProcessor audio_processor_;
#endif
    intent::LocalIntentDetector local_intent_detector_;
    Ota ota_;
    std::mutex mutex_;
    std::list<std::function<void()>> main_tasks_;
    EventGroupHandle_t event_group_ = nullptr;
    esp_timer_handle_t clock_timer_handle_ = nullptr;
    volatile DeviceState device_state_ = kDeviceStateUnknown;
    ListeningMode listening_mode_ = kListeningModeAutoStop;
#if CONFIG_USE_REALTIME_CHAT
    bool realtime_chat_enabled_ = true;
#else
    bool realtime_chat_enabled_ = false;  // 使用auto模式而不是realtime模式
#endif
    bool aborted_ = false;
    bool voice_detected_ = false;
    int clock_ticks_ = 0;
    TaskHandle_t main_loop_task_handle_ = nullptr;
    TaskHandle_t check_new_version_task_handle_ = nullptr;
    
    // 图片资源管理相关变量
    bool ota_check_completed_ = false;
    std::function<void()> image_resource_callback_;
    
    // 超时处理相关变量
    bool timeout_handling_active_ = false;
    bool protocol_invalidated_by_timeout_ = false;

    // Audio encode / decode
    TaskHandle_t audio_loop_task_handle_ = nullptr;
    BackgroundTask* background_task_ = nullptr;
    std::chrono::steady_clock::time_point last_output_time_;
    std::chrono::steady_clock::time_point last_button_wake_time_;  // 记录按键唤醒时间
#if CONFIG_USE_ALARM

#else
    std::list<std::vector<uint8_t>> audio_decode_queue_;
    std::unique_ptr<Protocol> protocol_;
#endif
    std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
    std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

    OpusResampler input_resampler_;
    OpusResampler reference_resampler_;
    OpusResampler output_resampler_;

    void MainLoop();
    void OnAudioInput();
    void OnAudioOutput();
    void ResetDecoder();
    void SetDecodeSampleRate(int sample_rate, int frame_duration);
    void CheckNewVersion();
    void ShowActivationCode();
    void OnClockTimer();
    void HandleProtocolTimeout();
    void SetListeningMode(ListeningMode mode);
    void AudioLoop();

    // 轻量 MQTT 通知组件
    std::unique_ptr<class MqttNotifier> notifier_;
};

#endif // _APPLICATION_H_
