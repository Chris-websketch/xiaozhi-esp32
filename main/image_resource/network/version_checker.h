#pragma once

#include <esp_err.h>
#include <string>
#include <vector>

namespace ImageResource {

/**
 * 版本检查器
 * 负责检查服务器资源版本
 */
class VersionChecker {
public:
    struct ResourceVersions {
        std::vector<std::string> dynamic_urls;  // 动态图片URL列表
        std::string static_url;                 // 静态图片URL
    };

    VersionChecker() = default;
    ~VersionChecker() = default;

    // 禁用拷贝
    VersionChecker(const VersionChecker&) = delete;
    VersionChecker& operator=(const VersionChecker&) = delete;

    /**
     * 检查服务器资源版本
     * @param api_url API地址
     * @param versions 输出服务器版本信息
     * @return ESP_OK成功，其他失败
     */
    esp_err_t CheckServer(const char* api_url, ResourceVersions& versions);

    /**
     * 检查是否需要更新
     * @param server_versions 服务器版本
     * @param local_versions 本地版本
     * @param need_update_dynamic 输出是否需要更新动态图片
     * @param need_update_static 输出是否需要更新静态图片
     * @return true需要更新，false不需要
     */
    bool NeedsUpdate(const ResourceVersions& server_versions,
                    const ResourceVersions& local_versions,
                    bool& need_update_dynamic,
                    bool& need_update_static);
};

} // namespace ImageResource
