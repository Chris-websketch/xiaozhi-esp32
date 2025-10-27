#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ImageResource {

/**
 * 文件清理辅助工具
 * 负责文件的删除和临时文件清理
 */
class CleanupHelper {
public:
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;

    CleanupHelper() = default;
    ~CleanupHelper() = default;

    // 禁用拷贝
    CleanupHelper(const CleanupHelper&) = delete;
    CleanupHelper& operator=(const CleanupHelper&) = delete;

    /**
     * 删除文件列表
     * @param files 文件路径列表
     * @param callback 进度回调
     * @return true成功，false失败
     */
    bool DeleteFiles(const std::vector<std::string>& files, ProgressCallback callback = nullptr);

    /**
     * 清理临时文件
     * @param directory 目录路径
     * @return 清理的文件数量
     */
    int CleanupTemporary(const char* directory);

    /**
     * 清理所有图片文件
     * @param base_path 图片基础路径
     * @param max_files 最大文件数
     * @return true成功，false失败
     */
    bool ClearAllImages(const char* base_path, int max_files);

    /**
     * 删除动画图片文件
     * @param base_path 图片基础路径
     * @param max_files 最大文件数
     * @param callback 进度回调
     * @return true成功，false失败
     */
    bool DeleteAnimationFiles(const char* base_path, int max_files, ProgressCallback callback = nullptr);

    /**
     * 删除Logo文件
     * @param logo_bin_path logo二进制文件路径
     * @param logo_h_path logo .h文件路径
     * @param callback 进度回调
     * @return true成功，false失败
     */
    bool DeleteLogoFile(const char* logo_bin_path, const char* logo_h_path, ProgressCallback callback = nullptr);

    /**
     * 删除表情包文件
     * @param base_path 表情包基础路径
     * @param filenames 文件名列表
     * @param file_count 文件数量
     * @param callback 进度回调
     * @return true成功，false失败
     */
    bool DeleteEmoticonFiles(const char* base_path, const char** filenames, int file_count, ProgressCallback callback = nullptr);
};

} // namespace ImageResource
