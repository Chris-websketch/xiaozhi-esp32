#include "network_downloader.h"
#include "boards/common/board.h"
#include "boards/common/wifi_board.h"
#include "../system_info.h"
#include <esp_log.h>
#include <esp_app_format.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>
#include <cstring>
#include <wifi_station.h>

#define TAG "NetworkDownloader"

namespace ImageResource {

// NetworkConnection 实现
NetworkConnection::NetworkConnection(const ResourceConfig& config) : config_(config) {}

NetworkConnection::~NetworkConnection() = default;

Result<Http*> NetworkConnection::create_connection(const std::string& url) {
    auto verify_result = verify_network_available();
    if (!verify_result.is_success()) {
        return Result<Http*>::error(verify_result.error());
    }
    
    auto http = Board::GetInstance().CreateHttp();
    if (!http) {
        return Result<Http*>::error(ErrorCode::NetworkConnectionFailed, ErrorMessages::NETWORK_CONNECTION_FAILED);
    }
    
    setup_headers(http);
    
    if (!http->Open("GET", url.c_str())) {
        return Result<Http*>::error(ErrorCode::NetworkConnectionFailed, ErrorMessages::NETWORK_CONNECTION_FAILED);
    }
    
    // 修复：直接返回原始指针
    return Result<Http*>::success(http);
}

Result<void> NetworkConnection::verify_network_available() {
    if (!WifiStation::GetInstance().IsConnected()) {
        return Result<void>::error(ErrorCode::NetworkNotConnected, ErrorMessages::NETWORK_NOT_CONNECTED);
    }
    return Result<void>::success();
}

void NetworkConnection::setup_headers(Http* http) {
    std::string device_id = SystemInfo::GetMacAddress();
    std::string client_id = SystemInfo::GetClientId();
    
    if (!device_id.empty()) {
        http->SetHeader("Device-Id", device_id.c_str());
    }
    if (!client_id.empty()) {
        http->SetHeader("Client-Id", client_id.c_str());
    }
    
    // 使用固定的版本号，避免依赖esp_app_get_description
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/1.0.0"));
    
    if (config_.network.enable_keep_alive) {
        http->SetHeader("Connection", "keep-alive");
    }
    
    http->SetHeader("Accept-Encoding", "identity");
    http->SetHeader("Cache-Control", "no-cache");
    http->SetHeader("Accept", "*/*");
}

// FileWriter 实现
FileWriter::FileWriter(const std::string& filepath, const ResourceConfig& config)
    : filepath_(filepath), config_(config), file_(nullptr), bytes_written_(0) {}

FileWriter::~FileWriter() {
    if (file_) {
        fclose(file_);
    }
}

Result<void> FileWriter::open() {
    const char* mode = filepath_.find(".bin") != std::string::npos ? "wb" : "w";
    file_ = fopen(filepath_.c_str(), mode);
    
    if (!file_) {
        LOG_AND_RETURN_ERROR(ErrorCode::FileWriteFailed, 
                           "无法创建文件: " + filepath_);
    }
    
    ESP_LOGI(TAG, "文件打开成功: %s", filepath_.c_str());
    return Result<void>::success();
}

Result<size_t> FileWriter::write(const uint8_t* data, size_t size) {
    if (!file_) {
        return Result<size_t>::error(ErrorCode::InvalidState, ErrorMessages::INVALID_STATE);
    }
    
    size_t written = fwrite(data, 1, size, file_);
    if (written != size) {
        return Result<size_t>::error(ErrorCode::FileWriteFailed, ErrorMessages::NO_MEMORY);
    }
    
    bytes_written_ += written;
    return Result<size_t>::success(written);
}

Result<void> FileWriter::close() {
    if (file_) {
        int result = fclose(file_);
        file_ = nullptr;
        
        if (result != 0) {
            LOG_AND_RETURN_ERROR(ErrorCode::FileWriteFailed, "文件关闭失败");
        }
        
        ESP_LOGI(TAG, "文件关闭成功，共写入 %zu 字节", bytes_written_);
    }
    
    return Result<void>::success();
}

// ProgressTracker 实现
ProgressTracker::ProgressTracker(ProgressCallback callback, int file_index, int total_files)
    : callback_(callback), last_reported_percentage_(-1) {
    progress_.file_index = file_index;
    progress_.total_files = total_files;
}

void ProgressTracker::set_total_size(size_t total_size) {
    progress_.total_bytes = total_size;
    progress_.downloaded_bytes = 0;
    last_reported_percentage_ = -1;
}

void ProgressTracker::update_progress(size_t bytes_downloaded, const std::string& message) {
    progress_.downloaded_bytes = bytes_downloaded;
    if (!message.empty()) {
        progress_.current_file = message;
    }
    notify_if_changed();
}

void ProgressTracker::set_message(const std::string& message) {
    progress_.current_file = message;
    if (callback_) {
        callback_(progress_);
    }
}

void ProgressTracker::complete(bool success, const std::string& message) {
    if (success) {
        progress_.downloaded_bytes = progress_.total_bytes;
    }
    progress_.current_file = message;
    if (callback_) {
        callback_(progress_);
    }
}

void ProgressTracker::notify_if_changed() {
    int current_percentage = progress_.get_total_percentage();
    
    // 每2%更新一次，减少回调频率
    if (current_percentage != last_reported_percentage_ && 
        current_percentage % 2 == 0) {
        last_reported_percentage_ = current_percentage;
        if (callback_) {
            callback_(progress_);
        }
    }
}

// NetworkDownloader 实现
NetworkDownloader::NetworkDownloader(const ResourceConfig& config) : config_(config) {}

Result<void> NetworkDownloader::download_file(const std::string& url, const std::string& filepath,
                                             int file_index, int total_files) {
    if (cancelled_) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidState, "下载已取消");
    }
    
    return download_with_retry(url, filepath, file_index, total_files);
}

