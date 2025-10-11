#include "cache_manager.h"
#include <esp_log.h>
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

#define TAG "CacheManager"

namespace ImageResource {

std::vector<std::string> CacheManager::ReadDynamicUrls(const char* cache_file) {
    std::vector<std::string> urls;
    
    FILE* f = fopen(cache_file, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开动态URL缓存文件: %s", cache_file);
        return urls;
    }
    
    char buffer[1024];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return urls;
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析动态URL缓存失败");
        return urls;
    }
    
    cJSON* dyn_array = cJSON_GetObjectItem(root, "dyn");
    if (dyn_array != NULL && cJSON_IsArray(dyn_array)) {
        int array_size = cJSON_GetArraySize(dyn_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* url_item = cJSON_GetArrayItem(dyn_array, i);
            if (cJSON_IsString(url_item)) {
                urls.push_back(url_item->valuestring);
            }
        }
        ESP_LOGI(TAG, "成功读取 %d 个动态URL", array_size);
    }
    
    cJSON_Delete(root);
    return urls;
}

std::string CacheManager::ReadStaticUrl(const char* cache_file) {
    std::string url;
    
    FILE* f = fopen(cache_file, "r");
    if (f == NULL) {
        ESP_LOGW(TAG, "无法打开静态URL缓存文件: %s", cache_file);
        return url;
    }
    
    char buffer[512];
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);
    
    if (len <= 0) {
        return url;
    }
    
    buffer[len] = '\0';
    
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "解析静态URL缓存失败");
        return url;
    }
    
    cJSON* sta_url = cJSON_GetObjectItem(root, "sta");
    if (sta_url != NULL && cJSON_IsString(sta_url) && strlen(sta_url->valuestring) > 0) {
        url = sta_url->valuestring;
        ESP_LOGI(TAG, "成功读取静态URL: %s", url.c_str());
    }
    
    cJSON_Delete(root);
    return url;
}

bool CacheManager::SaveDynamicUrls(const std::vector<std::string>& urls, const char* cache_file) {
    FILE* f = fopen(cache_file, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建动态URL缓存文件: %s", cache_file);
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON* dyn_array = cJSON_CreateArray();
    
    for (const auto& url : urls) {
        cJSON_AddItemToArray(dyn_array, cJSON_CreateString(url.c_str()));
    }
    
    cJSON_AddItemToObject(root, "dyn", dyn_array);
    
    char* json_str = cJSON_Print(root);
    fprintf(f, "%s", json_str);
    
    cJSON_Delete(root);
    free(json_str);
    fclose(f);
    
    ESP_LOGI(TAG, "成功保存 %zu 个动态URL", urls.size());
    return true;
}

bool CacheManager::SaveStaticUrl(const std::string& url, const char* cache_file) {
    FILE* f = fopen(cache_file, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法创建静态URL缓存文件: %s", cache_file);
        return false;
    }
    
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "sta", url.c_str());
    
    char* json_str = cJSON_Print(root);
    fprintf(f, "%s", json_str);
    
    cJSON_Delete(root);
    free(json_str);
    fclose(f);
    
    ESP_LOGI(TAG, "成功保存静态URL: %s", url.c_str());
    return true;
}

} // namespace ImageResource
