#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <string>
#include <functional>
#include <memory>
#include <map>
#include <type_traits>

namespace ImageResource {

/**
 * 扩展的错误码枚举
 */
enum class ErrorCode : int {
    // 成功
    Success = ESP_OK,
    
    // 通用错误
    InvalidArgument = ESP_ERR_INVALID_ARG,
    InvalidState = ESP_ERR_INVALID_STATE,
    NoMemory = ESP_ERR_NO_MEM,
    NotFound = ESP_ERR_NOT_FOUND,
    Timeout = ESP_ERR_TIMEOUT,
    
    // 网络相关错误
    NetworkNotConnected = 0x1001,
    NetworkTimeout = 0x1002,
    NetworkConnectionFailed = 0x1003,
    NetworkDataCorrupted = 0x1004,
    
    // 文件系统错误
    FileSystemNotMounted = 0x2001,
    FileNotFound = 0x2002,
    FileCorrupted = 0x2003,
    FileWriteFailed = 0x2004,
    FileReadFailed = 0x2005,
    InsufficientSpace = 0x2006,
    
    // 图片处理错误
    ImageFormatUnsupported = 0x3001,
    ImageSizeInvalid = 0x3002,
    ImageDataCorrupted = 0x3003,
    ImageLoadFailed = 0x3004,
    
    // 资源管理错误
    ResourceNotInitialized = 0x4001,
    ResourceAlreadyExists = 0x4002,
    ResourceVersionMismatch = 0x4003,
    ResourceQuotaExceeded = 0x4004,
    
    // 内存相关错误
    MemoryAllocationFailed = 0x5001,
    MemoryCorrupted = 0x5002,
    MemoryLeakDetected = 0x5003,
    
    // 未知错误
    Unknown = 0x9999
};

/**
 * 预定义错误消息常量（存储在ROM中，节省RAM）
 */
namespace ErrorMessages {
    // 通用错误
    constexpr const char* INVALID_ARGUMENT = "Invalid argument provided";
    constexpr const char* INVALID_STATE = "Invalid operation state";
    constexpr const char* NO_MEMORY = "Insufficient memory available";
    constexpr const char* NOT_FOUND = "Resource not found";
    constexpr const char* TIMEOUT = "Operation timeout";
    
    // 网络错误
    constexpr const char* NETWORK_NOT_CONNECTED = "Network not connected";
    constexpr const char* NETWORK_TIMEOUT = "Network operation timeout";
    constexpr const char* NETWORK_CONNECTION_FAILED = "Failed to establish network connection";
    constexpr const char* NETWORK_DATA_CORRUPTED = "Network data corrupted";
    
    // 文件系统错误
    constexpr const char* FILE_NOT_FOUND = "File not found";
    constexpr const char* FILE_CORRUPTED = "File data corrupted";
    constexpr const char* FILE_WRITE_FAILED = "File write operation failed";
    constexpr const char* FILE_READ_FAILED = "File read operation failed";
    constexpr const char* INSUFFICIENT_SPACE = "Insufficient storage space";
    
    // 内存错误
    constexpr const char* MEMORY_ALLOCATION_FAILED = "Memory allocation failed";
    constexpr const char* MEMORY_CORRUPTED = "Memory corruption detected";
    
    // 上下文常量
    constexpr const char* CONTEXT_DOWNLOAD = "Download operation";
    constexpr const char* CONTEXT_FILE_IO = "File I/O operation";
    constexpr const char* CONTEXT_MEMORY_MGMT = "Memory management";
    constexpr const char* CONTEXT_NETWORK = "Network operation";
}

/**
 * 轻量级错误信息结构
 * 优化：使用字符串常量替代动态字符串，内存占用从~100字节降至~16字节
 */
struct ErrorInfo {
    ErrorCode code;
    const char* message;        // 使用字符串常量（4/8字节指针）
    const char* context;        // 使用字符串常量（4/8字节指针）
    uint32_t context_data;      // 简化上下文为数值数据（4字节）
    
    ErrorInfo(ErrorCode c = ErrorCode::Success, 
              const char* msg = "",
              const char* ctx = "",
              uint32_t ctx_data = 0)
        : code(c), message(msg), context(ctx), context_data(ctx_data) {}
    
    // 兼容性构造函数（临时支持std::string参数）
    ErrorInfo(ErrorCode c, const std::string& msg, const std::string& ctx = "", uint32_t ctx_data = 0)
        : code(c), message(msg.c_str()), context(ctx.c_str()), context_data(ctx_data) {}
};

/**
 * 结果类型 - 类似Rust的Result<T, E>
 */
template<typename T>
class Result {
public:
    // 成功构造
    static Result success(T&& value) {
        return Result(std::move(value));
    }
    
    static Result success(const T& value) {
        return Result(value);
    }
    
    // 错误构造
    static Result error(const ErrorInfo& error) {
        return Result(error);
    }
    
    // 优化：使用字符串常量的轻量级错误构造
    static Result error(ErrorCode code, const char* message = "",
                       const char* context = "", uint32_t context_data = 0) {
        return Result(ErrorInfo(code, message, context, context_data));
    }
    
    // 兼容性：保持std::string版本（但建议使用上面的版本）
    static Result error(ErrorCode code, const std::string& message,
                       const std::string& context = "") {
        return Result(ErrorInfo(code, message, context));
    }
    
    // 检查是否成功
    bool is_success() const { return has_value_; }
    bool is_error() const { return !has_value_; }
    
    // 获取值（仅当成功时）
    const T& value() const {
        if (!has_value_) {
            ESP_LOGE("Result", "尝试从错误结果中获取值");
        }
        return value_;
    }
    
