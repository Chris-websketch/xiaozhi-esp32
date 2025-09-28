#ifndef LOCAL_INTENT_DETECTOR_H
#define LOCAL_INTENT_DETECTOR_H

#include <vector>
#include <string>
#include <functional>
#include <map>
#include <deque>
#include <mutex>

namespace intent {

enum class IntentType {
    UNKNOWN,
    VOLUME_CONTROL,
    BRIGHTNESS_CONTROL,
    THEME_CONTROL,
    DISPLAY_MODE_CONTROL,
    // 可扩展更多意图类型
};

struct IntentResult {
    IntentType type;
    std::string action;          // 如 "SetVolume", "TurnOn", "TurnOff"
    std::map<std::string, std::string> parameters;  // 如 {"volume": "80"}
    float confidence;            // 置信度 0.0-1.0
    std::string device_name;     // 目标设备名称
};

// 关键词检测规则
struct KeywordRule {
    std::vector<std::string> keywords;     // 关键词列表
    IntentType intent_type;                // 意图类型
    std::string action;                    // 对应的IOT动作
    std::string device;                    // 目标设备
    std::function<void(const std::string& text, IntentResult& result)> parameter_extractor; // 参数提取器
};

class LocalIntentDetector {
public:
    LocalIntentDetector();
    ~LocalIntentDetector();

    // 初始化检测器
    void Initialize();
    
    // 处理音频转文本结果（假设有STT结果）
    bool DetectIntent(const std::string& text, IntentResult& result);
    
    // 检测多个意图（用于同时调节多个参数）
    std::vector<IntentResult> DetectMultipleIntents(const std::string& text);
    
    // 设置音频数据缓冲（为将来可能的本地STT预留）
    void FeedAudioData(const std::vector<int16_t>& audio_data);
    
    // 注册意图检测成功回调
    void OnIntentDetected(std::function<void(const IntentResult& result)> callback);
    
    // 添加自定义检测规则
    void AddKeywordRule(const KeywordRule& rule);
    
    // 启用/禁用本地检测
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

private:
    bool enabled_;
    std::vector<KeywordRule> keyword_rules_;
    std::function<void(const IntentResult& result)> intent_callback_;
    std::mutex audio_buffer_mutex_;
    std::deque<std::vector<int16_t>> audio_buffer_;
    
    // 初始化默认的关键词检测规则
    void InitializeDefaultRules();
    
    // 静态数字提取工具
    static int ExtractNumberStatic(const std::string& text);
    
    // 基于上下文的数字提取（找离关键词最近的数字）
    static int ExtractNumberWithContext(const std::string& text, const std::vector<std::string>& context_keywords);
    
    // 中文数字转换
    int ChineseNumberToInt(const std::string& chinese_num);
    
    // 文本预处理（去除标点、统一大小写等）
    std::string PreprocessText(const std::string& text);
    
    // 关键词匹配
    bool MatchKeywords(const std::string& text, const std::vector<std::string>& keywords);
    
    // 音量控制参数提取器
    static void ExtractVolumeParameters(const std::string& text, IntentResult& result);
    
    // 亮度控制参数提取器
    static void ExtractBrightnessParameters(const std::string& text, IntentResult& result);
    
    // 主题控制参数提取器
    static void ExtractThemeParameters(const std::string& text, IntentResult& result);
    
    // 显示模式控制参数提取器
    static void ExtractDisplayModeParameters(const std::string& text, IntentResult& result);
};

} // namespace intent

#endif // LOCAL_INTENT_DETECTOR_H
