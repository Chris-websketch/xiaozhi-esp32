#include "preload_manager.h"
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config/resource_config.h"
#include "application.h"
#include "memory/memory_manager.h"

#define TAG "PreloadManager"

namespace ImageResource {

PreloadManager::PreloadManager(const ResourceConfig* config) 
    : config_(config) {
}

esp_err_t PreloadManager::PreloadRemaining(LoadCallback load_cb,
                                          CheckCallback check_cb,
                                          int total_images,
                                          ProgressCallback progress_cb) {
    return PreloadImpl(false, load_cb, check_cb, total_images, 0, progress_cb);
}

esp_err_t PreloadManager::PreloadSilent(LoadCallback load_cb,
                                       CheckCallback check_cb,
                                       int total_images,
                                       unsigned long time_budget_ms) {
    return PreloadImpl(true, load_cb, check_cb, total_images, time_budget_ms, nullptr);
}

void PreloadManager::Cancel() {
    cancel_preload_.store(true);
}

bool PreloadManager::IsPreloading() const {
    return is_preloading_.load();
}

bool PreloadManager::WaitForFinish(unsigned long timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    TickType_t budget = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    
    while (IsPreloading()) {
        vTaskDelay(pdMS_TO_TICKS(5));
        if (budget != 0 && (xTaskGetTickCount() - start) >= budget) {
            return false;
        }
    }
    return true;
}

esp_err_t PreloadManager::PreloadImpl(bool silent,
                                     LoadCallback load_cb,
                                     CheckCallback check_cb,
                                     int total_images,
                                     unsigned long time_budget_ms,
                                     ProgressCallback progress_cb) {
    if (is_preloading_.load()) {
        return ESP_ERR_INVALID_STATE;
    }
    
    is_preloading_.store(true);
    cancel_preload_.store(false);
    
    size_t free_heap = esp_get_free_heap_size();
    if (!silent) {
        ESP_LOGI(TAG, "开始预加载，可用内存: %u字节", (unsigned int)free_heap);
    }
    
    if (free_heap < config_->memory.preload_threshold) {
        if (!silent) {
            ESP_LOGW(TAG, "内存不足，跳过预加载");
        }
        is_preloading_.store(false);
        return ESP_ERR_NO_MEM;
    }
    
    auto& app = Application::GetInstance();
    
    int loaded_count = 0;
    TickType_t start_ticks = xTaskGetTickCount();
    TickType_t budget_ticks = (time_budget_ms == 0) ? 0 : pdMS_TO_TICKS(time_budget_ms);
    
    if (!silent && progress_cb) {
        progress_cb(0, total_images, "准备预加载图片资源...");
    }
    
    for (int i = 1; i <= total_images; i++) {
        // 取消检查
        if (cancel_preload_.load()) {
            if (!silent && progress_cb) {
                progress_cb(loaded_count, total_images, "预加载取消");
                vTaskDelay(pdMS_TO_TICKS(200));
                progress_cb(loaded_count, total_images, nullptr);
            }
            is_preloading_.store(false);
            return ESP_ERR_INVALID_STATE;
        }
        
        // 时间预算检查
        if (budget_ticks != 0 && (xTaskGetTickCount() - start_ticks) >= budget_ticks) {
            if (!silent && progress_cb) {
                progress_cb(loaded_count, total_images, "预加载时间用尽");
                vTaskDelay(pdMS_TO_TICKS(200));
                progress_cb(loaded_count, total_images, nullptr);
            }
            is_preloading_.store(false);
            return (loaded_count > 0) ? ESP_OK : ESP_FAIL;
        }
        
        // 已加载检查
        if (check_cb(i)) {
            loaded_count++;
            if (!silent && progress_cb) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 已加载，跳过...", i);
                progress_cb(loaded_count, total_images, message);
            }
            continue;
        }
        
        // 音频状态检查（静默模式跳过）
        if (!silent) {
            if (i % 3 == 0) {
                if (!app.IsAudioQueueEmpty() || app.GetDeviceState() != kDeviceStateIdle) {
                    ESP_LOGW(TAG, "检测到音频活动，暂停预加载");
                    if (progress_cb) {
                        progress_cb(loaded_count, total_images, "预加载中断：检测到音频活动");
                        vTaskDelay(pdMS_TO_TICKS(200));
                        progress_cb(loaded_count, total_images, nullptr);
                    }
                    is_preloading_.store(false);
                    return (loaded_count > 0) ? ESP_OK : ESP_FAIL;
                }
            }
        }
        
        // 内存检查
        free_heap = esp_get_free_heap_size();
        if (free_heap < 200000) {
            if (!silent) {
                ESP_LOGW(TAG, "预加载过程中内存不足");
            }
            if (!silent && progress_cb) {
                progress_cb(loaded_count, total_images, "预加载停止：内存不足");
                vTaskDelay(pdMS_TO_TICKS(200));
                progress_cb(loaded_count, total_images, nullptr);
            }
            is_preloading_.store(false);
            return (loaded_count > 0) ? ESP_OK : ESP_ERR_NO_MEM;
        }
        
        // 开始加载
        if (!silent && progress_cb) {
            char message[64];
            snprintf(message, sizeof(message), "正在预加载图片 %d/%d", i, total_images);
            progress_cb(loaded_count, total_images, message);
        }
        
        if (!silent) {
            ESP_LOGI(TAG, "预加载图片 %d/%d...", i, total_images);
        }
        
        if (load_cb(i)) {
            loaded_count++;
            if (!silent) {
                ESP_LOGI(TAG, "预加载图片 %d 成功", i);
            }
            if (!silent && progress_cb) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 预加载完成", i);
                progress_cb(loaded_count, total_images, message);
            }
        } else {
            if (!silent) {
                ESP_LOGE(TAG, "预加载图片 %d 失败", i);
            }
            if (!silent && progress_cb) {
                char message[64];
                snprintf(message, sizeof(message), "图片 %d 预加载失败，继续下一张", i);
                progress_cb(loaded_count, total_images, message);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(config_->preload.load_delay_ms));
    }
    
    free_heap = esp_get_free_heap_size();
    if (!silent) {
        ESP_LOGI(TAG, "预加载完成，成功: %d/%d，剩余内存: %u字节", 
                loaded_count, total_images, (unsigned int)free_heap);
    }
    
    if (!silent && progress_cb) {
        char message[64];
        if (loaded_count == total_images) {
            snprintf(message, sizeof(message), "所有图片预加载完成！");
        } else {
            snprintf(message, sizeof(message), "预加载完成：%d/%d 张图片", loaded_count, total_images);
        }
        progress_cb(loaded_count, total_images, message);
        vTaskDelay(pdMS_TO_TICKS(200));
        progress_cb(loaded_count, total_images, nullptr);
    }
    
    is_preloading_.store(false);
    return loaded_count > 0 ? ESP_OK : ESP_FAIL;
}

} // namespace ImageResource
