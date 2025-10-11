#include "cleanup_helper.h"
#include <esp_log.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "CleanupHelper"

namespace ImageResource {

bool CleanupHelper::DeleteFiles(const std::vector<std::string>& files, ProgressCallback callback) {
    if (files.empty()) {
        ESP_LOGI(TAG, "无需删除文件");
        if (callback) {
            callback(100, 100, "无需删除");
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        return true;
    }
    
    ESP_LOGI(TAG, "开始删除 %zu 个文件", files.size());
    
    int deleted_count = 0;
    int failed_count = 0;
    
    for (size_t i = 0; i < files.size(); i++) {
        const std::string& filepath = files[i];
        
        int progress = static_cast<int>((i + 1) * 100 / files.size());
        if (callback) {
            char message[128];
            const char* filename = strrchr(filepath.c_str(), '/');
            filename = filename ? filename + 1 : filepath.c_str();
            snprintf(message, sizeof(message), "删除文件: %s (%zu/%zu)", 
                    filename, i + 1, files.size());
            callback(progress, 100, message);
        }
        
        if (remove(filepath.c_str()) == 0) {
            deleted_count++;
            ESP_LOGI(TAG, "成功删除: %s", filepath.c_str());
        } else {
            failed_count++;
            ESP_LOGW(TAG, "删除失败: %s", filepath.c_str());
        }
        
        if (i % 3 == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    ESP_LOGI(TAG, "删除完成: 成功 %d 个, 失败 %d 个", deleted_count, failed_count);
    
    if (callback) {
        char final_message[128];
        if (failed_count == 0) {
            snprintf(final_message, sizeof(final_message), "成功删除 %d 个文件", deleted_count);
        } else {
            snprintf(final_message, sizeof(final_message), "删除完成: 成功 %d, 失败 %d", deleted_count, failed_count);
        }
        callback(100, 100, final_message);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    
    return failed_count == 0;
}

int CleanupHelper::CleanupTemporary(const char* directory) {
    ESP_LOGI(TAG, "开始清理临时文件: %s", directory);
    
    int cleaned_count = 0;
    
    DIR* dir = opendir(directory);
    if (!dir) {
        ESP_LOGW(TAG, "无法打开目录: %s", directory);
        return 0;
    }
    
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        const char* name = ent->d_name;
        if (!name || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        
        std::string full_path = std::string(directory) + "/" + name;
        size_t len = strlen(name);
        bool should_delete = false;
        
        // 检查临时文件扩展名
        if (len > 4) {
            const char* ext = name + (len - 4);
            if (strcmp(ext, ".tmp") == 0 || strcmp(ext, ".bak") == 0 || 
                strcmp(ext, ".old") == 0 || strstr(name, ".temp") != NULL ||
                strstr(name, "~") != NULL) {
                should_delete = true;
                ESP_LOGI(TAG, "清理临时文件: %s", name);
            }
        }
        
        // 检查损坏的缓存文件（大小为0）
        if (strstr(name, ".json") != NULL) {
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0 && st.st_size == 0) {
                should_delete = true;
                ESP_LOGI(TAG, "清理空缓存文件: %s", name);
            }
        }
        
        if (should_delete) {
            if (unlink(full_path.c_str()) == 0) {
                cleaned_count++;
                ESP_LOGI(TAG, "成功删除: %s", name);
            } else {
                ESP_LOGW(TAG, "删除失败: %s", name);
            }
        }
    }
    closedir(dir);
    
    // 清理已知的临时文件
    const char* known_temps[] = {
        "/resources/temp_packed.rgb",
        "/resources/downloading.tmp",
        "/resources/images/packed_part_0.rgb",
        "/resources/images/packed_part_1.rgb",
        "/resources/images/packed_part_2.rgb",
        "/resources/images/packed_part_3.rgb"
    };
    
    for (size_t i = 0; i < sizeof(known_temps) / sizeof(known_temps[0]); i++) {
        if (unlink(known_temps[i]) == 0) {
            cleaned_count++;
            ESP_LOGI(TAG, "清理已知临时文件: %s", known_temps[i]);
        }
    }
    
    ESP_LOGI(TAG, "临时文件清理完成，共清理 %d 个文件", cleaned_count);
    return cleaned_count;
}

bool CleanupHelper::ClearAllImages(const char* base_path, int max_files) {
    ESP_LOGI(TAG, "开始清理所有图片文件...");
    int deleted_count = 0;
    
    for (int i = 1; i <= max_files; i++) {
        char bin_filepath[128];
        char h_filepath[128];
        snprintf(bin_filepath, sizeof(bin_filepath), "%soutput_%04d.bin", base_path, i);
        snprintf(h_filepath, sizeof(h_filepath), "%soutput_%04d.h", base_path, i);
        
        if (remove(bin_filepath) == 0) {
            ESP_LOGI(TAG, "删除: %s", bin_filepath);
            deleted_count++;
        }
        
        if (remove(h_filepath) == 0) {
            ESP_LOGI(TAG, "删除: %s", h_filepath);
            deleted_count++;
        }
    }
    
    ESP_LOGI(TAG, "图片文件清理完成，共删除 %d 个文件", deleted_count);
    return true;
}

bool CleanupHelper::DeleteAnimationFiles(const char* base_path, int max_files, ProgressCallback callback) {
    ESP_LOGI(TAG, "开始删除动画图片文件...");
    
    // 删除打包文件
    const char* packed_file = "/resources/images/packed.rgb";
    if (unlink(packed_file) == 0) {
        ESP_LOGI(TAG, "已删除打包文件: %s", packed_file);
    }
    
    if (callback) {
        callback(0, 100, "正在删除旧的动画图片文件...");
    }
    
    // 扫描存在的文件
    std::vector<std::string> existing_files;
    for (int i = 1; i <= max_files; i++) {
        char bin_filepath[128];
        char h_filepath[128];
        snprintf(bin_filepath, sizeof(bin_filepath), "%soutput_%04d.bin", base_path, i);
        snprintf(h_filepath, sizeof(h_filepath), "%soutput_%04d.h", base_path, i);
        
        struct stat file_stat;
        if (stat(bin_filepath, &file_stat) == 0) {
            existing_files.push_back(bin_filepath);
        }
        if (stat(h_filepath, &file_stat) == 0) {
            existing_files.push_back(h_filepath);
        }
    }
    
    if (existing_files.empty()) {
        ESP_LOGI(TAG, "未发现需要删除的动画图片文件");
        if (callback) {
            callback(100, 100, "无需删除，准备下载新文件...");
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        return true;
    }
    
    return DeleteFiles(existing_files, callback);
}

bool CleanupHelper::DeleteLogoFile(const char* logo_bin_path, const char* logo_h_path, ProgressCallback callback) {
    ESP_LOGI(TAG, "开始删除logo文件...");
    
    if (callback) {
        callback(0, 100, "正在删除旧的logo文件...");
    }
    
    struct stat file_stat;
    bool bin_exists = (stat(logo_bin_path, &file_stat) == 0);
    bool h_exists = (stat(logo_h_path, &file_stat) == 0);
    
    if (!bin_exists && !h_exists) {
        ESP_LOGI(TAG, "未发现需要删除的logo文件");
        if (callback) {
            callback(100, 100, "无需删除logo，准备下载新文件...");
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        return true;
    }
    
    bool success = true;
    int deleted_count = 0;
    
    if (bin_exists) {
        if (callback) {
            callback(25, 100, "正在删除logo.bin文件...");
        }
        
        if (remove(logo_bin_path) == 0) {
            ESP_LOGI(TAG, "成功删除: %s", logo_bin_path);
            deleted_count++;
        } else {
            ESP_LOGW(TAG, "删除失败: %s", logo_bin_path);
            success = false;
        }
    }
    
    if (h_exists) {
        if (callback) {
            callback(75, 100, "正在删除logo.h文件...");
        }
        
        if (remove(logo_h_path) == 0) {
            ESP_LOGI(TAG, "成功删除: %s", logo_h_path);
            deleted_count++;
        } else {
            ESP_LOGW(TAG, "删除失败: %s", logo_h_path);
            success = false;
        }
    }
    
    if (callback) {
        char result_message[128];
        if (success && deleted_count > 0) {
            snprintf(result_message, sizeof(result_message), "成功删除 %d 个logo文件", deleted_count);
        } else if (deleted_count > 0) {
            snprintf(result_message, sizeof(result_message), "部分logo文件删除失败");
        } else {
            snprintf(result_message, sizeof(result_message), "logo文件删除失败");
        }
        callback(100, 100, result_message);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "logo文件删除完成，成功删除: %d 个文件", deleted_count);
    return success;
}

} // namespace ImageResource
