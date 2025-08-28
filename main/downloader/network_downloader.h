#pragma once

#include "../memory/memory_manager.h"
#include "../error/error_handling.h"
#include "../config/resource_config.h"
#include <string>
#include <functional>
#include <memory>

// 前向声明
class Http;

namespace ImageResource {

/**
 * 下载进度信息
 */
struct DownloadProgress {
    size_t total_bytes = 0;
    size_t downloaded_bytes = 0;
    int file_index = 0;
    int total_files = 1;
    std::string current_file;
    
    int get_file_percentage() const {
        return total_bytes > 0 ? (downloaded_bytes * 100 / total_bytes) : 0;
    }
    
    int get_total_percentage() const {
        return total_files > 0 ? ((file_index * 100 + get_file_percentage()) / total_files) : 0;
    }
};

/**
 * 下载回调类型
 */
using ProgressCallback = std::function<void(const DownloadProgress&)>;
using CompletionCallback = std::function<void(Result<void>)>;

/**
 * 网络连接管理器
 */
class NetworkConnection {
public:
    explicit NetworkConnection(const ResourceConfig& config);
    ~NetworkConnection();
    
    Result<Http*> create_connection(const std::string& url);
    Result<void> verify_network_available();
    
private:
    const ResourceConfig& config_;
    void setup_headers(Http* http);
};

/**
 * 文件写入管理器
 */
class FileWriter {
public:
    explicit FileWriter(const std::string& filepath, const ResourceConfig& config);
    ~FileWriter();
    
    Result<void> open();
    Result<size_t> write(const uint8_t* data, size_t size);
    Result<void> close();
    bool is_open() const { return file_ != nullptr; }
    
private:
    std::string filepath_;
    const ResourceConfig& config_;
    FILE* file_;
    size_t bytes_written_;
};

/**
 * 下载进度追踪器
 */
class ProgressTracker {
public:
    ProgressTracker(ProgressCallback callback, int file_index = 0, int total_files = 1);
    
    void set_total_size(size_t total_size);
    void update_progress(size_t bytes_downloaded, const std::string& message = "");
    void set_message(const std::string& message);
    void complete(bool success, const std::string& message = "");
    
private:
    ProgressCallback callback_;
    DownloadProgress progress_;
    int last_reported_percentage_;
    void notify_if_changed();
};

/**
 * 网络下载器
 */
class NetworkDownloader {
public:
    explicit NetworkDownloader(const ResourceConfig& config = ConfigManager::GetInstance().get_config());
    ~NetworkDownloader() = default;
    
    // 设置回调
    void set_progress_callback(ProgressCallback callback) { progress_callback_ = callback; }
    
    // 下载单个文件
    Result<void> download_file(const std::string& url, const std::string& filepath,
                              int file_index = 0, int total_files = 1);
    
    // 下载多个文件
    Result<void> download_files(const std::vector<std::pair<std::string, std::string>>& url_file_pairs);
    
    // 取消下载
    void cancel_download() { cancelled_ = true; }
    bool is_cancelled() const { return cancelled_; }
    
    // 重置取消状态
    void reset() { cancelled_ = false; }

private:
    const ResourceConfig& config_;
    ProgressCallback progress_callback_;
    std::atomic<bool> cancelled_{false};
    
    // 内部实现
    Result<void> download_file_impl(const std::string& url, const std::string& filepath,
                                   int file_index, int total_files);
    
    Result<void> download_with_retry(const std::string& url, const std::string& filepath,
                                    int file_index, int total_files);
    
    Result<void> perform_download(Http* http, FileWriter& writer, ProgressTracker& tracker);
    
    Result<void> validate_download(const std::string& filepath, size_t expected_size);
    
    void cleanup_on_failure(const std::string& filepath);
};

/**
 * 下载工厂类
 */
class DownloadFactory {
public:
    enum class DownloaderType {
        Standard,    // 标准下载器
        Optimized,   // 优化下载器（更大缓冲区）
        Conservative // 保守下载器（更小缓冲区，更多检查）
    };
    
    static std::unique_ptr<NetworkDownloader> create(DownloaderType type = DownloaderType::Standard);
    static std::unique_ptr<NetworkDownloader> create_for_device(); // 根据设备自动选择
};

} // namespace ImageResource
