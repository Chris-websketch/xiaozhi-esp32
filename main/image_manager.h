#pragma once

#include <string>
#include <vector>
#include <stdint.h>
#include <functional>
#include <atomic>
#include "esp_err.h"

// 前向声明
namespace ImageResource {
    struct ResourceConfig;
    class SpiffsManager;
    class CacheManager;
    class Downloader;
    class VersionChecker;
    class ImageLoader;
    class PackedLoader;
    class PreloadManager;
    class DownloadMode;
    class CleanupHelper;
}

// 二进制图片格式常量（保持向后兼容）
#define BINARY_IMAGE_MAGIC UINT32_C(0x42494D47)
#define BINARY_IMAGE_VERSION UINT32_C(1)

struct BinaryImageHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t data_size;
    uint32_t reserved[3];
};

/**
 * 图片资源管理器（重构版）
 * 协调各模块完成图片资源的下载、加载和管理
 */
class ImageResourceManager {
public:
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;

    // 获取单例
    static ImageResourceManager& GetInstance() {
        static ImageResourceManager instance;
        return instance;
    }
    
    // 初始化
    esp_err_t Initialize();
    
    // 资源检查和更新
    esp_err_t CheckAndUpdateResources(const char* api_url, const char* version_url);
    esp_err_t CheckAndUpdateLogo(const char* api_url, const char* logo_version_url);
    esp_err_t CheckAndUpdateAllResources(const char* api_url, const char* version_url);
    
    // 图片访问
    const std::vector<const uint8_t*>& GetImageArray() const;
    const uint8_t* GetLogoImage() const;
    
    // 按需加载
    bool LoadImageOnDemand(int image_index);
    bool IsImageLoaded(int image_index) const;
    
    // 预加载
    esp_err_t PreloadRemainingImages();
    esp_err_t PreloadRemainingImagesSilent(unsigned long time_budget_ms);
    void CancelPreload();
    bool IsPreloading() const;
    bool WaitForPreloadToFinish(unsigned long timeout_ms);
    
    // 回调设置
    void SetDownloadProgressCallback(ProgressCallback callback);
    void SetPreloadProgressCallback(ProgressCallback callback);
    
    // 调试功能
    bool ClearAllImageFiles();

private:
    ImageResourceManager();
    ~ImageResourceManager();
    
    // 禁用拷贝
    ImageResourceManager(const ImageResourceManager&) = delete;
    ImageResourceManager& operator=(const ImageResourceManager&) = delete;
    
    // 内部方法
    bool CheckImagesExist();
    bool CheckLogoExists();
    esp_err_t DownloadImages();
    esp_err_t DownloadLogo();
    void LoadImageData();
    bool LoadImageFile(int image_index);
    bool LoadLogoFile();
    bool BuildAndRestart();
    
    // 模块实例（使用指针以实现前向声明）
    ImageResource::SpiffsManager* spiffs_mgr_;
    ImageResource::CacheManager* cache_mgr_;
    ImageResource::Downloader* downloader_;
    ImageResource::VersionChecker* version_checker_;
    ImageResource::ImageLoader* image_loader_;
    ImageResource::PackedLoader* packed_loader_;
    ImageResource::PreloadManager* preload_mgr_;
    ImageResource::DownloadMode* download_mode_;
    ImageResource::CleanupHelper* cleanup_helper_;
    
    // 配置
    const ImageResource::ResourceConfig* config_;
    
    // 状态变量
    bool initialized_;
    bool has_valid_images_;
    bool has_valid_logo_;
    
    // 下载任务追踪
    bool pending_animations_download_;
    bool pending_logo_download_;
    bool animations_download_completed_;
    bool logo_download_completed_;
    
    // URL缓存
    std::string cached_static_url_;
    std::vector<std::string> cached_dynamic_urls_;
    std::string server_static_url_;
    std::vector<std::string> server_dynamic_urls_;
    
    // 图片数据
    std::vector<const uint8_t*> image_array_;
    std::vector<uint8_t*> image_data_pointers_;
    uint8_t* logo_data_;
    
    // 回调
    ProgressCallback progress_callback_;
    ProgressCallback preload_progress_callback_;
};
