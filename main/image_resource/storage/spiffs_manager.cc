#include "spiffs_manager.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <esp_partition.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "SpiffsManager"

namespace ImageResource {

esp_err_t SpiffsManager::Mount(const char* partition_label, 
                               const char* mount_point,
                               size_t max_files,
                               bool format_if_failed) {
    if (mounted_) {
        ESP_LOGW(TAG, "分区已挂载");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "准备挂载分区: %s -> %s", partition_label, mount_point);
    
    // 检查分区是否存在
    const esp_partition_t* target_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS, 
        partition_label);
    
    if (target_partition == NULL) {
        ESP_LOGE(TAG, "未找到分区: %s", partition_label);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "找到分区，大小: %lu字节 (%.1fMB)", 
             static_cast<unsigned long>(target_partition->size), 
             static_cast<double>(target_partition->size) / (1024.0 * 1024.0));
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = mount_point,
        .partition_label = partition_label,
        .max_files = max_files,
        .format_if_mount_failed = format_if_failed
    };
    
    ESP_LOGI(TAG, "开始挂载SPIFFS分区，如需格式化可能需要30-60秒...");
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    uint32_t end_time = esp_timer_get_time() / 1000;
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "挂载分区失败 (%s)", esp_err_to_name(ret));
        return ret;
    }
    
    uint32_t duration = end_time - start_time;
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取SPIFFS信息失败");
        return ret;
    }
    
    ESP_LOGI(TAG, "分区挂载成功! 耗时: %lums, 总大小: %lu字节, 已使用: %lu字节", 
             (unsigned long)duration, 
             static_cast<unsigned long>(total), 
             static_cast<unsigned long>(used));
    
    mounted_ = true;
    return ESP_OK;
}

esp_err_t SpiffsManager::Unmount(const char* mount_point) {
    if (!mounted_) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "卸载分区: %s", mount_point);
    
    for (int retry = 0; retry < 3; retry++) {
        esp_err_t ret = esp_vfs_spiffs_unregister(mount_point);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "分区卸载成功");
            mounted_ = false;
            return ESP_OK;
        }
        ESP_LOGW(TAG, "卸载失败 (重试%d/3): %s", retry + 1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGE(TAG, "卸载分区最终失败");
    return ESP_FAIL;
}

esp_err_t SpiffsManager::Format(const char* partition_label) {
    ESP_LOGW(TAG, "开始格式化分区: %s", partition_label);
    
    // 等待系统状态稳定
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    for (int retry = 0; retry < 3; retry++) {
        esp_err_t ret = esp_spiffs_format(partition_label);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "分区格式化成功");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "格式化失败 (重试%d/3): %s", retry + 1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGE(TAG, "格式化分区最终失败");
    return ESP_FAIL;
}

size_t SpiffsManager::GetFreeSpace(const char* partition_label) {
    size_t total = 0, used = 0;
    esp_err_t ret = esp_spiffs_info(partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取空间信息失败: %s", esp_err_to_name(ret));
        return 0;
    }
    return (total > used) ? (total - used) : 0;
}

bool SpiffsManager::OptimizeSpace(const char* partition_label) {
    ESP_LOGI(TAG, "开始优化SPIFFS空间碎片...");
    
    size_t total_before = 0, used_before = 0;
    esp_err_t ret = esp_spiffs_info(partition_label, &total_before, &used_before);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "无法获取优化前的空间信息");
        return false;
    }
    
    size_t free_before = (total_before > used_before) ? (total_before - used_before) : 0;
    ESP_LOGI(TAG, "优化前 - 总计: %lu字节, 已使用: %lu字节, 可用: %lu字节", 
             static_cast<unsigned long>(total_before), 
             static_cast<unsigned long>(used_before), 
             static_cast<unsigned long>(free_before));
    
    // 触发垃圾回收
    const char* gc_trigger = "/.gc_trigger.tmp";
    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", "/resources", gc_trigger);
    
    FILE* gc_file = fopen(full_path, "w");
    if (gc_file) {
        for (int i = 0; i < 100; i++) {
            fprintf(gc_file, "trigger_gc_%d\n", i);
        }
        fflush(gc_file);
        fsync(fileno(gc_file));
        fclose(gc_file);
        unlink(full_path);
        ESP_LOGI(TAG, "垃圾回收触发完成");
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // 多轮触发
    for (int round = 0; round < 3; round++) {
        snprintf(full_path, sizeof(full_path), "/resources/.gc_round_%d.tmp", round);
        FILE* round_file = fopen(full_path, "w");
        if (round_file) {
            for (int i = 0; i < 20; i++) {
                fprintf(round_file, "gc_round_%d_%d\n", round, i);
            }
            fflush(round_file);
            fclose(round_file);
            unlink(full_path);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    vTaskDelay(pdMS_TO_TICKS(300));
    
    size_t total_after = 0, used_after = 0;
    ret = esp_spiffs_info(partition_label, &total_after, &used_after);
    if (ret == ESP_OK) {
        size_t free_after = (total_after > used_after) ? (total_after - used_after) : 0;
        size_t space_gained = (free_after > free_before) ? (free_after - free_before) : 0;
        
        ESP_LOGI(TAG, "优化后 - 总计: %lu字节, 已使用: %lu字节, 可用: %lu字节", 
                 static_cast<unsigned long>(total_after), 
                 static_cast<unsigned long>(used_after), 
                 static_cast<unsigned long>(free_after));
        
        if (space_gained > 0) {
            ESP_LOGI(TAG, "空间优化成功，释放了 %lu 字节", static_cast<unsigned long>(space_gained));
        }
    }
    
    return true;
}

bool SpiffsManager::CreateDirectory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true; // 已存在
    }
    
    ESP_LOGI(TAG, "创建目录: %s", path);
    
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

} // namespace ImageResource
