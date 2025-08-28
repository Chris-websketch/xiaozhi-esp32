#include "resource_config.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <cJSON.h>
#include <fstream>

#define TAG "ConfigManager"

namespace ImageResource {

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::load_config(const std::string& config_path) {
    // 如果没有指定路径或文件不存在，使用默认配置
    if (config_path.empty()) {
        ESP_LOGI(TAG, "使用默认配置");
        reset_to_defaults();
        adjust_for_device();
        return true;
    }
    
    // 尝试从文件加载配置
    std::ifstream file(config_path);
    if (!file.is_open()) {
        ESP_LOGW(TAG, "无法打开配置文件: %s，使用默认配置", config_path.c_str());
        reset_to_defaults();
        adjust_for_device();
        return false;
    }
    
    // 读取文件内容
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // 解析JSON
    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) {
        ESP_LOGE(TAG, "配置文件JSON解析失败");
        reset_to_defaults();
        adjust_for_device();
        return false;
    }
    
    // 解析各个配置节
    cJSON* network = cJSON_GetObjectItem(root, "network");
    if (network) {
        cJSON* item;
        if ((item = cJSON_GetObjectItem(network, "timeout_ms"))) {
            config_.network.timeout_ms = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(network, "retry_count"))) {
            config_.network.retry_count = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(network, "buffer_size"))) {
            config_.network.buffer_size = item->valueint;
        }
    }
    
    // 类似地解析其他配置节...
    // （为简化代码，这里省略了其他配置的解析）
    
    cJSON_Delete(root);
    
    // 验证配置
    if (!validate_config()) {
        ESP_LOGW(TAG, "配置验证失败，使用默认配置");
        reset_to_defaults();
        adjust_for_device();
        return false;
    }
    
    ESP_LOGI(TAG, "配置加载成功");
    return true;
}

bool ConfigManager::save_config(const std::string& config_path) const {
    if (config_path.empty()) {
        return false;
    }
    
    // 创建JSON对象
    cJSON* root = cJSON_CreateObject();
    
    // 网络配置
    cJSON* network = cJSON_CreateObject();
    cJSON_AddNumberToObject(network, "timeout_ms", config_.network.timeout_ms);
    cJSON_AddNumberToObject(network, "retry_count", config_.network.retry_count);
    cJSON_AddNumberToObject(network, "buffer_size", config_.network.buffer_size);
    cJSON_AddItemToObject(root, "network", network);
    
    // 内存配置
    cJSON* memory = cJSON_CreateObject();
    cJSON_AddNumberToObject(memory, "allocation_threshold", config_.memory.allocation_threshold);
    cJSON_AddNumberToObject(memory, "download_threshold", config_.memory.download_threshold);
    cJSON_AddItemToObject(root, "memory", memory);
    
    // 生成JSON字符串
    char* json_string = cJSON_Print(root);
    
    // 写入文件
    std::ofstream file(config_path);
    bool success = false;
    if (file.is_open()) {
        file << json_string;
        file.close();
        success = true;
        ESP_LOGI(TAG, "配置保存成功: %s", config_path.c_str());
    } else {
        ESP_LOGE(TAG, "无法写入配置文件: %s", config_path.c_str());
    }
    
    free(json_string);
    cJSON_Delete(root);
    
    return success;
}

bool ConfigManager::validate_config() const {
    // 验证网络配置
    if (config_.network.timeout_ms < 1000 || config_.network.timeout_ms > 120000) {
        ESP_LOGE(TAG, "网络超时时间无效: %" PRIu32, config_.network.timeout_ms);
        return false;
    }
    
    if (config_.network.retry_count > 10) {
        ESP_LOGE(TAG, "重试次数过多: %" PRIu32, config_.network.retry_count);
        return false;
    }
    
    if (config_.network.buffer_size < 1024 || config_.network.buffer_size > 64 * 1024) {
        ESP_LOGE(TAG, "缓冲区大小无效: %" PRIu32, config_.network.buffer_size);
        return false;
    }
    
    // 验证内存配置
    if (config_.memory.allocation_threshold < 50 * 1024) {
        ESP_LOGE(TAG, "内存阈值过低: %" PRIu32, config_.memory.allocation_threshold);
        return false;
    }
    
    // 验证图片配置
    if (config_.image.max_image_count == 0 || config_.image.max_image_count > 20) {
        ESP_LOGE(TAG, "图片数量无效: %" PRIu32, config_.image.max_image_count);
        return false;
    }
    
    uint32_t image_size = config_.get_image_size();
    if (image_size == 0 || image_size > 1024 * 1024) {
        ESP_LOGE(TAG, "图片尺寸无效: %" PRIu32, image_size);
        return false;
    }
    
    ESP_LOGI(TAG, "配置验证通过");
    return true;
}

void ConfigManager::reset_to_defaults() {
    config_ = ResourceConfig{}; // 使用默认构造
    ESP_LOGI(TAG, "已重置为默认配置");
}

void ConfigManager::adjust_for_device() {
    // 根据可用内存调整配置
    size_t free_heap = esp_get_free_heap_size();
    
    ESP_LOGI(TAG, "根据设备调整配置，当前可用内存: %u bytes", (unsigned int)free_heap);
    
    if (free_heap < 1024 * 1024) { // 小于1MB
        // 内存受限设备
        config_.network.buffer_size = 4096;
        config_.memory.allocation_threshold = 100 * 1024;
        config_.memory.download_threshold = 200 * 1024;
        config_.memory.preload_threshold = 300 * 1024;
        config_.memory.buffer_pool_size = 5;
        config_.preload.check_interval = 2; // 更频繁的检查
        
        ESP_LOGI(TAG, "检测到内存受限设备，已调整配置");
    } else if (free_heap > 4 * 1024 * 1024) { // 大于4MB
        // 内存充足设备
        config_.network.buffer_size = 16384;
        config_.memory.buffer_pool_size = 15;
        config_.preload.check_interval = 5; // 较少的检查
        
        ESP_LOGI(TAG, "检测到内存充足设备，已优化配置");
    }
    
    // 根据CPU频率调整
    // （ESP32的CPU频率信息获取较复杂，这里简化处理）
    
    ESP_LOGI(TAG, "设备配置调整完成:");
    ESP_LOGI(TAG, "  网络缓冲区: %" PRIu32 " bytes", config_.network.buffer_size);
    ESP_LOGI(TAG, "  内存阈值: %" PRIu32 " bytes", config_.memory.allocation_threshold);
    ESP_LOGI(TAG, "  缓冲区池大小: %" PRIu32, config_.memory.buffer_pool_size);
}

} // namespace ImageResource
