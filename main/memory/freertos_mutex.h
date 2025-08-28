#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>

namespace ImageResource {

/**
 * FreeRTOS兼容的互斥锁包装器
 * 
 * 优势：
 * 1. 中断安全
 * 2. 优先级继承
 * 3. 更低的内存占用
 * 4. 与FreeRTOS调度器完全兼容
 */
class FreeRTOSMutex {
public:
    FreeRTOSMutex() {
        mutex_handle_ = xSemaphoreCreateMutex();
        if (!mutex_handle_) {
            ESP_LOGE("FreeRTOSMutex", "Failed to create mutex");
        }
    }
    
    ~FreeRTOSMutex() {
        if (mutex_handle_) {
            vSemaphoreDelete(mutex_handle_);
        }
    }
    
    // 禁止拷贝和移动
    FreeRTOSMutex(const FreeRTOSMutex&) = delete;
    FreeRTOSMutex& operator=(const FreeRTOSMutex&) = delete;
    FreeRTOSMutex(FreeRTOSMutex&&) = delete;
    FreeRTOSMutex& operator=(FreeRTOSMutex&&) = delete;
    
    void lock() {
        if (mutex_handle_) {
            xSemaphoreTake(mutex_handle_, portMAX_DELAY);
        }
    }
    
    bool try_lock() {
        if (!mutex_handle_) return false;
        return xSemaphoreTake(mutex_handle_, 0) == pdTRUE;
    }
    
    void unlock() {
        if (mutex_handle_) {
            xSemaphoreGive(mutex_handle_);
        }
    }
    
    // 带超时的锁定
    bool try_lock_for(uint32_t timeout_ms) {
        if (!mutex_handle_) return false;
        TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
        return xSemaphoreTake(mutex_handle_, timeout_ticks) == pdTRUE;
    }
    
    // 检查互斥锁是否有效
    bool is_valid() const { return mutex_handle_ != nullptr; }

private:
    SemaphoreHandle_t mutex_handle_;
};

/**
 * RAII锁守护，兼容std::lock_guard接口
 */
template<typename Mutex>
class lock_guard {
public:
    explicit lock_guard(Mutex& m) : mutex_(m) {
        mutex_.lock();
    }
    
    ~lock_guard() {
        mutex_.unlock();
    }
    
    // 禁止拷贝和移动
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;

private:
    Mutex& mutex_;
};

/**
 * 唯一锁，兼容std::unique_lock部分接口
 */
template<typename Mutex>
class unique_lock {
public:
    explicit unique_lock(Mutex& m) : mutex_(m), locked_(false) {
        lock();
    }
    
    ~unique_lock() {
        if (locked_) {
            unlock();
        }
    }
    
    void lock() {
        if (!locked_) {
            mutex_.lock();
            locked_ = true;
        }
    }
    
    void unlock() {
        if (locked_) {
            mutex_.unlock();
            locked_ = false;
        }
    }
    
    bool try_lock() {
        if (!locked_) {
            locked_ = mutex_.try_lock();
        }
        return locked_;
    }
    
    // 禁止拷贝，允许移动
    unique_lock(const unique_lock&) = delete;
    unique_lock& operator=(const unique_lock&) = delete;
    
    unique_lock(unique_lock&& other) noexcept 
        : mutex_(other.mutex_), locked_(other.locked_) {
        other.locked_ = false;
    }

private:
    Mutex& mutex_;
    bool locked_;
};

} // namespace ImageResource
