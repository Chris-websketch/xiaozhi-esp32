#pragma once

#include <string>
#include <cstdint>

namespace ImageResource {

/**
 * 资源管理配置
 */
struct ResourceConfig {
    /**
     * 网络配置参数
     * 控制网络连接、下载和重试行为
     */
    struct Network {
        uint32_t timeout_ms = 30000;           /**< 网络超时时间 (毫秒)
                                                 *   有效范围: 5000-60000ms
                                                 *   推荐值: 30000ms (30秒)
                                                 *   说明: 单次网络请求的最大等待时间 */
        
        uint32_t retry_count = 3;              /**< 网络请求重试次数
                                                 *   有效范围: 1-10次
                                                 *   推荐值: 3次
                                                 *   说明: 网络请求失败后的最大重试次数 */
        
        uint32_t retry_delay_ms = 3000;        /**< 重试间隔延迟 (毫秒)
                                                 *   有效范围: 1000-10000ms
                                                 *   推荐值: 3000ms (3秒)
                                                 *   说明: 每次重试之间的等待时间，避免频繁请求 */
        
        uint32_t buffer_size = 16384;          /**< 下载缓冲区大小 (字节)
                                                 *   有效范围: 4096-32768字节
                                                 *   推荐值: 16384字节 (16KB)
                                                 *   说明: 网络下载时的缓冲区大小，影响下载效率 */
        
        uint32_t connection_delay_ms = 200;    /**< 连接间延迟 (毫秒)
                                                 *   有效范围: 100-2000ms
                                                 *   推荐值: 200ms
                                                 *   说明: 多个文件下载之间的延迟，避免网络拥塞 */
        
        bool enable_keep_alive = true;         /**< 启用HTTP Keep-Alive
                                                 *   推荐值: true
                                                 *   说明: 复用TCP连接，提高下载效率 */
    } network;
    
    /**
     * 内存管理配置参数
     * 控制内存分配、检查和优化策略
     */
    struct Memory {
        uint32_t allocation_threshold = 150 * 1024;    /**< 内存分配阈值 (字节)
                                                         *   有效范围: 100KB-500KB
                                                         *   推荐值: 150KB
                                                         *   说明: 进行内存分配前的最小可用内存要求 */
        
        uint32_t download_threshold = 250 * 1024;      /**< 下载最小内存要求 (字节)
                                                         *   有效范围: 200KB-800KB
                                                         *   推荐值: 250KB
                                                         *   说明: 开始下载前必须保证的最小可用内存 */
        
        uint32_t preload_threshold = 400 * 1024;       /**< 预加载最小内存要求 (字节)
                                                         *   有效范围: 300KB-1MB
                                                         *   推荐值: 400KB
                                                         *   说明: 开始预加载图片前必须保证的最小可用内存 */
        
        uint32_t buffer_pool_size = 15;                /**< 缓冲区池大小 (个数)
                                                         *   有效范围: 5-50个
                                                         *   推荐值: 15个
                                                         *   说明: 预分配的缓冲区数量，减少动态分配开销 */
        
        bool enable_memory_pool = true;                /**< 启用内存池管理
                                                         *   推荐值: true
                                                         *   说明: 使用内存池减少碎片化，提高分配效率 */
    } memory;
    
    /**
     * 文件系统配置参数
     * 控制文件路径、存储限制和文件系统行为
     */
    struct FileSystem {
        std::string base_path = "/resources";        /**< 资源文件基础路径
                                                      *   推荐值: "/resources"
                                                      *   说明: 所有资源文件的根目录 */
        
        std::string image_path = "/resources/images/"; /**< 图片文件存储路径
                                                          *   推荐值: "/resources/images/"
                                                          *   说明: 图片文件的存储目录 */
        
        std::string cache_path = "/resources/cache/"; /**< 缓存文件存储路径
                                                         *   推荐值: "/resources/cache/"
                                                         *   说明: 临时缓存文件的存储目录 */
        
        std::string logo_filename = "logo.bin";      /**< Logo文件名
                                                         *   推荐值: "logo.bin"
                                                         *   说明: 设备Logo图片的文件名 */
        