Result<void> NetworkDownloader::download_files(
    const std::vector<std::pair<std::string, std::string>>& url_file_pairs) {
    
    int total_files = url_file_pairs.size();
    
    for (int i = 0; i < total_files; ++i) {
        if (cancelled_) {
            LOG_AND_RETURN_ERROR(ErrorCode::InvalidState, "批量下载已取消");
        }
        
        const auto& pair = url_file_pairs[i];
        auto result = download_file(pair.first, pair.second, i, total_files);
        
        if (result.is_error()) {
            ESP_LOGE(TAG, "文件下载失败: %s", pair.second.c_str());
            return result;
        }
        
        // 文件间延迟
        if (config_.network.connection_delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(config_.network.connection_delay_ms));
        }
    }
    
    return Result<void>::success();
}

Result<void> NetworkDownloader::download_with_retry(const std::string& url, 
                                                   const std::string& filepath,
                                                   int file_index, int total_files) {
    RetryStrategy retry_strategy(config_.network.retry_count, config_.network.retry_delay_ms);
    
    while (true) {
        auto result = download_file_impl(url, filepath, file_index, total_files);
        
        if (result.is_success()) {
            return result;
        }
        
        // 尝试错误恢复
        auto recovery = retry_strategy.recover(result.error());
        if (recovery.is_error()) {
            // 重试已达上限
            cleanup_on_failure(filepath);
            return result;
        }
        
        ESP_LOGW(TAG, "下载失败，准备重试: %s", url.c_str());
    }
}

Result<void> NetworkDownloader::download_file_impl(const std::string& url,
                                                  const std::string& filepath,
                                                  int file_index, int total_files) {
    // 内存检查
    if (!MemoryManager::GetInstance().has_available_memory(config_.network.buffer_size)) {
        RETURN_NO_MEMORY(); // 优化：使用预定义错误消息
    }
    
    // 创建网络连接
    NetworkConnection connection(config_);
    auto http_result = connection.create_connection(url);
    if (!http_result.is_success()) {
        return Result<void>::error(http_result.error());
    }
    auto http = http_result.value();
    
    // 创建文件写入器
    FileWriter writer(filepath, config_);
    auto open_result = writer.open();
    if (!open_result.is_success()) {
        return open_result;
    }
    
    // 创建进度追踪器
    ProgressTracker tracker(progress_callback_, file_index, total_files);
    const char* filename = strrchr(filepath.c_str(), '/');
    filename = filename ? filename + 1 : filepath.c_str();
    tracker.set_message("正在下载: " + std::string(filename));
    
    // 执行下载
    auto download_result = perform_download(http, writer, tracker);
    
    // 关闭文件
    auto close_result = writer.close();
    http->Close();
    
    RETURN_IF_ERROR(download_result);
    RETURN_IF_ERROR(close_result);
    
    tracker.complete(true, "下载完成: " + std::string(filename));
    
    ESP_LOGI(TAG, "文件下载成功: %s", filepath.c_str());
    return Result<void>::success();
}

