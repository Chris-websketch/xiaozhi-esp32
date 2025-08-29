#include "mqtt_music_handler.h"
#include "music_player_ui.h"
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <new>

static const char* TAG = "MqttMusicHandler";

// 全局实例指针（用于C接口）
static MqttMusicHandler* g_mqtt_music_handler = nullptr;

// C回调函数指针
static void (*g_connection_callback)(bool) = nullptr;

/**
 * @brief 构造函数
 */
MqttMusicHandler::MqttMusicHandler()
    : initialized_(false)
    , connected_(false)
    , reconnect_enabled_(false)
    , music_ui_(nullptr)
    , retry_count_(0)
    , current_retry_delay_(0)
    , last_error_(MQTT_MUSIC_OK)
    , reconnect_timer_(nullptr)
    , keepalive_timer_(nullptr)
    , broker_port_(1883)
{
    memset(&config_, 0, sizeof(config_));
}

/**
 * @brief 析构函数
 */
MqttMusicHandler::~MqttMusicHandler() {
    if (initialized_) {
        Destroy();
    }
}

/**
 * @brief 初始化MQTT音乐控制器
 */
mqtt_music_error_t MqttMusicHandler::Initialize(const mqtt_music_config_t* config, MusicPlayerUI* music_ui) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        ESP_LOGW(TAG, "MQTT music handler already initialized");
        return MQTT_MUSIC_ERR_ALREADY_INIT;
    }
    
    if (!config || !music_ui) {
        ESP_LOGE(TAG, "Invalid parameters for initialization");
        return MQTT_MUSIC_ERR_INVALID_PARAM;
    }
    
    // 复制配置
    config_ = *config;
    music_ui_ = music_ui;
    
    // 验证配置参数
    if (!config_.mqtt_broker_uri || !config_.client_id || 
        !config_.downlink_topic || !config_.uplink_topic) {
        ESP_LOGE(TAG, "Missing required MQTT configuration parameters");
        return MQTT_MUSIC_ERR_INVALID_PARAM;
    }
    
    // 验证重连配置
    if (config_.reconnect.max_retry_delay_ms > 30000) {
        ESP_LOGW(TAG, "Max retry delay exceeds 30s, capping to 30s");
        config_.reconnect.max_retry_delay_ms = 30000;
    }
    
    // 创建重连定时器
    esp_timer_create_args_t reconnect_timer_args = {
        .callback = ReconnectTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mqtt_reconnect_timer"
    };
    
    esp_err_t ret = esp_timer_create(&reconnect_timer_args, &reconnect_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create reconnect timer: %s", esp_err_to_name(ret));
        return MQTT_MUSIC_ERR_TIMER_CREATE;
    }
    
    // 创建心跳定时器
    esp_timer_create_args_t keepalive_timer_args = {
        .callback = KeepaliveTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mqtt_keepalive_timer"
    };
    
    ret = esp_timer_create(&keepalive_timer_args, &keepalive_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create keepalive timer: %s", esp_err_to_name(ret));
        esp_timer_delete(reconnect_timer_);
        reconnect_timer_ = nullptr;
        return MQTT_MUSIC_ERR_TIMER_CREATE;
    }
    
    // 重置重连状态
    ResetReconnectState();
    
    initialized_ = true;
    ESP_LOGI(TAG, "MQTT music handler initialized successfully");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 销毁MQTT音乐控制器
 */
mqtt_music_error_t MqttMusicHandler::Destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    // 停止自动重连
    StopAutoReconnect();
    
    // 断开连接
    if (connected_.load()) {
        Disconnect();
    }
    
    // 清理资源
    CleanupResources();
    
    initialized_ = false;
    ESP_LOGI(TAG, "MQTT music handler destroyed");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 连接到MQTT代理
 */