        std::string packed_filename = "packed.rgb";  /**< 打包图片文件名
                                                         *   推荐值: "packed.rgb"
                                                         *   说明: 打包后的RGB图片数据文件名 */
        
        uint32_t max_files = 30;                     /**< SPIFFS最大文件数
                                                         *   有效范围: 10-100个
                                                         *   推荐值: 30个
                                                         *   说明: 文件系统支持的最大文件数量限制 */
        
        bool format_on_mount_fail = true;            /**< 挂载失败时自动格式化
                                                         *   推荐值: true
                                                         *   说明: 文件系统挂载失败时是否自动格式化 */
    } filesystem;
    
    /**
     * 图片处理配置参数
     * 控制图片尺寸、格式和处理方式
     */
    struct Image {
        uint32_t max_image_count = 9;           /**< 最大图片数量
                                                 *   有效范围: 1-20张
                                                 *   推荐值: 9张
                                                 *   说明: 系统支持的最大图片文件数量 */
        
        uint32_t image_width = 240;             /**< 图片宽度 (像素)
                                                 *   有效范围: 128-480像素
                                                 *   推荐值: 240像素
                                                 *   说明: 处理图片的标准宽度 */
        
        uint32_t image_height = 240;            /**< 图片高度 (像素)
                                                 *   有效范围: 128-480像素
                                                 *   推荐值: 240像素
                                                 *   说明: 处理图片的标准高度 */
        
        uint32_t bytes_per_pixel = 2;           /**< 每像素字节数
                                                 *   有效值: 2 (RGB565) 或 3 (RGB888)
                                                 *   推荐值: 2 (RGB565格式)
                                                 *   说明: 图片像素格式的字节数 */
        
        bool enable_format_conversion = true;   /**< 启用格式转换
                                                 *   推荐值: true
                                                 *   说明: 是否启用图片格式自动转换功能 */
        
        bool enable_packed_loading = true;      /**< 启用打包加载
                                                 *   推荐值: true
                                                 *   说明: 是否支持打包图片文件的加载 */
    } image;
    
    /**
     * 下载模式配置参数
     * 控制下载过程中的系统优化和性能调整
     */
    struct DownloadMode {
        bool disable_power_save = true;            /**< 下载模式下禁用省电
                                                     *   推荐值: true
                                                     *   说明: 下载时禁用省电模式，确保网络稳定性 */
        
        bool pause_audio = true;                   /**< 暂停音频处理
                                                     *   推荐值: true
                                                     *   说明: 下载时暂停音频处理，释放CPU资源 */
        
        bool boost_task_priority = true;           /**< 提升任务优先级
                                                     *   推荐值: true
                                                     *   说明: 提升下载任务优先级，加快下载速度 */
        
        uint32_t gc_interval_ms = 500;             /**< 垃圾回收间隔 (毫秒)
                                                     *   有效范围: 200-2000ms
                                                     *   推荐值: 500ms
                                                     *   说明: 下载过程中内存垃圾回收的执行间隔 */
        
        uint32_t network_stabilize_ms = 300;       /**< 网络稳定等待时间 (毫秒)
                                                     *   有效范围: 100-1000ms
                                                     *   推荐值: 300ms
                                                     *   说明: 网络连接建立后的稳定等待时间 */
    } download_mode;
    
    /**
     * 预加载配置参数
     * 控制图片预加载的时机、频率和行为
     */
    struct Preload {
        uint32_t check_interval = 3;            /**< 音频状态检查间隔
                                                 *   有效范围: 1-10张图片
                                                 *   推荐值: 3张图片
                                                 *   说明: 每处理N张图片后检查一次音频状态 */
        
        uint32_t load_delay_ms = 10;            /**< 预加载延迟 (毫秒)
                                                 *   有效范围: 5-100ms
                                                 *   推荐值: 10ms
                                                 *   说明: 预加载图片之间的延迟时间 */
        
        uint32_t progress_update_threshold = 2; /**< 进度更新阈值
                                                 *   有效范围: 1-5张图片
                                                 *   推荐值: 2张图片
                                                 *   说明: 每处理N张图片后更新一次进度 */
        
        bool enable_silent_preload = true;      /**< 启用静默预加载
                                                 *   推荐值: true
                                                 *   说明: 在后台静默预加载图片，不影响用户体验 */
        
