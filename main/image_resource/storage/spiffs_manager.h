#pragma once

#include <esp_err.h>
#include <stddef.h>

namespace ImageResource {

/**
 * 文件系统分区管理器（LittleFS）
 * 负责分区的挂载、卸载、格式化和空间管理
 * 注意：已从SPIFFS迁移到LittleFS以获得更好的性能和碎片管理
 */
class SpiffsManager {
public:
    SpiffsManager() = default;
    ~SpiffsManager() = default;

    // 禁用拷贝
    SpiffsManager(const SpiffsManager&) = delete;
    SpiffsManager& operator=(const SpiffsManager&) = delete;

    /**
     * 挂载LittleFS分区
     * @param partition_label 分区标签
     * @param mount_point 挂载点路径
     * @param max_files 最大文件数
     * @param format_if_failed 失败时是否格式化
     * @return ESP_OK成功，其他失败
     */
    esp_err_t Mount(const char* partition_label, 
                   const char* mount_point,
                   size_t max_files = 30,
                   bool format_if_failed = true);

    /**
     * 卸载LittleFS分区
     * @param mount_point 挂载点路径
     * @return ESP_OK成功，其他失败
     */
    esp_err_t Unmount(const char* mount_point);

    /**
     * 格式化LittleFS分区
     * @param partition_label 分区标签
     * @return ESP_OK成功，其他失败
     */
    esp_err_t Format(const char* partition_label);

    /**
     * 获取可用空间
     * @param partition_label 分区标签
     * @return 可用空间大小（字节）
     */
    size_t GetFreeSpace(const char* partition_label);

    /**
     * 优化文件系统空间（LittleFS自动管理，保留接口兼容性）
     * @param partition_label 分区标签
     * @return true成功，false失败
     */
    bool OptimizeSpace(const char* partition_label);

    /**
     * 创建目录（递归创建）
     * @param path 目录路径
     * @return true成功，false失败
     */
    bool CreateDirectory(const char* path);

    /**
     * 检查分区是否已挂载
     */
    bool IsMounted() const { return mounted_; }

private:
    bool mounted_ = false;
};

} // namespace ImageResource