mqtt_music_error_t MqttMusicHandler::Connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    if (connected_.load()) {
        ESP_LOGW(TAG, "MQTT already connected");
        return MQTT_MUSIC_OK;
    }
    
    ESP_LOGI(TAG, "Connecting to MQTT broker: %s", config_.mqtt_broker_uri);
    
    // TODO: 实际的MQTT连接逻辑
    // 这里需要集成实际的MQTT客户端库（如esp-mqtt）
    
    // 模拟连接成功
    connected_.store(true);
    ResetReconnectState();
    
    // 启动心跳定时器
    if (config_.keepalive_interval_s > 0) {
        esp_timer_start_periodic(keepalive_timer_, config_.keepalive_interval_s * 1000000ULL);
    }
    
    // 调用连接回调
    if (connection_callback_) {
        connection_callback_(true);
    }
    
    ESP_LOGI(TAG, "MQTT connected successfully");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 断开MQTT连接
 */
mqtt_music_error_t MqttMusicHandler::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    if (!connected_.load()) {
        return MQTT_MUSIC_OK;
    }
    
    ESP_LOGI(TAG, "Disconnecting from MQTT broker");
    
    // 停止心跳定时器
    esp_timer_stop(keepalive_timer_);
    
    // TODO: 实际的MQTT断开逻辑
    
    connected_.store(false);
    
    // 调用连接回调
    if (connection_callback_) {
        connection_callback_(false);
    }
    
    ESP_LOGI(TAG, "MQTT disconnected");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 订阅音乐控制主题
 */
mqtt_music_error_t MqttMusicHandler::Subscribe() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !connected_.load()) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    ESP_LOGI(TAG, "Subscribing to topic: %s", config_.downlink_topic);
    
    // TODO: 实际的MQTT订阅逻辑
    
    ESP_LOGI(TAG, "Successfully subscribed to music control topic");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 取消订阅音乐控制主题
 */
mqtt_music_error_t MqttMusicHandler::Unsubscribe() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !connected_.load()) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    ESP_LOGI(TAG, "Unsubscribing from topic: %s", config_.downlink_topic);
    
    // TODO: 实际的MQTT取消订阅逻辑
    
    ESP_LOGI(TAG, "Successfully unsubscribed from music control topic");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 发布音乐状态消息
 */
mqtt_music_error_t MqttMusicHandler::PublishStatus(const char* status_json) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !connected_.load()) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    if (!status_json) {
        return MQTT_MUSIC_ERR_INVALID_PARAM;
    }
    
    ESP_LOGI(TAG, "Publishing status to topic: %s", config_.uplink_topic);
    
    // TODO: 实际的MQTT发布逻辑
    
    if (config_.enable_debug_log) {
        ESP_LOGD(TAG, "Published status: %s", status_json);
    }
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 处理接收到的MQTT消息
 */
mqtt_music_error_t MqttMusicHandler::HandleMessage(const char* topic, const char* payload, size_t payload_len) {
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    if (!topic || !payload || payload_len == 0) {
        return MQTT_MUSIC_ERR_INVALID_PARAM;
    }
    
    // 检查是否是音乐控制主题
    if (strcmp(topic, config_.downlink_topic) != 0) {
        ESP_LOGW(TAG, "Received message from unexpected topic: %s", topic);
        return MQTT_MUSIC_OK; // 不是错误，只是不处理
    }
    
    // 确保payload以null结尾
    std::string payload_str(payload, payload_len);
    
    if (config_.enable_debug_log) {
        ESP_LOGD(TAG, "Received message: %s", payload_str.c_str());
    }
    
    // 解析JSON消息
    cJSON* json = cJSON_Parse(payload_str.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON message: %s", payload_str.c_str());
        return MQTT_MUSIC_ERR_JSON_PARSE;
    }
    
    // 验证消息格式
    if (!ValidateJsonMessage(json)) {
        ESP_LOGE(TAG, "Invalid JSON message format");
        cJSON_Delete(json);
        return MQTT_MUSIC_ERR_JSON_PARSE;
    }
    
    // 解析音乐控制消息
    mqtt_music_error_t result = ParseMusicControlMessage(json);
    
    // 生成状态响应
    cJSON* action_json = cJSON_GetObjectItem(json, "action");
    const char* action = action_json ? cJSON_GetStringValue(action_json) : "unknown";
    std::string response = GenerateStatusResponse(action, result);
    
    // 发布状态响应
    PublishStatus(response.c_str());
    
    // 调用消息回调
    if (message_callback_) {
        message_callback_(topic, payload_str);
    }
    
    cJSON_Delete(json);
    
    return result;
}

