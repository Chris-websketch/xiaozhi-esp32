#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include <functional>
#include <esp_err.h>

// 前向声明
namespace ImageResource {
    struct ResourceConfig;
}

namespace ImageResource {

/**
 * 打包文件加载器
 * 负责构建和加载打包文件以加速启动
 */
class PackedLoader {
public:
    using ProgressCallback = std::function<void(int current, int total, const char* message)>;

    explicit PackedLoader(const ResourceConfig* config);
    ~PackedLoader() = default;

    // 禁用拷贝
    PackedLoader(const PackedLoader&) = delete;
    PackedLoader& operator=(const PackedLoader&) = delete;

    /**
     * 构建打包文件
     * @param source_files 源文件路径列表
     * @param packed_file 打包文件路径
     * @param frame_size 每帧大小
     * @param callback 进度回调
     * @return true成功，false失败
     */
    bool BuildPacked(const std::vector<std::string>& source_files,
                    const char* packed_file,
                    size_t frame_size,
                    ProgressCallback callback = nullptr);

    /**
     * 加载打包文件
     * @param packed_file 打包文件路径
     * @param frame_size 每帧大小
     * @param frame_count 帧数量
     * @param out_buffers 输出缓冲区列表
     * @return true成功，false失败
     */
    bool LoadPacked(const char* packed_file,
                   size_t frame_size,
                   int frame_count,
                   std::vector<uint8_t*>& out_buffers);

private:
    const ResourceConfig* config_;
};

} // namespace ImageResource