    T& value() {
        if (!has_value_) {
            ESP_LOGE("Result", "尝试从错误结果中获取值");
        }
        return value_;
    }
    
    // 获取错误信息（仅当失败时）
    const ErrorInfo& error() const {
        if (has_value_) {
            static ErrorInfo empty_error;
            return empty_error;
        }
        return error_;
    }
    
    // 安全获取值（带默认值）
    T value_or(const T& default_value) const {
        return has_value_ ? value_ : default_value;
    }
    
    // 链式操作（简化版本，避免复杂模板推导）
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        if (is_error()) {
            return Result<decltype(func(std::declval<T>()))>::error(error_);
        }
        return Result<decltype(func(std::declval<T>()))>::success(func(value_));
    }
    
    // 错误恢复
    template<typename F>
    Result recover(F&& recovery_func) {
        if (is_error()) {
            return recovery_func(error_);
        }
        return *this;
    }

private:
    explicit Result(T&& val) : value_(std::move(val)), has_value_(true) {}
    explicit Result(const T& val) : value_(val), has_value_(true) {}
    explicit Result(const ErrorInfo& err) : error_(err), has_value_(false) {}
    
    T value_;
    ErrorInfo error_;
    bool has_value_;
};

/**
 * void特化的Result
 */
template<>
class Result<void> {
public:
    static Result success() {
        return Result(true);
    }
    
    static Result error(const ErrorInfo& error) {
        return Result(error);
    }
    
    // 优化：使用字符串常量的轻量级错误构造
    static Result error(ErrorCode code, const char* message = "",
                       const char* context = "", uint32_t context_data = 0) {
        return Result(ErrorInfo(code, message, context, context_data));
    }
    
    // 兼容性：保持std::string版本（但建议使用上面的版本）
    static Result error(ErrorCode code, const std::string& message = "",
                       const std::string& context = "") {
        return Result(ErrorInfo(code, message, context));
    }
    
    bool is_success() const { return is_success_; }
    bool is_error() const { return !is_success_; }
    
    const ErrorInfo& error() const {
        static ErrorInfo empty_error;
        return is_success_ ? empty_error : error_;
    }

private:
    explicit Result(bool success) : is_success_(success) {}
    explicit Result(const ErrorInfo& err) : error_(err), is_success_(false) {}
    
    ErrorInfo error_;
    bool is_success_;
};

/**
 * 错误恢复策略
 */
class ErrorRecoveryStrategy {
public:
    virtual ~ErrorRecoveryStrategy() = default;
    virtual Result<void> recover(const ErrorInfo& error) = 0;
    virtual bool can_handle(ErrorCode code) const = 0;
};

/**
 * 重试策略
 */
class RetryStrategy : public ErrorRecoveryStrategy {
public:
    explicit RetryStrategy(int max_retries = 3, int delay_ms = 1000)
        : max_retries_(max_retries), delay_ms_(delay_ms), current_retries_(0) {}
    
    Result<void> recover(const ErrorInfo& error) override;
    bool can_handle(ErrorCode code) const override;
    
    void reset() { current_retries_ = 0; }

private:
    int max_retries_;
    int delay_ms_;
    int current_retries_;
};

/**
 * 错误处理器
 */
class ErrorHandler {
public:
    static ErrorHandler& GetInstance();
    
    // 注册恢复策略
    void register_strategy(ErrorCode code, std::unique_ptr<ErrorRecoveryStrategy> strategy);
    
    // 处理错误
    Result<void> handle_error(const ErrorInfo& error);
    
    // 记录错误
    void log_error(const ErrorInfo& error);
    
    // 转换esp_err_t到ErrorCode
    static ErrorCode from_esp_err(esp_err_t esp_error);
    
    // 获取错误描述
    static std::string get_error_description(ErrorCode code);

private:
    ErrorHandler() = default;
    std::map<ErrorCode, std::unique_ptr<ErrorRecoveryStrategy>> strategies_;
};

/**
 * 便利宏定义
 */
#define TRY_RESULT(expr) \
    do { \
        auto result = (expr); \
        if (result.is_error()) { \
            return Result<decltype(result.value())>::error(result.error()); \
        } \
    } while(0)

#define RETURN_IF_ERROR(expr) \
    do { \
        auto result = (expr); \
        if (result.is_error()) { \
            return result; \
        } \
    } while(0)

// 优化版本：使用预定义错误消息（推荐）
#define LOG_AND_RETURN_ERROR_LITE(code, predefined_msg) \
    do { \
        ErrorInfo error(code, predefined_msg, __FUNCTION__, __LINE__); \
        ErrorHandler::GetInstance().log_error(error); \
        return Result<void>::error(error); \
    } while(0)

// 兼容版本：支持动态错误消息（向后兼容）
#define LOG_AND_RETURN_ERROR(code, message) \
    do { \
        ErrorInfo error(code, message, __FUNCTION__); \
        ErrorHandler::GetInstance().log_error(error); \
        return Result<void>::error(error); \
    } while(0)

// 便利宏：常用错误的快捷方式
#define RETURN_NO_MEMORY() LOG_AND_RETURN_ERROR_LITE(ErrorCode::NoMemory, ErrorMessages::NO_MEMORY)
#define RETURN_INVALID_ARG() LOG_AND_RETURN_ERROR_LITE(ErrorCode::InvalidArgument, ErrorMessages::INVALID_ARGUMENT)
#define RETURN_NOT_FOUND() LOG_AND_RETURN_ERROR_LITE(ErrorCode::NotFound, ErrorMessages::NOT_FOUND)
#define RETURN_NETWORK_ERROR() LOG_AND_RETURN_ERROR_LITE(ErrorCode::NetworkConnectionFailed, ErrorMessages::NETWORK_CONNECTION_FAILED)

} // namespace ImageResource
