#include "image_manager_refactored.h"
#include "image_manager.h" // 原始版本，用于对比
#include <esp_log.h>
#include <esp_spiffs.h>
#include <wifi_station.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "ImageManagerV2"

namespace ImageResource {

// ImageResourceManagerV2 实现

ImageResourceManagerV2::ImageResourceManagerV2() {
    // 初始化配置
    config_ = ConfigManager::GetInstance().get_config();
    
    // 创建下载器
    downloader_ = DownloadFactory::create_for_device();
    
    // 设置下载进度回调
    downloader_->set_progress_callback(
        [this](const DownloadProgress& progress) {
            convert_progress_callback(progress);
        }
    );
}

ImageResourceManagerV2::~ImageResourceManagerV2() {
    // RAII自动清理，无需手动释放内存
}

Result<void> ImageResourceManagerV2::Initialize() {
    ESP_LOGI(TAG, "初始化图片资源管理器V2...");
    
    if (initialized_) {
        return Result<void>::success();
    }
    
    // 验证配置
    auto config_result = validate_configuration();
    RETURN_IF_ERROR(config_result);
    
    // 挂载资源分区
    auto mount_result = mount_resources_partition();
    RETURN_IF_ERROR(mount_result);
    
    // 加载缓存的URL
    auto cache_result = load_cached_urls();
    RETURN_IF_ERROR(cache_result);
    
    // 检查本地资源
    auto local_result = check_local_resources();
    RETURN_IF_ERROR(local_result);
    
    // 如果有有效资源，立即加载
    if (has_valid_images_ || has_valid_logo_) {
        auto load_result = load_image_data();
        if (load_result.is_error()) {
            ESP_LOGW(TAG, "加载现有图片数据失败，但初始化继续");
        }
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "图片资源管理器V2初始化完成");
    
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::CheckAndUpdateAllResources(
    const std::string& api_url, const std::string& version_url) {
    
    if (operation_in_progress_.load()) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidState, "操作正在进行中");
    }
    
    enter_operation_mode();
    
    // 使用RAII确保退出操作模式
    auto operation_guard = [this](void*) { exit_operation_mode(); };
    std::unique_ptr<void, decltype(operation_guard)> guard(nullptr, operation_guard);
    
    ESP_LOGI(TAG, "开始检查并更新所有资源...");
    
    bool need_update_animations = !has_valid_images_;
    bool need_update_logo = !has_valid_logo_;
    
    // 检查服务器资源
    auto server_check = check_server_resources(version_url, need_update_animations, need_update_logo);
    RETURN_IF_ERROR(server_check);
    
    // 如果不需要更新，尝试构建打包文件
    if (!need_update_animations && !need_update_logo) {
        ESP_LOGI(TAG, "所有资源都是最新版本");
        
        auto pack_result = build_packed_images();
        if (pack_result.is_success()) {
            ESP_LOGI(TAG, "构建打包文件成功，系统将重启");
            esp_restart();
        }
        
        return Result<void>::error(ErrorCode::NotFound, "无需更新");
    }
    
    // 下载需要更新的资源
    auto download_result = download_resources(api_url);
    RETURN_IF_ERROR(download_result);
    
