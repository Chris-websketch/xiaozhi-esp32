#include "downloader.h"
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_station.h>
#include <board.h>
#include <system_info.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include "config/resource_config.h"
#include <stdio.h>
#include <string.h>

#define TAG "Downloader"
#define DEBUG_IMAGE_FILES 0

namespace ImageResource {

Downloader::Downloader(const ResourceConfig* config) 
    : config_(config), progress_callback_(nullptr) {
}

esp_err_t Downloader::DownloadFile(const char* url, const char* filepath, 
                                   int file_index, int total_files) {
    int retry_count = 0;
    static int last_logged_percent = -1;
    
    size_t free_heap = esp_get_free_heap_size();
    if (free_heap < 100000) {
        ESP_LOGE(TAG, "内存不足，可用: %u字节", (unsigned int)free_heap);
        if (progress_callback_) {
            progress_callback_(0, 100, "内存不足，下载失败");
        }
        return ESP_ERR_NO_MEM;
    }
    
    while (retry_count < config_->network.retry_count) {
        last_logged_percent = -1;
        
        if (!WifiStation::GetInstance().IsConnected()) {
            ESP_LOGE(TAG, "WiFi连接已断开");
            if (progress_callback_) {
                progress_callback_(0, 100, "网络连接已断开，等待重连...");
            }
            vTaskDelay(pdMS_TO_TICKS(config_->download_mode.network_stabilize_ms));
            retry_count++;
            continue;
        }
        
        // 注释掉初始消息设置，保持调用者设置的初始消息（如"正在下载动图"）
        // 只在重试时更新消息
        if (progress_callback_ && retry_count > 0) {
            char message[128];
            const char* filename = strrchr(filepath, '/');
            filename = filename ? filename + 1 : filepath;
            snprintf(message, sizeof(message), "重试下载: %s (%d/%lu)",
                    filename, retry_count + 1, config_->network.retry_count);
            progress_callback_(0, 100, message);
        }
        
        auto http = Board::GetInstance().CreateHttp();
        if (!http) {
            ESP_LOGE(TAG, "无法创建HTTP客户端");
            retry_count++;
            if (progress_callback_) {
                progress_callback_(0, 100, "HTTP客户端创建失败");
            }
            vTaskDelay(pdMS_TO_TICKS(1000 * retry_count));
            continue;
        }
        
        std::string device_id = SystemInfo::GetMacAddress();
        std::string client_id = SystemInfo::GetClientId();
        
        if (!device_id.empty()) {
            http->SetHeader("Device-Id", device_id.c_str());
        }
        if (!client_id.empty()) {
            http->SetHeader("Client-Id", client_id.c_str());
        }
        
        auto app_desc = esp_app_get_description();
        http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
        http->SetHeader("Connection", "keep-alive");
        http->SetHeader("Accept-Encoding", "identity");
        http->SetHeader("Cache-Control", "no-cache");
        http->SetHeader("Accept", "*/*");
        
        if (!http->Open("GET", url)) {
            ESP_LOGE(TAG, "无法连接: %s", url);
            delete http;
            retry_count++;
            
            if (progress_callback_) {
                char message[128];
                snprintf(message, sizeof(message), "连接失败，正在重试(%d/%lu)",
                        retry_count, config_->network.retry_count);
                progress_callback_(0, 100, message);
            }
            
            vTaskDelay(pdMS_TO_TICKS(config_->network.retry_delay_ms));
            if (retry_count >= 2) {
                vTaskDelay(pdMS_TO_TICKS(config_->network.retry_delay_ms * retry_count / 2));
            }
            continue;
        }
        
        const char* file_ext = strrchr(filepath, '.');
        const char* mode = (file_ext && strcmp(file_ext, ".bin") == 0) ? "wb" : "w";
        
        FILE* f = fopen(filepath, mode);
        if (f == NULL) {
            ESP_LOGE(TAG, "无法创建文件: %s", filepath);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "创建文件失败");
            }
            return ESP_ERR_NO_MEM;
        }
        
