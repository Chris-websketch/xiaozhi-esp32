#pragma once

#include <esp_err.h>
#include <string>
#include <vector>
#include <functional>

// 前向声明
namespace ImageResource {
    struct ResourceConfig;
}

namespace ImageResource {

/**
 * 文件下载器
 * 负责HTTP文件下载、重试机制和进度管理
 */
class Downloader {
public:
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;

    explicit Downloader(const ResourceConfig* config);
    ~Downloader() = default;

    // 禁用拷贝
    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    /**
     * 下载单个文件
     * @param url 文件URL
     * @param filepath 保存路径
     * @param file_index 文件索引（用于批量下载进度计算）
     * @param total_files 总文件数
     * @return ESP_OK成功，其他失败
     */
    esp_err_t DownloadFile(const char* url, 
                          const char* filepath,
                          int file_index = 0,
                          int total_files = 1);

    /**
     * 批量下载文件
     * @param urls URL列表
     * @param filepaths 文件路径列表
     * @return ESP_OK成功，其他失败
     */
    esp_err_t DownloadBatch(const std::vector<std::string>& urls,
                           const std::vector<std::string>& filepaths);

    /**
     * 设置进度回调
     */
    void SetProgressCallback(ProgressCallback callback) {
        progress_callback_ = callback;
    }

private:
    const ResourceConfig* config_;
    ProgressCallback progress_callback_;
};

} // namespace ImageResource
