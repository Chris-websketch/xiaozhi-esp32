#pragma once

#include <string>
#include <vector>

namespace ImageResource {

/**
 * URL缓存管理器
 * 负责URL缓存的读写
 */
class CacheManager {
public:
    CacheManager() = default;
    ~CacheManager() = default;

    // 禁用拷贝
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    /**
     * 读取动态图片URL列表
     * @param cache_file 缓存文件路径
     * @return URL列表
     */
    std::vector<std::string> ReadDynamicUrls(const char* cache_file);

    /**
     * 读取静态图片URL
     * @param cache_file 缓存文件路径
     * @return URL字符串
     */
    std::string ReadStaticUrl(const char* cache_file);

    /**
     * 保存动态图片URL列表
     * @param urls URL列表
     * @param cache_file 缓存文件路径
     * @return true成功，false失败
     */
    bool SaveDynamicUrls(const std::vector<std::string>& urls, const char* cache_file);

    /**
     * 保存静态图片URL
     * @param url URL字符串
     * @param cache_file 缓存文件路径
     * @return true成功，false失败
     */
    bool SaveStaticUrl(const std::string& url, const char* cache_file);
};

} // namespace ImageResource