/**
 * @brief 启动自动重连
 */
mqtt_music_error_t MqttMusicHandler::StartAutoReconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    if (reconnect_enabled_.load()) {
        ESP_LOGW(TAG, "Auto reconnect already enabled");
        return MQTT_MUSIC_OK;
    }
    
    reconnect_enabled_.store(true);
    ESP_LOGI(TAG, "Auto reconnect enabled");
    
    // 如果当前未连接，立即开始重连
    if (!connected_.load()) {
        PerformReconnect();
    }
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 停止自动重连
 */
mqtt_music_error_t MqttMusicHandler::StopAutoReconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    reconnect_enabled_.store(false);
    
    // 停止重连定时器
    esp_timer_stop(reconnect_timer_);
    
    ESP_LOGI(TAG, "Auto reconnect disabled");
    
    return MQTT_MUSIC_OK;
}

/**
 * @brief 获取重连统计信息
 */
void MqttMusicHandler::GetReconnectStats(uint32_t* retry_count, mqtt_music_error_t* last_error, uint32_t* next_retry_delay) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (retry_count) {
        *retry_count = retry_count_.load();
    }
    
    if (last_error) {
        *last_error = last_error_;
    }
    
    if (next_retry_delay) {
        *next_retry_delay = current_retry_delay_.load();
    }
}

/**
 * @brief 设置连接状态回调
 */
void MqttMusicHandler::SetConnectionCallback(std::function<void(bool connected)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_callback_ = callback;
}

/**
 * @brief 设置消息接收回调
 */
void MqttMusicHandler::SetMessageCallback(std::function<void(const std::string& topic, const std::string& payload)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_callback_ = callback;
}

/**
 * @brief 设置MQTT代理主机
 */
void MqttMusicHandler::SetBrokerHost(const std::string& host) {
    std::lock_guard<std::mutex> lock(mutex_);
    broker_host_ = host;
    ESP_LOGI(TAG, "MQTT broker host set to: %s", host.c_str());
}

/**
 * @brief 设置MQTT代理端口
 */
void MqttMusicHandler::SetBrokerPort(int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    broker_port_ = port;
    ESP_LOGI(TAG, "MQTT broker port set to: %d", port);
}

/**
 * @brief 设置MQTT用户名
 */
void MqttMusicHandler::SetUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    username_ = username;
    ESP_LOGI(TAG, "MQTT username set to: %s", username.c_str());
}

/**
 * @brief 设置MQTT密码
 */
void MqttMusicHandler::SetPassword(const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    password_ = password;
    ESP_LOGI(TAG, "MQTT password updated");
}

/**
 * @brief 设置MQTT客户端ID
 */
void MqttMusicHandler::SetClientId(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    client_id_ = client_id;
    ESP_LOGI(TAG, "MQTT client ID set to: %s", client_id.c_str());
}

/**
 * @brief 设置音乐命令回调
 */
void MqttMusicHandler::SetMusicCommandCallback(std::function<void(const char* command, const char* params)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    music_command_callback_ = callback;
    ESP_LOGI(TAG, "Music command callback set");
}

/**
 * @brief 执行重连逻辑
 */
