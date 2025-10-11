#include "packed_loader.h"
#include <esp_log.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "config/resource_config.h"
#include "storage/spiffs_manager.h"

#define TAG "PackedLoader"

namespace ImageResource {

PackedLoader::PackedLoader(const ResourceConfig* config) 
    : config_(config) {
}

bool PackedLoader::BuildPacked(const std::vector<std::string>& source_files,
                              const char* packed_file,
                              size_t frame_size,
                              ProgressCallback callback) {
    ESP_LOGI(TAG, "开始构建打包文件: %s", packed_file);
    
    // 检查源文件
    for (const auto& file : source_files) {
        struct stat st;
        if (stat(file.c_str(), &st) != 0) {
            ESP_LOGE(TAG, "源文件不存在: %s", file.c_str());
            return false;
        }
        if ((size_t)st.st_size != frame_size) {
            ESP_LOGE(TAG, "文件大小不匹配: %s (size=%ld, expect=%zu)", 
                    file.c_str(), (long)st.st_size, frame_size);
            return false;
        }
    }
    
    // 删除旧文件
    unlink(packed_file);
    
    const size_t chunk = 4 * 1024;  // 4KB分块
    const size_t total_bytes = frame_size * source_files.size();
    size_t processed_bytes = 0;
    int last_percent = -1;
    
    if (callback) {
        callback(0, 100, "正在检查资源完整性...");
    }
    
    uint8_t* buf = (uint8_t*)malloc(chunk);
    if (!buf) {
        ESP_LOGE(TAG, "分配缓冲区失败");
        return false;
    }
    
    FILE* out = fopen(packed_file, "wb");
    if (!out) {
        ESP_LOGE(TAG, "无法创建输出文件: %s (errno=%d)", packed_file, errno);
        free(buf);
        return false;
    }
    setvbuf(out, NULL, _IOFBF, 32768);
    
    for (size_t i = 0; i < source_files.size(); i++) {
        FILE* in = fopen(source_files[i].c_str(), "rb");
        if (!in) {
            ESP_LOGE(TAG, "无法打开源文件: %s", source_files[i].c_str());
            free(buf);
            fclose(out);
            unlink(packed_file);
            return false;
        }
        setvbuf(in, NULL, _IOFBF, 32768);
        
        size_t remaining = frame_size;
        while (remaining > 0) {
            size_t to_read = remaining > chunk ? chunk : remaining;
            size_t r = fread(buf, 1, to_read, in);
            if (r == 0 && remaining > 0) break;
            
            // 写入重试机制
            size_t w = 0;
            for (int retry = 0; retry < 3; ++retry) {
                w = fwrite(buf, 1, r, out);
                if (w == r) break;
                
                ESP_LOGW(TAG, "第%zu帧写入重试%d次", i + 1, retry + 1);
                
                if (errno == 28) {  // ENOSPC
                    ESP_LOGE(TAG, "磁盘空间不足");
                    fclose(in);
                    free(buf);
                    fclose(out);
                    unlink(packed_file);
                    return false;
                }
                
                fflush(out);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            
            if (w != r) {
                ESP_LOGE(TAG, "第%zu帧写入失败", i + 1);
                fclose(in);
                free(buf);
                fclose(out);
                unlink(packed_file);
                return false;
            }
            
            remaining -= r;
            
            // 更新进度
            if (callback) {
                processed_bytes += w;
                int percent = (int)((processed_bytes * 100) / total_bytes);
                if (percent > 100) percent = 100;
                if (percent != last_percent) {
                    callback(percent, 100, "正在检查资源完整性");
                    last_percent = percent;
                }
            }
        }
        
        fclose(in);
        ESP_LOGI(TAG, "第%zu帧写入完成", i + 1);
    }
    
    free(buf);
    fflush(out);
    fclose(out);
    
    ESP_LOGI(TAG, "已构建打包文件: %s", packed_file);
    
    if (callback) {
        callback(100, 100, "检查完成");
        vTaskDelay(pdMS_TO_TICKS(500));
        callback(100, 100, nullptr);
    }
    
    return true;
}

bool PackedLoader::LoadPacked(const char* packed_file,
                             size_t frame_size,
                             int frame_count,
                             std::vector<uint8_t*>& out_buffers) {
    ESP_LOGI(TAG, "从打包文件加载: %s", packed_file);
    
    FILE* f = fopen(packed_file, "rb");
    if (!f) {
        ESP_LOGW(TAG, "无法打开打包文件");
        return false;
    }
    setvbuf(f, NULL, _IOFBF, 65536);
    
    out_buffers.clear();
    out_buffers.resize(frame_count, nullptr);
    
    for (int i = 0; i < frame_count; ++i) {
        uint8_t* buf = (uint8_t*)malloc(frame_size);
        if (!buf) {
            ESP_LOGE(TAG, "内存分配失败");
            // 释放已分配的内存
            for (int j = 0; j < i; ++j) {
                if (out_buffers[j]) free(out_buffers[j]);
            }
            out_buffers.clear();
            fclose(f);
            return false;
        }
        
        size_t read_size = fread(buf, 1, frame_size, f);
        if (read_size != frame_size) {
            ESP_LOGE(TAG, "读取第%d帧失败: %zu/%zu字节", i + 1, read_size, frame_size);
            free(buf);
            // 释放已分配的内存
            for (int j = 0; j < i; ++j) {
                if (out_buffers[j]) free(out_buffers[j]);
            }
            out_buffers.clear();
            fclose(f);
            return false;
        }
        
        out_buffers[i] = buf;
    }
    
    fclose(f);
    ESP_LOGI(TAG, "成功加载 %d 帧", frame_count);
    return true;
}

} // namespace ImageResource