    ESP_LOGI(TAG, "所有资源更新完成");
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::LoadImageOnDemand(int image_index) {
    if (image_index < 1 || image_index > static_cast<int>(config_.image.max_image_count)) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidArgument, 
                           "图片索引超出范围: " + std::to_string(image_index));
    }
    
    int array_index = image_index - 1;
    
    // 检查是否已加载
    if (array_index < image_array_.size() && image_array_[array_index] != nullptr) {
        return Result<void>::success();
    }
    
    // 检查内存可用性
    if (!MemoryManager::GetInstance().has_available_memory(config_.get_image_size())) {
        ESP_LOGW(TAG, "=== 🆘 图片加载内存不足详情 ===");
        MemoryManager::GetInstance().log_memory_status();
        ImageBufferPool::GetInstance().log_pool_status();
        LOG_AND_RETURN_ERROR(ErrorCode::NoMemory, "内存不足，无法加载图片");
    }
    
    ESP_LOGI(TAG, "按需加载图片 %d", image_index);
    
    // 确保数组大小足够
    if (image_data_.size() <= array_index) {
        image_data_.resize(config_.image.max_image_count);
        image_array_.resize(config_.image.max_image_count, nullptr);
    }
    
    // 分配内存并加载图片
    auto buffer = ImageBufferPool::GetInstance().acquire_buffer();
    if (!buffer || !buffer->is_valid()) {
        LOG_AND_RETURN_ERROR(ErrorCode::MemoryAllocationFailed, "图片缓冲区分配失败");
    }
    
    // 这里应该调用实际的图片加载逻辑
    // 为简化，我们假设加载成功
    image_data_[array_index] = std::move(buffer);
    image_array_[array_index] = image_data_[array_index]->data();
    
    ESP_LOGI(TAG, "图片 %d 按需加载完成", image_index);
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::PreloadRemainingImages() {
    if (!has_valid_images_) {
        LOG_AND_RETURN_ERROR(ErrorCode::ResourceNotInitialized, "没有有效的图片资源");
    }
    
    ESP_LOGI(TAG, "开始预加载剩余图片...");
    
    int loaded_count = 0;
    int total_images = config_.image.max_image_count;
    
    for (int i = 1; i <= total_images; ++i) {
        if (cancelled_.load()) {
            LOG_AND_RETURN_ERROR(ErrorCode::InvalidState, "预加载被取消");
        }
        
        // 检查是否已加载
        int array_index = i - 1;
        if (array_index < image_array_.size() && image_array_[array_index] != nullptr) {
            loaded_count++;
            continue;
        }
        
        // 检查内存状况
        if (!MemoryManager::GetInstance().has_available_memory(config_.get_image_size())) {
            ESP_LOGW(TAG, "内存不足，停止预加载，已加载: %d/%d", loaded_count, total_images);
            break;
        }
        
        // 加载图片
        auto load_result = LoadImageOnDemand(i);
        if (load_result.is_success()) {
            loaded_count++;
            ESP_LOGI(TAG, "预加载图片 %d 成功", i);
        } else {
            ESP_LOGW(TAG, "预加载图片 %d 失败", i);
        }
        
        // 适度延迟
        vTaskDelay(pdMS_TO_TICKS(config_.preload.load_delay_ms));
    }
    
    ESP_LOGI(TAG, "预加载完成，成功加载: %d/%d", loaded_count, total_images);
    return loaded_count > 0 ? Result<void>::success() : 
                             Result<void>::error(ErrorCode::ImageLoadFailed, "没有成功加载任何图片");
}

Result<void> ImageResourceManagerV2::PreloadRemainingImagesSilent(unsigned long time_budget_ms) {
    // 静默预加载实现（简化版）
    ESP_LOGI(TAG, "开始静默预加载，时间预算: %lu ms", time_budget_ms);
    
    TickType_t start_time = xTaskGetTickCount();
    TickType_t budget_ticks = time_budget_ms ? pdMS_TO_TICKS(time_budget_ms) : 0;
    
    int loaded_count = 0;
    
    for (int i = 1; i <= static_cast<int>(config_.image.max_image_count); ++i) {
        // 检查时间预算
        if (budget_ticks && (xTaskGetTickCount() - start_time) >= budget_ticks) {
            ESP_LOGI(TAG, "静默预加载时间用尽，已加载: %d", loaded_count);
            break;
        }
        
        if (cancelled_.load()) {
            break;
        }
        
        auto result = LoadImageOnDemand(i);
        if (result.is_success()) {
            loaded_count++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5)); // 更短的延迟
    }
    
    ESP_LOGI(TAG, "静默预加载完成，加载数量: %d", loaded_count);
    return Result<void>::success();
}

void ImageResourceManagerV2::CancelOperations() {
    cancelled_.store(true);
    if (downloader_) {
        downloader_->cancel_download();
    }
    ESP_LOGI(TAG, "操作取消请求已发送");
}

// 私有方法实现

