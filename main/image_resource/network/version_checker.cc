#include "version_checker.h"
#include <esp_log.h>
#include <cJSON.h>
#include <wifi_station.h>
#include <board.h>
#include <system_info.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>

#define TAG "VersionChecker"

namespace ImageResource {

esp_err_t VersionChecker::CheckServer(const char* api_url, ResourceVersions& versions) {
    if (!Board::GetInstance().IsNetworkReady()) {
        ESP_LOGW(TAG, "网络未连接");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "检查服务器资源版本: %s", api_url);
    
    auto http = Board::GetInstance().CreateHttp();
    if (!http) {
        ESP_LOGE(TAG, "无法创建HTTP客户端");
        return ESP_FAIL;
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
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Content-Type", "application/json");
    
    if (!http->Open("GET", api_url)) {
        ESP_LOGE(TAG, "无法连接到服务器: %s", api_url);
        delete http;
        return ESP_FAIL;
    }
    
    std::string response = http->GetBody();
    http->Close();
    delete http;
    
    if (response.empty()) {
        ESP_LOGE(TAG, "服务器返回空响应");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "服务器响应: %s", response.c_str());
    
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "解析服务器响应失败");
        return ESP_FAIL;
    }
    
    // 解析动态图片URL数组
    cJSON* dyn_array = cJSON_GetObjectItem(root, "dyn");
    if (dyn_array != NULL && cJSON_IsArray(dyn_array)) {
        versions.dynamic_urls.clear();
        int array_size = cJSON_GetArraySize(dyn_array);
        for (int i = 0; i < array_size; i++) {
            cJSON* url_item = cJSON_GetArrayItem(dyn_array, i);
            if (cJSON_IsString(url_item)) {
                versions.dynamic_urls.push_back(url_item->valuestring);
            }
        }
        ESP_LOGI(TAG, "解析到 %d 个动态图片URL", array_size);
    } else {
        ESP_LOGW(TAG, "未找到动态图片URL数组");
    }
    
    // 解析静态图片URL
    cJSON* sta_url = cJSON_GetObjectItem(root, "sta");
    if (sta_url != NULL && cJSON_IsString(sta_url)) {
        versions.static_url = sta_url->valuestring;
        ESP_LOGI(TAG, "解析到静态图片URL: %s", versions.static_url.c_str());
    } else {
        ESP_LOGW(TAG, "未找到静态图片URL");
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

bool VersionChecker::NeedsUpdate(const ResourceVersions& server_versions,
                                const ResourceVersions& local_versions,
                                bool& need_update_dynamic,
                                bool& need_update_static) {
    need_update_dynamic = false;
    need_update_static = false;
    
    // 检查动态图片URL
    if (server_versions.dynamic_urls.empty()) {
        ESP_LOGW(TAG, "服务器未返回动态图片URL");
    } else if (local_versions.dynamic_urls.size() != server_versions.dynamic_urls.size()) {
        ESP_LOGI(TAG, "动态图片URL数量不一致: 本地%zu，服务器%zu",
                local_versions.dynamic_urls.size(), server_versions.dynamic_urls.size());
        need_update_dynamic = true;
    } else {
        bool urls_differ = false;
        for (size_t i = 0; i < local_versions.dynamic_urls.size(); i++) {
            if (local_versions.dynamic_urls[i] != server_versions.dynamic_urls[i]) {
                urls_differ = true;
                break;
            }
        }
        if (urls_differ) {
            ESP_LOGI(TAG, "动态图片URL内容不一致");
            need_update_dynamic = true;
        } else {
            ESP_LOGI(TAG, "动态图片URL一致");
        }
    }
    
    // 检查静态图片URL
    if (server_versions.static_url.empty()) {
        ESP_LOGW(TAG, "服务器未返回静态图片URL");
    } else if (local_versions.static_url != server_versions.static_url) {
        ESP_LOGI(TAG, "静态图片URL不一致: 本地'%s'，服务器'%s'",
                local_versions.static_url.c_str(), server_versions.static_url.c_str());
        need_update_static = true;
    } else {
        ESP_LOGI(TAG, "静态图片URL一致");
    }
    
    return need_update_dynamic || need_update_static;
}

} // namespace ImageResource