        size_t content_length = http->GetBodyLength();
        if (content_length == 0) {
            ESP_LOGE(TAG, "无法获取文件大小");
            fclose(f);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "无法获取文件大小");
            }
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "下载文件大小: %u字节", (unsigned int)content_length);
        
        size_t buffer_size = config_->network.buffer_size;
        size_t current_free_heap = esp_get_free_heap_size();
        if (current_free_heap < config_->memory.allocation_threshold) {
            buffer_size = config_->network.buffer_size / 4;
        } else if (current_free_heap < config_->memory.download_threshold) {
            buffer_size = config_->network.buffer_size / 2;
        }
        
        char* buffer = (char*)malloc(buffer_size);
        if (!buffer) {
            ESP_LOGE(TAG, "无法分配%zu字节缓冲区", buffer_size);
            fclose(f);
            http->Close();
            delete http;
            if (progress_callback_) {
                progress_callback_(0, 100, "内存分配失败");
            }
            return ESP_ERR_NO_MEM;
        }
        
        size_t total_read = 0;
        bool download_success = true;
        
        while (true) {
            if (total_read % config_->memory.download_threshold == 0) {
                free_heap = esp_get_free_heap_size();
                if (free_heap < config_->memory.allocation_threshold) {
                    ESP_LOGW(TAG, "内存不足，中止下载");
                    download_success = false;
                    break;
                }
                
                if (!WifiStation::GetInstance().IsConnected()) {
                    ESP_LOGE(TAG, "WiFi断开，中止下载");
                    download_success = false;
                    break;
                }
            }
            
            int ret = http->Read(buffer, buffer_size);
            if (ret < 0) {
                ESP_LOGE(TAG, "读取HTTP数据失败: %d", ret);
                download_success = false;
                break;
            }
            
            if (ret == 0) {
                if (total_read < content_length) {
                    ESP_LOGW(TAG, "下载未完成: %zu/%zu字节", total_read, content_length);
                    download_success = false;
                } else {
                    ESP_LOGI(TAG, "下载完成: %zu字节", total_read);
                }
                break;
            }
            
            size_t written = fwrite(buffer, 1, ret, f);
            if (written != ret) {
                ESP_LOGE(TAG, "写入失败: 期望%d，实际%zu", ret, written);
                download_success = false;
                break;
            }
            
            total_read += ret;
            
            if (content_length > 0) {
                int file_percent = (float)total_read * 100 / content_length;
                int total_percent = 0;
                
                if (total_files > 1) {
                    total_percent = (file_index * 100 + file_percent) / total_files;
                } else {
                    total_percent = file_percent;
                }
                
                if (total_percent != last_logged_percent && total_percent % 2 == 0) {
                    if (progress_callback_) {
                        // 不论单文件还是批量，都不生成消息，保持调用者设置的初始消息（如"正在下载动图"/"正在下载静图"）
                        progress_callback_(total_percent, 100, nullptr);
                    }
                    
                    if (file_percent % 25 == 0 || file_percent == 100) {
                        ESP_LOGI(TAG, "下载进度: 文件%d/%d %d%%, 总体%d%%", 
                                file_index + 1, total_files, file_percent, total_percent);
                    }
                    
                    last_logged_percent = total_percent;
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(config_->download_mode.network_stabilize_ms / 100));
        }
        
        free(buffer);
        fclose(f);
        http->Close();
        delete http;
        
        vTaskDelay(pdMS_TO_TICKS(config_->download_mode.gc_interval_ms / 25));
        
        if (download_success) {
            if (progress_callback_) {
                int total_percent = (total_files > 1) ? 
                    ((file_index + 1) * 100) / total_files : 100;
                
                // 不论单文件还是批量，都不生成消息，保持调用者设置的初始消息
                // 让调用者自己决定何时显示完成消息
                progress_callback_(total_percent, 100, nullptr);
            }
            
            ESP_LOGI(TAG, "文件下载完成");
            return ESP_OK;
        } else {
            retry_count++;
            remove(filepath);
            
            if (progress_callback_) {
                char message[128];
                const char* filename = strrchr(filepath, '/');
                filename = filename ? filename + 1 : filepath;
                snprintf(message, sizeof(message), "下载 %s 失败，准备重试 (%d/%lu)", 
                        filename, retry_count, config_->network.retry_count);
                progress_callback_(0, 100, message);
            }
            
            vTaskDelay(pdMS_TO_TICKS(config_->network.retry_delay_ms * retry_count));
        }
    }
    
    const char* filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;
    ESP_LOGE(TAG, "文件 %s 下载失败，重试次数已达上限", filename);
    if (progress_callback_) {
        char message[128];
        snprintf(message, sizeof(message), "下载 %s 失败", filename);
        progress_callback_(0, 100, message);
    }
    
    return ESP_FAIL;
}

esp_err_t Downloader::DownloadBatch(const std::vector<std::string>& urls,
                                    const std::vector<std::string>& filepaths) {
    if (urls.size() != filepaths.size()) {
        ESP_LOGE(TAG, "URL和文件路径数量不匹配");
        return ESP_ERR_INVALID_ARG;
    }
    
    int failed_count = 0;
    for (size_t i = 0; i < urls.size(); i++) {
        esp_err_t result = DownloadFile(urls[i].c_str(), filepaths[i].c_str(), 
                                       (int)i, (int)urls.size());
        if (result != ESP_OK) {
            failed_count++;
            if (failed_count > 3) {
                ESP_LOGE(TAG, "连续失败过多，停止下载");
                return ESP_FAIL;
            }
        } else {
            failed_count = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(config_->network.connection_delay_ms));
    }
    
    return (failed_count < urls.size() / 2) ? ESP_OK : ESP_FAIL;
}

} // namespace ImageResource
