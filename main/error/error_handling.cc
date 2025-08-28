#include "error_handling.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <map>
#include <inttypes.h>

#define TAG "ErrorHandler"

namespace ImageResource {

// RetryStrategy 实现
Result<void> RetryStrategy::recover(const ErrorInfo& error) {
    if (current_retries_ >= max_retries_) {
        ESP_LOGE(TAG, "重试次数已达上限: %d", max_retries_);
        return Result<void>::error(error);
    }
    
    current_retries_++;
    ESP_LOGW(TAG, "执行重试 %d/%d，延迟 %d ms", current_retries_, max_retries_, delay_ms_);
    
    if (delay_ms_ > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms_));
    }
    
    return Result<void>::success();
}

bool RetryStrategy::can_handle(ErrorCode code) const {
    // 可重试的错误类型
    switch (code) {
        case ErrorCode::NetworkTimeout:
        case ErrorCode::NetworkConnectionFailed:
        case ErrorCode::FileWriteFailed:
        case ErrorCode::FileReadFailed:
            return true;
        default:
            return false;
    }
}

// ErrorHandler 实现
ErrorHandler& ErrorHandler::GetInstance() {
    static ErrorHandler instance;
    return instance;
}

void ErrorHandler::register_strategy(ErrorCode code, std::unique_ptr<ErrorRecoveryStrategy> strategy) {
    strategies_[code] = std::move(strategy);
    ESP_LOGI(TAG, "已注册错误恢复策略: 错误码 %d", static_cast<int>(code));
}

Result<void> ErrorHandler::handle_error(const ErrorInfo& error) {
    log_error(error);
    
    auto it = strategies_.find(error.code);
    if (it != strategies_.end()) {
        ESP_LOGI(TAG, "找到恢复策略，尝试恢复");
        return it->second->recover(error);
    }
    
    ESP_LOGW(TAG, "未找到适用的恢复策略");
    return Result<void>::error(error);
}

void ErrorHandler::log_error(const ErrorInfo& error) {
    ESP_LOGE(TAG, "错误发生:");
    ESP_LOGE(TAG, "  代码: %d (%s)", static_cast<int>(error.code), get_error_description(error.code).c_str());
    ESP_LOGE(TAG, "  消息: %s", error.message);  // 优化：直接使用const char*
    ESP_LOGE(TAG, "  上下文: %s", error.context); // 优化：直接使用const char*
    ESP_LOGE(TAG, "  上下文数据: %" PRIu32, error.context_data); // 新增：显示数值上下文（使用PRIu32确保类型匹配）
}

ErrorCode ErrorHandler::from_esp_err(esp_err_t esp_error) {
    switch (esp_error) {
        case ESP_OK:
            return ErrorCode::Success;
        case ESP_ERR_INVALID_ARG:
            return ErrorCode::InvalidArgument;
        case ESP_ERR_INVALID_STATE:
            return ErrorCode::InvalidState;
        case ESP_ERR_NO_MEM:
            return ErrorCode::NoMemory;
        case ESP_ERR_NOT_FOUND:
            return ErrorCode::NotFound;
        case ESP_ERR_TIMEOUT:
            return ErrorCode::Timeout;
        default:
            return ErrorCode::Unknown;
    }
}

std::string ErrorHandler::get_error_description(ErrorCode code) {
    static const std::map<ErrorCode, std::string> descriptions = {
        {ErrorCode::Success, "成功"},
        {ErrorCode::InvalidArgument, "无效参数"},
        {ErrorCode::InvalidState, "无效状态"},
        {ErrorCode::NoMemory, "内存不足"},
        {ErrorCode::NotFound, "未找到"},
        {ErrorCode::Timeout, "超时"},
        
        {ErrorCode::NetworkNotConnected, "网络未连接"},
        {ErrorCode::NetworkTimeout, "网络超时"},
        {ErrorCode::NetworkConnectionFailed, "网络连接失败"},
        {ErrorCode::NetworkDataCorrupted, "网络数据损坏"},
        
        {ErrorCode::FileSystemNotMounted, "文件系统未挂载"},
        {ErrorCode::FileNotFound, "文件未找到"},
        {ErrorCode::FileCorrupted, "文件损坏"},
        {ErrorCode::FileWriteFailed, "文件写入失败"},
        {ErrorCode::FileReadFailed, "文件读取失败"},
        {ErrorCode::InsufficientSpace, "存储空间不足"},
        
        {ErrorCode::ImageFormatUnsupported, "不支持的图片格式"},
        {ErrorCode::ImageSizeInvalid, "图片尺寸无效"},
        {ErrorCode::ImageDataCorrupted, "图片数据损坏"},
        {ErrorCode::ImageLoadFailed, "图片加载失败"},
        
        {ErrorCode::ResourceNotInitialized, "资源未初始化"},
        {ErrorCode::ResourceAlreadyExists, "资源已存在"},
        {ErrorCode::ResourceVersionMismatch, "资源版本不匹配"},
        {ErrorCode::ResourceQuotaExceeded, "资源配额超限"},
        
        {ErrorCode::MemoryAllocationFailed, "内存分配失败"},
        {ErrorCode::MemoryCorrupted, "内存损坏"},
        {ErrorCode::MemoryLeakDetected, "检测到内存泄漏"},
        
        {ErrorCode::Unknown, "未知错误"}
    };
    
    auto it = descriptions.find(code);
    return (it != descriptions.end()) ? it->second : "未定义错误";
}

} // namespace ImageResource
