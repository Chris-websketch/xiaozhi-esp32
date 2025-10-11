#pragma once

#include <esp_err.h>
#include <functional>
#include <atomic>

// 前向声明
namespace ImageResource {
    struct ResourceConfig;
}

namespace ImageResource {

/**
 * 预加载管理器
 * 负责图片的延迟加载和批量预加载
 */
class PreloadManager {
public:
    using LoadCallback = std::function<bool(int)>;  // 加载指定索引的回调
    using CheckCallback = std::function<bool(int)>; // 检查是否已加载的回调
    using ProgressCallback = std::function<void(int, int, const char*)>;

    explicit PreloadManager(const ResourceConfig* config);
    ~PreloadManager() = default;

    // 禁用拷贝
    PreloadManager(const PreloadManager&) = delete;
    PreloadManager& operator=(const PreloadManager&) = delete;

    /**
     * 预加载剩余图片
     * @param load_cb 加载回调
     * @param check_cb 检查回调
     * @param total_images 总图片数
     * @param progress_cb 进度回调
     * @return ESP_OK成功，其他失败
     */
    esp_err_t PreloadRemaining(LoadCallback load_cb,
                              CheckCallback check_cb,
                              int total_images,
                              ProgressCallback progress_cb = nullptr);

    /**
     * 静默预加载（不触发UI回调）
     * @param load_cb 加载回调
     * @param check_cb 检查回调
     * @param total_images 总图片数
     * @param time_budget_ms 时间预算（毫秒，0表示不限时）
     * @return ESP_OK成功，其他失败
     */
    esp_err_t PreloadSilent(LoadCallback load_cb,
                           CheckCallback check_cb,
                           int total_images,
                           unsigned long time_budget_ms);

    /**
     * 取消预加载
     */
    void Cancel();

    /**
     * 检查是否正在预加载
     */
    bool IsPreloading() const;

    /**
     * 等待预加载完成
     * @param timeout_ms 超时时间（毫秒，0表示不等待）
     * @return true完成，false超时
     */
    bool WaitForFinish(unsigned long timeout_ms);

private:
    esp_err_t PreloadImpl(bool silent,
                         LoadCallback load_cb,
                         CheckCallback check_cb,
                         int total_images,
                         unsigned long time_budget_ms,
                         ProgressCallback progress_cb);

    const ResourceConfig* config_;
    std::atomic<bool> cancel_preload_{false};
    std::atomic<bool> is_preloading_{false};
};

} // namespace ImageResource
