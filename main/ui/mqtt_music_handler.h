#ifndef MQTT_MUSIC_HANDLER_H
#define MQTT_MUSIC_HANDLER_H

#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <functional>
#include <string>
#include <mutex>
#include <atomic>
#include "music_player_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MQTT音乐控制器错误代码
 */
typedef enum {
    MQTT_MUSIC_OK = 0,                      // 成功
    MQTT_MUSIC_ERR_INVALID_PARAM = -1,      // 无效参数
    MQTT_MUSIC_ERR_NOT_INIT = -2,           // 未初始化
    MQTT_MUSIC_ERR_ALREADY_INIT = -3,       // 已经初始化
    MQTT_MUSIC_ERR_MQTT_CONNECT = -4,       // MQTT连接失败
    MQTT_MUSIC_ERR_JSON_PARSE = -5,         // JSON解析失败
    MQTT_MUSIC_ERR_SUBSCRIBE_FAIL = -6,     // 订阅失败
    MQTT_MUSIC_ERR_PUBLISH_FAIL = -7,       // 发布失败
    MQTT_MUSIC_ERR_TIMER_CREATE = -8        // 定时器创建失败
} mqtt_music_error_t;

/**
 * @brief MQTT重连配置
 */
typedef struct {
    uint32_t initial_retry_delay_ms;        // 初始重试延迟（毫秒）
    uint32_t max_retry_delay_ms;            // 最大重试延迟（毫秒，不超过30秒）
    uint32_t retry_backoff_multiplier;      // 退避倍数
    uint32_t max_retry_attempts;            // 最大重试次数
    bool enable_exponential_backoff;        // 启用指数退避
} mqtt_reconnect_config_t;

/**
 * @brief MQTT音乐控制器配置
 */
typedef struct {
    const char* mqtt_broker_uri;            // MQTT代理URI
    const char* client_id;                  // 客户端ID
    const char* username;                   // 用户名
    const char* password;                   // 密码
    const char* downlink_topic;             // 下行主题
    const char* uplink_topic;               // 上行主题
    mqtt_reconnect_config_t reconnect;     // 重连配置
    uint32_t keepalive_interval_s;          // 心跳间隔（秒）
    uint32_t message_timeout_ms;            // 消息超时时间（毫秒）
    bool enable_ssl;                        // 启用SSL
    bool enable_debug_log;                  // 启用调试日志
} mqtt_music_config_t;

#ifdef __cplusplus
}

/**
 * @brief MQTT音乐控制处理器类
 */
class MqttMusicHandler {
public:
    /**
     * @brief 构造函数
     */
    MqttMusicHandler();
    
    /**
     * @brief 析构函数
     */
    ~MqttMusicHandler();

    /**
     * @brief 初始化MQTT音乐控制器
     * @param config MQTT配置
     * @param music_ui 音乐播放器UI实例
     * @return 错误代码
     */
    mqtt_music_error_t Initialize(const mqtt_music_config_t* config, MusicPlayerUI* music_ui);

    /**
     * @brief 销毁MQTT音乐控制器
     * @return 错误代码
     */
    mqtt_music_error_t Destroy();

    /**
     * @brief 连接到MQTT代理
     * @return 错误代码
     */
    mqtt_music_error_t Connect();

    /**
     * @brief 断开MQTT连接
     * @return 错误代码
     */
    mqtt_music_error_t Disconnect();

    /**
     * @brief 订阅音乐控制主题
     * @return 错误代码
     */
    mqtt_music_error_t Subscribe();

    /**
     * @brief 取消订阅音乐控制主题
     * @return 错误代码
     */
    mqtt_music_error_t Unsubscribe();

    /**
     * @brief 发布音乐状态消息
     * @param status_json 状态JSON字符串
     * @return 错误代码
     */
    mqtt_music_error_t PublishStatus(const char* status_json);

    /**
     * @brief 处理接收到的MQTT消息
     * @param topic 主题
     * @param payload 消息载荷
     * @param payload_len 载荷长度
     * @return 错误代码
     */
    mqtt_music_error_t HandleMessage(const char* topic, const char* payload, size_t payload_len);

    /**
     * @brief 启动自动重连
     * @return 错误代码
     */
    mqtt_music_error_t StartAutoReconnect();

    /**
     * @brief 停止自动重连
     * @return 错误代码
     */
    mqtt_music_error_t StopAutoReconnect();

    /**
     * @brief 检查连接状态
     * @return true表示已连接
     */
    bool IsConnected() const { return connected_.load(); }

    /**
     * @brief 检查是否已初始化
     * @return true表示已初始化
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * @brief 获取重连统计信息
     * @param retry_count 重试次数
     * @param last_error 最后错误
     * @param next_retry_delay 下次重试延迟
     */
    void GetReconnectStats(uint32_t* retry_count, mqtt_music_error_t* last_error, uint32_t* next_retry_delay) const;

