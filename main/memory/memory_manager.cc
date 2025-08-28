#include "memory_manager.h"
#include <algorithm>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>

#define TAG "MemoryManager"

namespace ImageResource {

// MemoryBlock 实现
MemoryBlock::MemoryBlock(size_t size) : data_(nullptr), size_(size) {
    if (size > 0) {
        data_ = static_cast<uint8_t*>(malloc(size));
        if (data_) {
            MemoryManager::GetInstance().update_stats(size);
            ESP_LOGD(TAG, "分配内存块: %zu bytes", size);
        } else {
            ESP_LOGE(TAG, "内存分配失败: %zu bytes", size);
        }
    }
}

MemoryBlock::~MemoryBlock() {
    reset();
}

MemoryBlock::MemoryBlock(MemoryBlock&& other) noexcept 
    : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

MemoryBlock& MemoryBlock::operator=(MemoryBlock&& other) noexcept {
    if (this != &other) {
        reset();
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void MemoryBlock::reset() {
    if (data_) {
        MemoryManager::GetInstance().update_dealloc_stats(size_);
        free(data_);
        data_ = nullptr;
        size_ = 0;
        ESP_LOGD(TAG, "释放内存块: %zu bytes", size_);
    }
}

// MemoryManager 实现
MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

std::unique_ptr<MemoryBlock> MemoryManager::allocate(size_t size) {
    // 检查内存可用性
    if (!has_available_memory(size)) {
        ESP_LOGW(TAG, "内存不足，尝试垃圾回收");
        force_gc();
        
        // 再次检查
        if (!has_available_memory(size)) {
            ESP_LOGE(TAG, "垃圾回收后仍然内存不足: 需要 %zu bytes", size);
            return nullptr;
        }
    }
    
    auto block = std::make_unique<MemoryBlock>(size);
    if (!block->is_valid()) {
        ESP_LOGE(TAG, "内存块创建失败");
        return nullptr;
    }
    
    return block;
}

bool MemoryManager::has_available_memory(size_t required_size) const {
    size_t free_heap = esp_get_free_heap_size();
    size_t required_with_threshold = required_size + memory_threshold_;
    
    ESP_LOGD(TAG, "内存检查: 可用=%zu, 需要=%zu, 阈值=%zu", 
             free_heap, required_size, memory_threshold_);
    
    return free_heap >= required_with_threshold;
}

MemoryStats MemoryManager::get_stats() const {
    MemoryStats current_stats = stats_;
    current_stats.current_usage = stats_.total_allocated - 
                                 (stats_.deallocation_count * sizeof(size_t)); // 简化计算
    return current_stats;
}

void MemoryManager::force_gc() {
    ESP_LOGI(TAG, "执行强制垃圾回收");
    
    // 让出CPU时间，允许其他任务释放内存
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 触发堆压缩（如果ESP-IDF支持）
    // esp_heap_caps_check_integrity_all(true);
    
    ESP_LOGI(TAG, "垃圾回收完成，当前可用内存: %" PRIu32 " bytes", esp_get_free_heap_size());
}

void MemoryManager::update_stats(size_t allocated_size) {
    stats_.total_allocated += allocated_size;
    stats_.current_usage += allocated_size;
    stats_.allocation_count++;
    
    if (stats_.current_usage > stats_.peak_usage) {
        stats_.peak_usage = stats_.current_usage;
    }
}

void MemoryManager::update_dealloc_stats(size_t deallocated_size) {
    stats_.current_usage = (stats_.current_usage >= deallocated_size) ? 
                          (stats_.current_usage - deallocated_size) : 0;
    stats_.deallocation_count++;
}

// 内存监控实现
bool MemoryManager::is_memory_critical() const {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    
    // 多级内存危险状态判断：
    // 1. 绝对危险：小于1MB
    // 2. 相对危险：小于阈值的25%  
    // 3. 历史对比：仅当远低于历史最低点时才危险（98%阈值）
    return (free_heap < 1024 * 1024) || 
           (free_heap < memory_threshold_ / 4) ||
           (free_heap < min_free * 0.98);
}

bool MemoryManager::is_memory_warning() const {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    
    // 内存警告状态判断（比危险状态宽松）：
    // 1. 绝对警告：小于2MB
    // 2. 相对警告：小于阈值的50%
    // 3. 历史对比：当接近历史最低点时警告（102%阈值）
    return (free_heap < 2 * 1024 * 1024) || 
           (free_heap < memory_threshold_ / 2) ||
           (free_heap < min_free * 1.02);
}

MemoryStatus MemoryManager::get_memory_status() const {
    if (is_memory_critical()) {
        return MemoryStatus::CRITICAL;
    } else if (is_memory_warning()) {
        return MemoryStatus::WARNING;
    } else {
        return MemoryStatus::GOOD;
    }
}

void MemoryManager::log_memory_status() const {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    size_t fragmentation = get_heap_fragmentation_percent();
    MemoryStatus status = get_memory_status();
    
    const char* status_str = "未知";
    const char* status_emoji = "❓";
    
    switch (status) {
        case MemoryStatus::GOOD:
            status_str = "正常";
            status_emoji = "✅";
            break;
        case MemoryStatus::WARNING:
            status_str = "警告";
            status_emoji = "⚠️ ";
            break;
        case MemoryStatus::CRITICAL:
            status_str = "危险";
            status_emoji = "🆘";
            break;
    }
    
    ESP_LOGI(TAG, "=== 内存状态报告 ===");
    ESP_LOGI(TAG, "可用堆内存: %zu bytes (%.1f KB)", free_heap, free_heap / 1024.0f);
    ESP_LOGI(TAG, "历史最低内存: %zu bytes (%.1f KB)", min_free, min_free / 1024.0f);
    ESP_LOGI(TAG, "最大连续块: %zu bytes (%.1f KB)", largest_block, largest_block / 1024.0f);
    ESP_LOGI(TAG, "碎片率: %zu%%", fragmentation);
    ESP_LOGI(TAG, "内存阈值: %zu bytes (%.1f KB)", memory_threshold_, memory_threshold_ / 1024.0f);
    ESP_LOGI(TAG, "内存状态: %s %s", status_emoji, status_str);
    ESP_LOGI(TAG, "已分配次数: %zu", stats_.allocation_count);
    ESP_LOGI(TAG, "已释放次数: %zu", stats_.deallocation_count);
}

size_t MemoryManager::get_heap_fragmentation_percent() const {
    size_t free_heap = esp_get_free_heap_size();
    size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    if (free_heap == 0) return 100;
    
    // 碎片率 = (总可用内存 - 最大连续块) / 总可用内存 * 100
    size_t fragmented = (free_heap > largest_block) ? (free_heap - largest_block) : 0;
    return (fragmented * 100) / free_heap;
}

// ImageBufferPool 实现
ImageBufferPool& ImageBufferPool::GetInstance() {
    static ImageBufferPool instance;
    return instance;
}

std::unique_ptr<MemoryBlock> ImageBufferPool::acquire_buffer() {
    lock_guard<FreeRTOSMutex> lock(pool_mutex_); // 优化：FreeRTOS兼容锁
    
    if (!available_buffers_.empty()) {
        auto buffer = std::move(available_buffers_.back());
        available_buffers_.pop_back();
        ESP_LOGD(TAG, "从池中获取缓冲区，剩余: %zu", available_buffers_.size());
        return buffer;
    }
    
    // 检查是否超过总缓冲区限制
    if (total_allocated_buffers_.load() >= max_total_buffers()) {
        ESP_LOGW(TAG, "🚫 已达到缓冲区总数限制 (%zu)，无法分配新缓冲区", max_total_buffers());
        // 达到限制时触发详细监控
        ESP_LOGW(TAG, "=== 🆘 缓冲区池已满详情 ===");
        log_pool_status();
        MemoryManager::GetInstance().log_memory_status();
        return nullptr;
    }
    
    // 智能监控：当使用率超过75%时发出警告
    if (is_pool_under_pressure()) {
        ESP_LOGW(TAG, "⚠️  缓冲区池承受高压力 (使用率 %.1f%%)，建议释放未使用的资源", 
                 get_pool_utilization_percent());
    }
    
    // 池中没有可用缓冲区，动态创建新的
    ESP_LOGD(TAG, "池为空，动态创建新缓冲区 (%zu/%zu)", 
             total_allocated_buffers_.load() + 1, max_total_buffers());
    
    auto buffer = MemoryManager::GetInstance().allocate(STANDARD_IMAGE_SIZE);
    if (buffer && buffer->is_valid()) {
        total_allocated_buffers_.fetch_add(1);
    }
    
    return buffer;
}

void ImageBufferPool::release_buffer(std::unique_ptr<MemoryBlock> buffer) {
    if (!buffer || !buffer->is_valid()) {
        return;
    }
    
    lock_guard<FreeRTOSMutex> lock(pool_mutex_); // 优化：FreeRTOS兼容锁
    
    // 如果池未满，返回到池中复用
    if (available_buffers_.size() < POOL_SIZE) {
        available_buffers_.push_back(std::move(buffer));
        ESP_LOGD(TAG, "缓冲区返回池中，当前池大小: %zu", available_buffers_.size());
    } else {
        // 池已满，释放缓冲区并更新计数器
        ESP_LOGD(TAG, "池已满，直接释放缓冲区");
        total_allocated_buffers_.fetch_sub(1);
        // buffer会在作用域结束时自动释放
    }
}

bool ImageBufferPool::warm_up() {
    lock_guard<FreeRTOSMutex> lock(pool_mutex_); // 优化：FreeRTOS兼容锁
    
    ESP_LOGI(TAG, "预热图片缓冲区池（优化版：最多%zu个缓冲区，~%zuKB）", 
             POOL_SIZE, (POOL_SIZE * STANDARD_IMAGE_SIZE) / 1024);
    
    // 清空现有缓冲区
    available_buffers_.clear();
    total_allocated_buffers_.store(0);
    
    // 预分配缓冲区（现在只分配3个而不是10个）
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        auto buffer = MemoryManager::GetInstance().allocate(STANDARD_IMAGE_SIZE);
        if (buffer && buffer->is_valid()) {
            available_buffers_.push_back(std::move(buffer));
            total_allocated_buffers_.fetch_add(1);
        } else {
            ESP_LOGW(TAG, "预分配第 %zu 个缓冲区失败", i + 1);
            break;
        }
    }
    
    ESP_LOGI(TAG, "池预热完成，预分配 %zu/%zu 个缓冲区，最多可动态扩展到 %zu 个", 
             available_buffers_.size(), POOL_SIZE, max_total_buffers());
    
    return available_buffers_.size() > 0;
}

void ImageBufferPool::clear() {
    lock_guard<FreeRTOSMutex> lock(pool_mutex_); // 优化：FreeRTOS兼容锁
    size_t cleared_count = available_buffers_.size();
    available_buffers_.clear();
    total_allocated_buffers_.store(0);
    ESP_LOGI(TAG, "图片缓冲区池已清空，释放了 %zu 个缓冲区", cleared_count);
}

// ImageBufferPool 监控功能实现
float ImageBufferPool::get_pool_utilization_percent() const {
    size_t allocated = total_allocated_buffers_.load();
    size_t max_buffers = max_total_buffers();
    
    if (max_buffers == 0) return 0.0f;
    return (float(allocated) / float(max_buffers)) * 100.0f;
}

bool ImageBufferPool::is_pool_under_pressure() const {
    // 如果使用率超过80%，认为承受压力
    return get_pool_utilization_percent() > 80.0f;
}

void ImageBufferPool::log_pool_status() const {
    size_t available = available_count();
    size_t allocated = current_allocated_count();
    size_t max_buffers = max_total_buffers();
    float utilization = get_pool_utilization_percent();
    
    ESP_LOGI(TAG, "=== 图片缓冲区池状态 ===");
    ESP_LOGI(TAG, "池中可用缓冲区: %zu", available);
    ESP_LOGI(TAG, "已分配缓冲区: %zu", allocated);
    ESP_LOGI(TAG, "最大容量: %zu 个缓冲区", max_buffers);
    ESP_LOGI(TAG, "使用率: %.1f%%", utilization);
    ESP_LOGI(TAG, "内存占用: ~%.1f KB / ~%.1f KB (最大)", 
             (allocated * STANDARD_IMAGE_SIZE) / 1024.0f,
             (max_buffers * STANDARD_IMAGE_SIZE) / 1024.0f);
    ESP_LOGI(TAG, "压力状态: %s", is_pool_under_pressure() ? "高压力" : "正常");
}

} // namespace ImageResource