void MqttMusicHandler::PerformReconnect() {
    if (!reconnect_enabled_.load() || connected_.load()) {
        return;
    }
    
    // 检查是否超过最大重试次数
    if (config_.reconnect.max_retry_attempts > 0 && 
        retry_count_.load() >= config_.reconnect.max_retry_attempts) {
        ESP_LOGE(TAG, "Max retry attempts reached (%lu), stopping reconnect", 
                config_.reconnect.max_retry_attempts);
        reconnect_enabled_.store(false);
        return;
    }
    
    ESP_LOGI(TAG, "Attempting to reconnect (attempt %lu)", retry_count_.load() + 1);
    
    // 尝试连接
    mqtt_music_error_t result = Connect();
    
    if (result == MQTT_MUSIC_OK) {
        // 连接成功，订阅主题
        Subscribe();
        ResetReconnectState();
        ESP_LOGI(TAG, "Reconnection successful");
    } else {
        // 连接失败，计划下次重连
        retry_count_.fetch_add(1);
        last_error_ = result;
        
        uint32_t delay = CalculateNextRetryDelay();
        current_retry_delay_.store(delay);
        
        ESP_LOGW(TAG, "Reconnection failed (error %d), next attempt in %lu ms", result, delay);
        
        // 启动重连定时器
        esp_timer_start_once(reconnect_timer_, delay * 1000ULL);
    }
}

/**
 * @brief 计算下次重连延迟（指数退避）
 */
uint32_t MqttMusicHandler::CalculateNextRetryDelay() {
    if (!config_.reconnect.enable_exponential_backoff) {
        return config_.reconnect.initial_retry_delay_ms;
    }
    
    uint32_t delay = config_.reconnect.initial_retry_delay_ms;
    uint32_t retry_count = retry_count_.load();
    
    // 指数退避计算
    for (uint32_t i = 0; i < retry_count && delay < config_.reconnect.max_retry_delay_ms; i++) {
        delay *= config_.reconnect.retry_backoff_multiplier;
    }
    
    // 确保不超过最大延迟
    return std::min(delay, config_.reconnect.max_retry_delay_ms);
}

/**
 * @brief 重置重连状态
 */
void MqttMusicHandler::ResetReconnectState() {
    retry_count_.store(0);
    current_retry_delay_.store(0);
    last_error_ = MQTT_MUSIC_OK;
}

/**
 * @brief 解析音乐控制JSON消息
 */