    /**
     * @brief 设置连接状态回调
     * @param callback 回调函数
     */
    void SetConnectionCallback(std::function<void(bool connected)> callback);

    /**
     * @brief 设置消息接收回调
     * @param callback 回调函数
     */
    void SetMessageCallback(std::function<void(const std::string& topic, const std::string& payload)> callback);

    /**
     * @brief 设置MQTT代理主机
     * @param host 主机地址
     */
    void SetBrokerHost(const std::string& host);

    /**
     * @brief 设置MQTT代理端口
     * @param port 端口号
     */
    void SetBrokerPort(int port);

    /**
     * @brief 设置MQTT用户名
     * @param username 用户名
     */
    void SetUsername(const std::string& username);

    /**
     * @brief 设置MQTT密码
     * @param password 密码
     */
    void SetPassword(const std::string& password);

    /**
     * @brief 设置MQTT客户端ID
     * @param client_id 客户端ID
     */
    void SetClientId(const std::string& client_id);

    /**
     * @brief 设置音乐命令回调
     * @param callback 回调函数
     */
    void SetMusicCommandCallback(std::function<void(const char* command, const char* params)> callback);

private:
    // 初始化状态
    bool initialized_;
    std::atomic<bool> connected_;
    std::atomic<bool> reconnect_enabled_;
    
    // 配置
    mqtt_music_config_t config_;
    
    // 音乐UI实例
    MusicPlayerUI* music_ui_;
    
    // 重连状态
    std::atomic<uint32_t> retry_count_;
    std::atomic<uint32_t> current_retry_delay_;
    mqtt_music_error_t last_error_;
    
    // 定时器
    esp_timer_handle_t reconnect_timer_;
    esp_timer_handle_t keepalive_timer_;
    
    // 回调函数
    std::function<void(bool)> connection_callback_;
    std::function<void(const std::string&, const std::string&)> message_callback_;
    std::function<void(const char*, const char*)> music_command_callback_;
    
    // 动态配置参数
    std::string broker_host_;
    int broker_port_;
    std::string username_;
    std::string password_;
    std::string client_id_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    /**
     * @brief 执行重连逻辑
     */
    void PerformReconnect();
    
    /**
     * @brief 计算下次重连延迟（指数退避）
     * @return 延迟时间（毫秒）
     */
    uint32_t CalculateNextRetryDelay();
    
    /**
     * @brief 重置重连状态
     */
    void ResetReconnectState();
    
    /**
     * @brief 解析音乐控制JSON消息
     * @param json_payload JSON载荷
     * @return 错误代码
     */
    mqtt_music_error_t ParseMusicControlMessage(const cJSON* json_payload);
    
    /**
     * @brief 验证JSON消息格式
     * @param json_payload JSON载荷
     * @return true表示格式有效
     */
    bool ValidateJsonMessage(const cJSON* json_payload);
    
    /**
     * @brief 生成状态响应JSON
     * @param action 动作类型
     * @param result 执行结果
     * @return JSON字符串
     */
    std::string GenerateStatusResponse(const char* action, mqtt_music_error_t result);
    
    /**
     * @brief 重连定时器回调
     */
    static void ReconnectTimerCallback(void* arg);
    
    /**
     * @brief 心跳定时器回调
     */
    static void KeepaliveTimerCallback(void* arg);
    
    /**
     * @brief 清理资源
     */
    void CleanupResources();
};

// C接口包装函数
extern "C" {

/**
 * @brief 初始化MQTT音乐控制器（C接口）
 * @param config MQTT配置
 * @return 错误代码
 */
mqtt_music_error_t initMqttMusicHandler(const mqtt_music_config_t* config);

/**
 * @brief 销毁MQTT音乐控制器（C接口）
 * @return 错误代码
 */
mqtt_music_error_t destroyMqttMusicHandler(void);

/**
 * @brief 连接MQTT（C接口）
 * @return 错误代码
 */
mqtt_music_error_t connectMqttMusic(void);

/**
 * @brief 断开MQTT连接（C接口）
 * @return 错误代码
 */
mqtt_music_error_t disconnectMqttMusic(void);

/**
 * @brief 发布音乐状态（C接口）
 * @param status_json 状态JSON
 * @return 错误代码
 */
mqtt_music_error_t publishMusicStatus(const char* status_json);

/**
 * @brief 获取默认MQTT配置（C接口）
 * @param client_id 客户端ID
 * @return 默认配置
 */
mqtt_music_config_t getDefaultMqttMusicConfig(const char* client_id);

/**
 * @brief 设置MQTT连接回调（C接口）
 * @param callback 回调函数
 */
void setMqttConnectionCallback(void (*callback)(bool connected));

#endif // __cplusplus

}

#endif // MQTT_MUSIC_HANDLER_H