Result<void> NetworkDownloader::perform_download(Http* http, FileWriter& writer, 
                                                ProgressTracker& tracker) {
    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        LOG_AND_RETURN_ERROR(ErrorCode::NetworkDataCorrupted, "无法获取文件大小");
    }
    
    tracker.set_total_size(content_length);
    
    // 分配下载缓冲区
    auto buffer = MemoryManager::GetInstance().allocate(config_.network.buffer_size);
    if (!buffer || !buffer->is_valid()) {
        return Result<void>::error(ErrorCode::MemoryAllocationFailed, ErrorMessages::MEMORY_ALLOCATION_FAILED);
    }
    
    ESP_LOGI(TAG, "开始下载，文件大小: %u 字节，缓冲区: %u 字节",
             (unsigned int)content_length, (unsigned int)config_.network.buffer_size);
    
    size_t total_downloaded = 0;
    
    while (total_downloaded < content_length && !cancelled_) {
        // 网络状态检查
        if (!WifiStation::GetInstance().IsConnected()) {
            return Result<void>::error(ErrorCode::NetworkConnectionFailed, ErrorMessages::NETWORK_CONNECTION_FAILED);
        }
        
        // 读取数据
        int bytes_read = http->Read(const_cast<char*>(reinterpret_cast<const char*>(buffer->data())), buffer->size());
        if (bytes_read < 0) {
            return Result<void>::error(ErrorCode::NetworkDataCorrupted, ErrorMessages::NETWORK_DATA_CORRUPTED);
        }
        
        if (bytes_read == 0) {
            // 检查是否下载完成
            if (total_downloaded < content_length) {
                return Result<void>::error(ErrorCode::NetworkDataCorrupted, ErrorMessages::NETWORK_DATA_CORRUPTED);
            }
            break;
        }
        
        // 写入文件
        auto write_result = writer.write(buffer->data(), bytes_read);
        if (!write_result.is_success()) {
            return Result<void>::error(write_result.error());
        }
        
        total_downloaded += bytes_read;
        tracker.update_progress(total_downloaded);
        
        // 适度延迟，避免过度占用CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (cancelled_) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidState, "下载被用户取消");
    }
    
    if (total_downloaded != content_length) {
        LOG_AND_RETURN_ERROR(ErrorCode::NetworkDataCorrupted, 
                           "下载大小不匹配: 期望 " + std::to_string(content_length) + 
                           "，实际 " + std::to_string(total_downloaded));
    }
    
    return Result<void>::success();
}

Result<void> NetworkDownloader::validate_download(const std::string& filepath, 
                                                 size_t expected_size) {
    struct stat file_stat;
    if (stat(filepath.c_str(), &file_stat) != 0) {
        LOG_AND_RETURN_ERROR(ErrorCode::FileNotFound, "下载文件验证失败：文件不存在");
    }
    
    if (static_cast<size_t>(file_stat.st_size) != expected_size) {
        LOG_AND_RETURN_ERROR(ErrorCode::FileCorrupted, 
                           "文件大小验证失败: 期望 " + std::to_string(expected_size) +
                           "，实际 " + std::to_string(file_stat.st_size));
    }
    
    return Result<void>::success();
}

void NetworkDownloader::cleanup_on_failure(const std::string& filepath) {
    if (remove(filepath.c_str()) == 0) {
        ESP_LOGI(TAG, "已清理失败的下载文件: %s", filepath.c_str());
    } else {
        ESP_LOGW(TAG, "清理失败文件时出错: %s", filepath.c_str());
    }
}

// DownloadFactory 实现
std::unique_ptr<NetworkDownloader> DownloadFactory::create(DownloaderType type) {
    auto& config_manager = ConfigManager::GetInstance();
    auto config = config_manager.get_config();
    
    switch (type) {
        case DownloaderType::Optimized:
            config.network.buffer_size = 16384;
            config.network.connection_delay_ms = 200;
            break;
            
        case DownloaderType::Conservative:
            config.network.buffer_size = 4096;
            config.network.retry_count = 5;
            config.network.connection_delay_ms = 2000;
            break;
            
        case DownloaderType::Standard:
        default:
            // 使用默认配置
            break;
    }
    
    return std::make_unique<NetworkDownloader>(config);
}

std::unique_ptr<NetworkDownloader> DownloadFactory::create_for_device() {
    size_t free_heap = esp_get_free_heap_size();
    
    if (free_heap < 1024 * 1024) {
        return create(DownloaderType::Conservative);
    } else if (free_heap > 4 * 1024 * 1024) {
        return create(DownloaderType::Optimized);
    } else {
        return create(DownloaderType::Standard);
    }
}

} // namespace ImageResource