        uint32_t time_budget_ms = 0;            /**< 预加载时间预算 (毫秒)
                                                 *   有效范围: 0-5000ms (0=无限制)
                                                 *   推荐值: 0 (无限制)
                                                 *   说明: 单次预加载操作的最大时间限制 */
    } preload;
    
    /**
     * 调试配置参数
     * 控制调试功能、日志输出和性能监控
     */
    struct Debug {
        bool enable_file_verification = false;  /**< 启用文件验证
                                                 *   推荐值: false (生产环境)
                                                 *   说明: 启用文件完整性验证，仅用于调试 */
        
        bool verbose_logging = false;           /**< 详细日志输出
                                                 *   推荐值: false (生产环境)
                                                 *   说明: 输出详细的调试日志信息 */
        
        bool enable_memory_tracking = true;     /**< 启用内存跟踪
                                                 *   推荐值: true
                                                 *   说明: 跟踪内存使用情况，用于优化和调试 */
        
        bool enable_performance_metrics = false; /**< 启用性能指标
                                                  *   推荐值: false (生产环境)
                                                  *   说明: 收集和输出性能统计数据 */
    } debug;
    
    /**
     * 辅助函数：计算衍生值和生成路径
     */
    
    /**
     * 计算单张图片的内存大小
     * @return 单张图片占用的字节数
     */
    uint32_t get_image_size() const {
        return image.image_width * image.image_height * image.bytes_per_pixel;
    }
    
    /**
     * 计算所有图片的总内存大小
     * @return 所有图片占用的总字节数
     */
    uint32_t get_total_images_size() const {
        return get_image_size() * image.max_image_count;
    }
    
    /**
     * 获取Logo文件的完整路径
     * @return Logo文件的完整路径字符串
     */
    std::string get_logo_path() const {
        return filesystem.image_path + filesystem.logo_filename;
    }
    
    /**
     * 获取打包图片文件的完整路径
     * @return 打包图片文件的完整路径字符串
     */
    std::string get_packed_path() const {
        return filesystem.image_path + filesystem.packed_filename;
    }
    
    /**
     * 根据索引生成图片文件名和完整路径
     * @param index 图片索引 (0-based)
     * @return 图片文件的完整路径字符串
     */
    std::string get_image_filename(int index) const {
        char filename[64];
        snprintf(filename, sizeof(filename), "output_%04d.bin", index);
        return filesystem.image_path + filename;
    }
};

/**
 * 配置管理器
 * 单例模式管理系统配置参数的加载、保存、验证和调整
 */
class ConfigManager {
public:
    /**
     * 获取配置管理器单例实例
     * @return ConfigManager的单例引用
     */
    static ConfigManager& GetInstance();
    
    /**
     * 获取只读配置对象
     * @return 当前配置的常量引用
     */
    const ResourceConfig& get_config() const { return config_; }
    
    /**
     * 获取可修改的配置对象
     * @return 当前配置的可修改引用
     * @warning 直接修改配置后需要调用validate_config()验证
     */
    ResourceConfig& get_mutable_config() { return config_; }
    
    /**
     * 从文件加载配置参数
     * @param config_path 配置文件路径，为空时使用默认路径
     * @return 加载成功返回true，失败返回false
     * @note 加载失败时会使用默认配置
     */
    bool load_config(const std::string& config_path = "");
    
    /**
     * 保存当前配置到文件
     * @param config_path 配置文件路径，为空时使用默认路径
     * @return 保存成功返回true，失败返回false
     */
    bool save_config(const std::string& config_path = "") const;
    
    /**
     * 验证当前配置参数的有效性
     * @return 配置有效返回true，无效返回false
     * @note 会检查所有参数的有效范围和逻辑一致性
     */
    bool validate_config() const;
    
    /**
     * 重置所有配置为默认值
     * @note 会丢失当前所有自定义配置
     */
    void reset_to_defaults();
    
    /**
     * 根据设备硬件能力自动调整配置参数
     * @note 会根据可用内存、存储空间等硬件条件优化配置
     */
    void adjust_for_device();

private:
    ConfigManager() = default;  ///< 私有构造函数，确保单例模式
    ResourceConfig config_;     ///< 配置数据存储
};

} // namespace ImageResource
