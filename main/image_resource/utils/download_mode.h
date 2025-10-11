#pragma once

namespace ImageResource {

/**
 * 下载模式管理器
 * 负责进入/退出下载模式，优化系统资源
 */
class DownloadMode {
public:
    DownloadMode() = default;
    ~DownloadMode();

    // 禁用拷贝
    DownloadMode(const DownloadMode&) = delete;
    DownloadMode& operator=(const DownloadMode&) = delete;

    /**
     * 进入下载模式
     * 禁用省电、暂停音频、提高任务优先级
     */
    void Enter();

    /**
     * 退出下载模式
     * 恢复正常运行状态
     */
    void Exit();

    /**
     * 检查是否处于下载模式
     */
    bool IsActive() const { return is_active_; }

private:
    bool is_active_ = false;
};

} // namespace ImageResource
