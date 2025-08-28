#pragma once

#include <string>
#include <cstdint>

namespace ImageResource {

/**
 * 资源管理配置
 */
struct ResourceConfig {
    // 网络配置
    struct Network {
        uint32_t timeout_ms = 30000;           // 网络超时时间
        uint32_t retry_count = 3;              // 重试次数
        uint32_t retry_delay_ms = 5000;        // 重试延迟
        uint32_t buffer_size = 8192;           // 下载缓冲区大小
        uint32_t connection_delay_ms = 1000;   // 连接间延迟
        bool enable_keep_alive = true;         // 启用keep-alive
    } network;
    
    // 内存配置
    struct Memory {
        uint32_t allocation_threshold = 200 * 1024;  // 内存分配阈值 200KB
        uint32_t download_threshold = 300 * 1024;    // 下载最小内存要求 300KB
        uint32_t preload_threshold = 500 * 1024;     // 预加载最小内存要求 500KB
        uint32_t buffer_pool_size = 10;              // 缓冲区池大小
        bool enable_memory_pool = true;              // 启用内存池
    } memory;
    
    // 文件系统配置
    struct FileSystem {
        std::string base_path = "/resources";
        std::string image_path = "/resources/images/";
        std::string cache_path = "/resources/cache/";
        std::string logo_filename = "logo.bin";
        std::string packed_filename = "packed.rgb";
        uint32_t max_files = 30;                     // SPIFFS最大文件数
        bool format_on_mount_fail = true;            // 挂载失败时格式化
    } filesystem;
    
    // 图片配置
    struct Image {
        uint32_t max_image_count = 9;           // 最大图片数量
        uint32_t image_width = 240;             // 图片宽度
        uint32_t image_height = 240;            // 图片高度
        uint32_t bytes_per_pixel = 2;           // 每像素字节数 (RGB565)
        bool enable_format_conversion = true;   // 启用格式转换
        bool enable_packed_loading = true;      // 启用打包加载
    } image;
    
    // 下载模式配置
    struct DownloadMode {
        bool disable_power_save = true;         // 禁用省电模式
        bool pause_audio = true;                // 暂停音频处理
        bool boost_priority = true;             // 提升任务优先级
        uint32_t gc_interval_ms = 1000;         // 垃圾回收间隔
        uint32_t network_stabilize_ms = 1000;   // 网络稳定等待时间
    } download_mode;
    
    // 预加载配置
    struct Preload {
        uint32_t check_interval = 3;            // 音频状态检查间隔（每N张图片）
        uint32_t load_delay_ms = 10;            // 加载延迟
        uint32_t progress_update_threshold = 2; // 进度更新阈值
        bool enable_silent_preload = true;      // 启用静默预加载
        uint32_t time_budget_ms = 0;            // 时间预算（0=无限制）
    } preload;
    
    // 调试配置
    struct Debug {
        bool enable_file_verification = false;  // 启用文件验证（仅调试）
        bool verbose_logging = false;           // 详细日志
        bool enable_memory_tracking = true;     // 启用内存跟踪
        bool enable_performance_metrics = false; // 启用性能指标
    } debug;
    
    // 计算衍生值
    uint32_t get_image_size() const {
        return image.image_width * image.image_height * image.bytes_per_pixel;
    }
    
    uint32_t get_total_images_size() const {
        return get_image_size() * image.max_image_count;
    }
    
    std::string get_logo_path() const {
        return filesystem.image_path + filesystem.logo_filename;
    }
    
    std::string get_packed_path() const {
        return filesystem.image_path + filesystem.packed_filename;
    }
    
    std::string get_image_filename(int index) const {
        char filename[64];
        snprintf(filename, sizeof(filename), "output_%04d.bin", index);
        return filesystem.image_path + filename;
    }
};

/**
 * 配置管理器
 */
class ConfigManager {
public:
    static ConfigManager& GetInstance();
    
    // 获取配置
    const ResourceConfig& get_config() const { return config_; }
    ResourceConfig& get_mutable_config() { return config_; }
    
    // 加载配置（从文件或默认值）
    bool load_config(const std::string& config_path = "");
    
    // 保存配置
    bool save_config(const std::string& config_path = "") const;
    
    // 验证配置
    bool validate_config() const;
    
    // 重置为默认配置
    void reset_to_defaults();
    
    // 根据设备能力调整配置
    void adjust_for_device();

private:
    ConfigManager() = default;
    ResourceConfig config_;
};

} // namespace ImageResource