mqtt_music_error_t MqttMusicHandler::ParseMusicControlMessage(const cJSON* json_payload) {
    if (!music_ui_) {
        ESP_LOGE(TAG, "Music UI instance not available");
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    // 将cJSON转换为music_player_ui可以处理的格式
    music_player_error_t result = music_ui_->HandleMqttMessage(json_payload);
    
    // 转换错误代码
    switch (result) {
        case MUSIC_PLAYER_OK:
            return MQTT_MUSIC_OK;
        case MUSIC_PLAYER_ERR_INVALID_PARAM:
            return MQTT_MUSIC_ERR_INVALID_PARAM;
        case MUSIC_PLAYER_ERR_NOT_INIT:
            return MQTT_MUSIC_ERR_NOT_INIT;
        case MUSIC_PLAYER_ERR_MQTT_PARSE:
            return MQTT_MUSIC_ERR_JSON_PARSE;
        default:
            return MQTT_MUSIC_ERR_JSON_PARSE;
    }
}

/**
 * @brief 验证JSON消息格式
 */
bool MqttMusicHandler::ValidateJsonMessage(const cJSON* json_payload) {
    if (!json_payload || !cJSON_IsObject(json_payload)) {
        return false;
    }
    
    // 检查必需的action字段
    cJSON* action = cJSON_GetObjectItem(json_payload, "action");
    if (!action || !cJSON_IsString(action)) {
        return false;
    }
    
    const char* action_str = cJSON_GetStringValue(action);
    if (!action_str) {
        return false;
    }
    
    // 验证支持的动作类型
    if (strcmp(action_str, "show") != 0 && 
        strcmp(action_str, "hide") != 0) {
        ESP_LOGW(TAG, "Unsupported action: %s", action_str);
        return false;
    }
    
    return true;
}

/**
 * @brief 生成状态响应JSON
 */
std::string MqttMusicHandler::GenerateStatusResponse(const char* action, mqtt_music_error_t result) {
    cJSON* response = cJSON_CreateObject();
    if (!response) {
        return "{}";
    }
    
    // 添加基本信息
    cJSON_AddStringToObject(response, "type", "music_player_status");
    cJSON_AddStringToObject(response, "action", action ? action : "unknown");
    cJSON_AddNumberToObject(response, "result_code", result);
    cJSON_AddStringToObject(response, "result", result == MQTT_MUSIC_OK ? "success" : "error");
    cJSON_AddNumberToObject(response, "timestamp", esp_timer_get_time() / 1000); // 毫秒时间戳
    
    // 添加设备信息
    cJSON* device_info = cJSON_CreateObject();
    if (device_info) {
        cJSON_AddStringToObject(device_info, "client_id", config_.client_id);
        cJSON_AddNumberToObject(device_info, "free_heap", esp_get_free_heap_size());
        cJSON_AddNumberToObject(device_info, "uptime_ms", esp_timer_get_time() / 1000);
        cJSON_AddItemToObject(response, "device", device_info);
    }
    
    // 添加音乐播放器状态
    if (music_ui_) {
        cJSON* player_status = cJSON_CreateObject();
        if (player_status) {
            cJSON_AddBoolToObject(player_status, "initialized", music_ui_->IsInitialized());
            cJSON_AddBoolToObject(player_status, "visible", music_ui_->IsVisible());
            
            // 添加性能统计
            performance_stats_t stats = music_ui_->GetPerformanceStats();
            cJSON* perf_stats = cJSON_CreateObject();
            if (perf_stats) {
                cJSON_AddNumberToObject(perf_stats, "fps_current", stats.fps_current);
                cJSON_AddNumberToObject(perf_stats, "fps_average", stats.fps_average);
                cJSON_AddNumberToObject(perf_stats, "frame_count", stats.frame_count);
                cJSON_AddNumberToObject(perf_stats, "memory_used", stats.memory_used);
                cJSON_AddItemToObject(player_status, "performance", perf_stats);
            }
            
            cJSON_AddItemToObject(response, "player", player_status);
        }
    }
    
    char* json_string = cJSON_Print(response);
    std::string result_str = json_string ? json_string : "{}";
    
    if (json_string) {
        free(json_string);
    }
    cJSON_Delete(response);
    
    return result_str;
}

/**
 * @brief 重连定时器回调
 */
void MqttMusicHandler::ReconnectTimerCallback(void* arg) {
    MqttMusicHandler* handler = static_cast<MqttMusicHandler*>(arg);
    if (handler) {
        handler->PerformReconnect();
    }
}

/**
 * @brief 心跳定时器回调
 */
void MqttMusicHandler::KeepaliveTimerCallback(void* arg) {
    MqttMusicHandler* handler = static_cast<MqttMusicHandler*>(arg);
    if (handler && handler->connected_.load()) {
        // 发送心跳消息
        std::string heartbeat = handler->GenerateStatusResponse("heartbeat", MQTT_MUSIC_OK);
        handler->PublishStatus(heartbeat.c_str());
    }
}

/**
 * @brief 清理资源
 */
void MqttMusicHandler::CleanupResources() {
    // 删除定时器
    if (reconnect_timer_) {
        esp_timer_stop(reconnect_timer_);
        esp_timer_delete(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }
    
    if (keepalive_timer_) {
        esp_timer_stop(keepalive_timer_);
        esp_timer_delete(keepalive_timer_);
        keepalive_timer_ = nullptr;
    }
    
    // 清理回调
    connection_callback_ = nullptr;
    message_callback_ = nullptr;
    
    ESP_LOGI(TAG, "MQTT music handler resources cleaned up");
}

// C接口实现
extern "C" {

/**
 * @brief 初始化MQTT音乐控制器（C接口）
 */
mqtt_music_error_t initMqttMusicHandler(const mqtt_music_config_t* config) {
    if (g_mqtt_music_handler) {
        ESP_LOGW(TAG, "MQTT music handler already initialized");
        return MQTT_MUSIC_ERR_ALREADY_INIT;
    }
    
    if (!g_music_player_instance) {
        ESP_LOGE(TAG, "Music player UI not initialized");
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    g_mqtt_music_handler = new MqttMusicHandler();
    if (!g_mqtt_music_handler) {
        ESP_LOGE(TAG, "Failed to allocate memory for MqttMusicHandler");
        return MQTT_MUSIC_ERR_INVALID_PARAM;
    }
    
    mqtt_music_error_t ret = g_mqtt_music_handler->Initialize(config, g_music_player_instance);
    if (ret != MQTT_MUSIC_OK) {
        delete g_mqtt_music_handler;
        g_mqtt_music_handler = nullptr;
    }
    
    return ret;
}

/**
 * @brief 销毁MQTT音乐控制器（C接口）
 */
mqtt_music_error_t destroyMqttMusicHandler(void) {
    if (!g_mqtt_music_handler) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    mqtt_music_error_t ret = g_mqtt_music_handler->Destroy();
    delete g_mqtt_music_handler;
    g_mqtt_music_handler = nullptr;
    
    return ret;
}

/**
 * @brief 连接MQTT（C接口）
 */
mqtt_music_error_t connectMqttMusic(void) {
    if (!g_mqtt_music_handler) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    return g_mqtt_music_handler->Connect();
}

/**
 * @brief 断开MQTT连接（C接口）
 */
mqtt_music_error_t disconnectMqttMusic(void) {
    if (!g_mqtt_music_handler) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    return g_mqtt_music_handler->Disconnect();
}

/**
 * @brief 发布音乐状态（C接口）
 */
mqtt_music_error_t publishMusicStatus(const char* status_json) {
    if (!g_mqtt_music_handler) {
        return MQTT_MUSIC_ERR_NOT_INIT;
    }
    
    return g_mqtt_music_handler->PublishStatus(status_json);
}

/**
 * @brief 获取默认MQTT配置（C接口）
 */
mqtt_music_config_t getDefaultMqttMusicConfig(const char* client_id) {
    mqtt_music_config_t config = {};
    
    // 基本MQTT配置
    config.mqtt_broker_uri = "mqtt://localhost:1883";
    config.client_id = client_id ? client_id : "xiaozhi_music_player";
    config.username = nullptr;
    config.password = nullptr;
    config.downlink_topic = "devices/xiaozhi/downlink";
    config.uplink_topic = "devices/xiaozhi/uplink";
    config.keepalive_interval_s = 60;
    config.message_timeout_ms = 5000;
    config.enable_ssl = false;
    config.enable_debug_log = false;
    
    // 重连配置
    config.reconnect.initial_retry_delay_ms = 1000;     // 1秒
    config.reconnect.max_retry_delay_ms = 30000;        // 30秒
    config.reconnect.retry_backoff_multiplier = 2;      // 指数退避倍数
    config.reconnect.max_retry_attempts = 10;           // 最大重试10次
    config.reconnect.enable_exponential_backoff = true; // 启用指数退避
    
    return config;
}

/**
 * @brief 设置MQTT连接回调（C接口）
 */
void setMqttConnectionCallback(void (*callback)(bool connected)) {
    g_connection_callback = callback;
    
    if (g_mqtt_music_handler && callback) {
        g_mqtt_music_handler->SetConnectionCallback([](bool connected) {
            if (g_connection_callback) {
                g_connection_callback(connected);
            }
        });
    }
}

} // extern "C"