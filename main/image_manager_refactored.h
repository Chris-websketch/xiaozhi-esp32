#pragma once

#include "memory/memory_manager.h"
#include "error/error_handling.h"
#include "config/resource_config.h"
#include "downloader/network_downloader.h"
#include <vector>
#include <memory>
#include <atomic>

// 前向声明
class ImageResourceManager;

namespace ImageResource {

/**
 * 重构后的图片资源管理器
 * 
 * 主要改进：
 * 1. 使用Result<T>替代混乱的返回值类型
 * 2. 使用RAII内存管理替代手动malloc/free
 * 3. 使用配置管理器替代硬编码常量
 * 4. 职责分离：专门的下载器、文件处理器等
 * 5. 更好的错误处理和恢复机制
 */
class ImageResourceManagerV2 {
public:
    // 获取单例实例
    static ImageResourceManagerV2& GetInstance() {
        static ImageResourceManagerV2 instance;
        return instance;
    }
    
    // 初始化（使用Result<void>替代esp_err_t）
    Result<void> Initialize();
    
    // 检查并更新所有资源（简化的API）
    Result<void> CheckAndUpdateAllResources(const std::string& api_url, const std::string& version_url);
    
    // 获取图片数组（不变）
    const std::vector<const uint8_t*>& GetImageArray() const { return image_array_; }
    
    // 获取logo图片（不变）
    const uint8_t* GetLogoImage() const { return logo_data_ ? logo_data_->data() : nullptr; }
    
    // 按需加载图片（改进的API）
    Result<void> LoadImageOnDemand(int image_index);
    
    // 预加载剩余图片（改进的API）
    Result<void> PreloadRemainingImages();
    
    // 静默预加载（新功能）
    Result<void> PreloadRemainingImagesSilent(unsigned long time_budget_ms = 0);
    
    // 设置进度回调
    void SetProgressCallback(std::function<void(int current, int total, const char* message)> callback) {
        legacy_progress_callback_ = callback;
    }
    
    // 取消操作
    void CancelOperations();
    
    // 获取状态信息
    bool HasValidImages() const { return has_valid_images_; }
    bool HasValidLogo() const { return has_valid_logo_; }
    bool IsOperationInProgress() const { return operation_in_progress_.load(); }

private:
    ImageResourceManagerV2();
    ~ImageResourceManagerV2();
    
    // 禁用拷贝
    ImageResourceManagerV2(const ImageResourceManagerV2&) = delete;
    ImageResourceManagerV2& operator=(const ImageResourceManagerV2&) = delete;
    
    // 核心功能
    Result<void> mount_resources_partition();
    Result<void> load_cached_urls();
    Result<void> check_local_resources();
    Result<void> check_server_resources(const std::string& version_url, 
                                       bool& need_update_animations, bool& need_update_logo);
    Result<void> download_resources(const std::string& api_url);
    Result<void> load_image_data();
    
    // 专用功能
    Result<void> download_animations();
    Result<void> download_logo();
    Result<void> build_packed_images();
    
    // 辅助方法
    void convert_progress_callback(const DownloadProgress& progress);
    Result<void> validate_configuration() const;
    void enter_operation_mode();
    void exit_operation_mode();
    
    // 成员变量
    ResourceConfig config_;
    std::unique_ptr<NetworkDownloader> downloader_;
    
    // 图片数据（使用智能指针管理）
    std::vector<std::unique_ptr<MemoryBlock>> image_data_;
    std::vector<const uint8_t*> image_array_;
    std::unique_ptr<MemoryBlock> logo_data_;
    
    // URL缓存
    std::vector<std::string> cached_dynamic_urls_;
    std::string cached_static_url_;
    std::vector<std::string> server_dynamic_urls_;
    std::string server_static_url_;
    
    // 状态管理
    bool mounted_ = false;
    bool initialized_ = false;
    bool has_valid_images_ = false;
    bool has_valid_logo_ = false;
    std::atomic<bool> operation_in_progress_{false};
    std::atomic<bool> cancelled_{false};
    
    // 回调函数（兼容性）
    std::function<void(int, int, const char*)> legacy_progress_callback_;
};

/**
 * 迁移助手类
 * 帮助从旧版本API逐步迁移到新版本
 */
class MigrationHelper {
public:
    // 将esp_err_t转换为Result<void>
    static Result<void> from_esp_err(esp_err_t err, const std::string& context = "");
    
    // 将Result<void>转换为esp_err_t（用于兼容性）
    static esp_err_t to_esp_err(const Result<void>& result);
    
    // 包装旧版本回调为新版本格式
    static std::function<void(const DownloadProgress&)> 
    wrap_legacy_callback(std::function<void(int, int, const char*)> legacy_callback);
    
    // 性能对比工具
    static void benchmark_old_vs_new(::ImageResourceManager& old_manager, 
                                    ImageResourceManagerV2& new_manager);
};

} // namespace ImageResource
