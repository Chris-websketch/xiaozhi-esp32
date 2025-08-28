#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "freertos_mutex.h"

namespace ImageResource {

/**
 * 内存统计信息
 */
struct MemoryStats {
    size_t total_allocated = 0;
    size_t peak_usage = 0;
    size_t current_usage = 0;
    size_t allocation_count = 0;
    size_t deallocation_count = 0;
};

/**
 * RAII内存块包装器
 */
class MemoryBlock {
public:
    explicit MemoryBlock(size_t size);
    ~MemoryBlock();
    
    // 禁止拷贝，允许移动
    MemoryBlock(const MemoryBlock&) = delete;
    MemoryBlock& operator=(const MemoryBlock&) = delete;
    MemoryBlock(MemoryBlock&& other) noexcept;
    MemoryBlock& operator=(MemoryBlock&& other) noexcept;
    
    uint8_t* data() { return data_; }
    const uint8_t* data() const { return data_; }
    size_t size() const { return size_; }
    bool is_valid() const { return data_ != nullptr; }
    
    // 释放内存（提前释放）
    void reset();

private:
    uint8_t* data_;
    size_t size_;
};

/**
 * 智能内存管理器
 */
class MemoryManager {
public:
    static MemoryManager& GetInstance();
    
    // 分配内存块
    std::unique_ptr<MemoryBlock> allocate(size_t size);
    
    // 检查内存可用性
    bool has_available_memory(size_t required_size) const;
    
    // 获取内存统计
    MemoryStats get_stats() const;
    
    // 强制垃圾回收
    void force_gc();
    
    // 设置内存使用阈值
    void set_memory_threshold(size_t threshold) { memory_threshold_ = threshold; }
    
    // 内存监控功能
    bool is_memory_critical() const; // 检查内存是否处于危险水平
    void log_memory_status() const;  // 记录当前内存状态
    size_t get_heap_fragmentation_percent() const; // 获取堆碎片率

private:
    MemoryManager() = default;
    ~MemoryManager() = default;
    
    // 禁止拷贝
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
    void update_stats(size_t allocated_size);
    void update_dealloc_stats(size_t deallocated_size);
    
    mutable MemoryStats stats_;
    size_t memory_threshold_ = 200 * 1024; // 200KB默认阈值
    
    friend class MemoryBlock;
};

/**
 * 图片缓冲区专用管理器
 */
class ImageBufferPool {
public:
    static constexpr size_t STANDARD_IMAGE_SIZE = 240 * 240 * 2; // RGB565 = ~115KB
    static constexpr size_t POOL_SIZE = 3; // 优化：仅预分配3个缓冲区（~345KB vs 1.15MB）
    static constexpr size_t MAX_DYNAMIC_BUFFERS = 7; // 最大动态分配缓冲区数
    
    static ImageBufferPool& GetInstance();
    
    // 获取图片缓冲区
    std::unique_ptr<MemoryBlock> acquire_buffer();
    
    // 返回缓冲区到池中（自动处理）
    void release_buffer(std::unique_ptr<MemoryBlock> buffer);
    
    // 预热池（预分配缓冲区）
    bool warm_up();
    
    // 清空池
    void clear();
    
    // 获取池状态
    size_t available_count() const { return available_buffers_.size(); }
    size_t total_capacity() const { return POOL_SIZE; }
    size_t max_total_buffers() const { return POOL_SIZE + MAX_DYNAMIC_BUFFERS; }
    size_t current_allocated_count() const { return total_allocated_buffers_.load(); }
    
    // 内存监控功能
    float get_pool_utilization_percent() const; // 获取池使用率
    bool is_pool_under_pressure() const;        // 检查池是否承受压力
    void log_pool_status() const;               // 记录池状态

private:
    ImageBufferPool() = default;
    ~ImageBufferPool() = default;
    
    std::vector<std::unique_ptr<MemoryBlock>> available_buffers_;
    mutable FreeRTOSMutex pool_mutex_; // 优化：FreeRTOS原生互斥锁，中断安全
    std::atomic<size_t> total_allocated_buffers_{0}; // 跟踪总分配数量
};

} // namespace ImageResource