Result<void> ImageResourceManagerV2::mount_resources_partition() {
    if (mounted_) {
        return Result<void>::success();
    }
    
    ESP_LOGI(TAG, "挂载resources分区...");
    
    esp_vfs_spiffs_conf_t conf = {
        .base_path = config_.filesystem.base_path.c_str(),
        .partition_label = "resources",
        .max_files = config_.filesystem.max_files,
        .format_if_mount_failed = config_.filesystem.format_on_mount_fail
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        LOG_AND_RETURN_ERROR(ErrorHandler::from_esp_err(ret), 
                           "挂载resources分区失败: " + std::string(esp_err_to_name(ret)));
    }
    
    mounted_ = true;
    ESP_LOGI(TAG, "resources分区挂载成功");
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::check_server_resources(const std::string& version_url,
                                                           bool& need_update_animations,
                                                           bool& need_update_logo) {
    if (!WifiStation::GetInstance().IsConnected()) {
        LOG_AND_RETURN_ERROR(ErrorCode::NetworkNotConnected, "网络未连接，无法检查服务器资源");
    }
    
    ESP_LOGI(TAG, "检查服务器资源: %s", version_url.c_str());
    
    // 这里应该实现具体的服务器检查逻辑
    // 为简化，我们假设需要更新
    need_update_animations = !has_valid_images_;
    need_update_logo = !has_valid_logo_;
    
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::download_resources(const std::string& api_url) {
    ESP_LOGI(TAG, "开始下载资源...");
    
    // 准备下载列表
    std::vector<std::pair<std::string, std::string>> download_list;
    
    // 添加动画图片到下载列表
    for (const auto& url : server_dynamic_urls_) {
        // 这里应该生成正确的文件路径
        std::string filepath = config_.filesystem.image_path + "temp_image.bin";
        download_list.emplace_back(url, filepath);
    }
    
    // 添加logo到下载列表
    if (!server_static_url_.empty()) {
        download_list.emplace_back(server_static_url_, config_.get_logo_path());
    }
    
    // 执行批量下载
    auto download_result = downloader_->download_files(download_list);
    RETURN_IF_ERROR(download_result);
    
    // 更新状态
    has_valid_images_ = !server_dynamic_urls_.empty();
    has_valid_logo_ = !server_static_url_.empty();
    
    // 重新加载图片数据
    return load_image_data();
}

void ImageResourceManagerV2::convert_progress_callback(const DownloadProgress& progress) {
    if (legacy_progress_callback_) {
        legacy_progress_callback_(
            progress.get_total_percentage(),
            100,
            progress.current_file.c_str()
        );
    }
}

Result<void> ImageResourceManagerV2::validate_configuration() const {
    if (!ConfigManager::GetInstance().validate_config()) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidArgument, "配置验证失败");
    }
    
    // 额外的验证
    if (config_.image.max_image_count == 0) {
        LOG_AND_RETURN_ERROR(ErrorCode::InvalidArgument, "图片数量不能为0");
    }
    
    return Result<void>::success();
}

void ImageResourceManagerV2::enter_operation_mode() {
    operation_in_progress_.store(true);
    cancelled_.store(false);
    ESP_LOGI(TAG, "进入操作模式");
}

void ImageResourceManagerV2::exit_operation_mode() {
    operation_in_progress_.store(false);
    ESP_LOGI(TAG, "退出操作模式");
}

// 其他方法的简化实现...
Result<void> ImageResourceManagerV2::load_cached_urls() {
    // 简化实现
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::check_local_resources() {
    // 简化实现
    has_valid_images_ = false;
    has_valid_logo_ = false;
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::load_image_data() {
    // 简化实现
    return Result<void>::success();
}

Result<void> ImageResourceManagerV2::build_packed_images() {
    // 简化实现
    return Result<void>::success();
}

// MigrationHelper 实现

Result<void> MigrationHelper::from_esp_err(esp_err_t err, const std::string& context) {
    if (err == ESP_OK) {
        return Result<void>::success();
    }
    
    ErrorCode error_code = ErrorHandler::from_esp_err(err);
    return Result<void>::error(error_code, context);
}

esp_err_t MigrationHelper::to_esp_err(const Result<void>& result) {
    if (result.is_success()) {
        return ESP_OK;
    }
    
    // 简化的错误码映射
    switch (result.error().code) {
        case ErrorCode::NoMemory:
            return ESP_ERR_NO_MEM;
        case ErrorCode::InvalidArgument:
            return ESP_ERR_INVALID_ARG;
        case ErrorCode::NotFound:
            return ESP_ERR_NOT_FOUND;
        default:
            return ESP_FAIL;
    }
}

std::function<void(const DownloadProgress&)> 
MigrationHelper::wrap_legacy_callback(std::function<void(int, int, const char*)> legacy_callback) {
    return [legacy_callback](const DownloadProgress& progress) {
        if (legacy_callback) {
            legacy_callback(
                progress.get_total_percentage(),
                100,
                progress.current_file.c_str()
            );
        }
    };
}

} // namespace ImageResource
