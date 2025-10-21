#include "local_intent_detector.h"
#include "board.h"
#include "display.h"
#include <esp_log.h>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <vector>
#include <cmath>
#include <climits>
#include <limits>

static const char* TAG = "LocalIntentDetector";

namespace intent {

LocalIntentDetector::LocalIntentDetector() 
    : enabled_(true) {
}

LocalIntentDetector::~LocalIntentDetector() {
}

void LocalIntentDetector::Initialize() {
    ESP_LOGI(TAG, "初始化本地意图检测器");
    InitializeDefaultRules();
}

void LocalIntentDetector::InitializeDefaultRules() {
    // 屏幕亮度控制规则 - 放在前面，优先匹配更具体的关键词
    KeywordRule brightness_rule;
    brightness_rule.keywords = {
        "亮度", "屏幕亮度", "调亮", "调暗", "屏幕", "亮点", "暗点",
        "brightness", "screen brightness", "调节亮度", "设置亮度",
        "屏幕调", "亮度调", "变亮", "变暗", "屏幕亮度调",
        // 添加特殊情况关键词
        "亮度最大", "亮度最小", "亮度大一点", "亮度小一点", "屏幕亮一点", "屏幕暗一点",
        "亮度调到最大", "亮度调到最小", "屏幕调到最大", "屏幕调到最小",
        "最亮", "最暗", "调到最亮", "调到最暗"
    };
    brightness_rule.intent_type = IntentType::BRIGHTNESS_CONTROL;
    brightness_rule.action = "SetBrightness";
    brightness_rule.device = "Screen";
    brightness_rule.parameter_extractor = ExtractBrightnessParameters;
    AddKeywordRule(brightness_rule);
    
    // 音量控制规则 - 移除通用关键词，避免与亮度冲突
    KeywordRule volume_rule;
    volume_rule.keywords = {
        "音量", "声音", "大声", "小声", "调节音量", "设置音量", "音量调",
        "volume", "sound", "音量调到", "音量设为", "音量调成", "音量变成",
        // 添加特殊情况关键词
        "音量最大", "音量最小", "音量大一点", "音量小一点", "声音大一点", "声音小一点",
        "音量调到最大", "音量调到最小", "声音调到最大", "声音调到最小",
        "最响", "静音", "调到最响", "调到静音"
    };
    volume_rule.intent_type = IntentType::VOLUME_CONTROL;
    volume_rule.action = "SetVolume";
    volume_rule.device = "Speaker";
    volume_rule.parameter_extractor = ExtractVolumeParameters;
    AddKeywordRule(volume_rule);
    
    // 主题控制规则 - 精简关键词，避免冲突
    KeywordRule theme_rule;
    theme_rule.keywords = {
        "白色主题", "黑色主题",
        "白天模式", "黑夜模式", 
        "白色字体", "黑色字体",
        "白色字幕", "黑色字幕"
    };
    theme_rule.intent_type = IntentType::THEME_CONTROL;
    theme_rule.action = "SetTheme";
    theme_rule.device = "Screen";
    theme_rule.parameter_extractor = ExtractThemeParameters;
    AddKeywordRule(theme_rule);
    
    // 显示模式控制规则
    KeywordRule display_mode_rule;
    display_mode_rule.keywords = {
        "静态模式", "动态模式",
        "静态壁纸", "动态壁纸",
        "静态皮肤", "动态皮肤",
        "表情包模式", "表情模式",
        "情绪模式", "切换到表情包",
        "表情包", "emoji模式"
    };
    display_mode_rule.intent_type = IntentType::DISPLAY_MODE_CONTROL;
    display_mode_rule.action = "SetAnimatedMode"; // 默认动作，会在参数提取器中调整
    display_mode_rule.device = "ImageDisplay";
    display_mode_rule.parameter_extractor = ExtractDisplayModeParameters;
    AddKeywordRule(display_mode_rule);
    
    // 字幕控制规则
    KeywordRule subtitle_rule;
    subtitle_rule.keywords = {
        "打开字幕", "开启字幕", "显示字幕",
        "关闭字幕", "隐藏字幕", "关掉字幕"
    };
    subtitle_rule.intent_type = IntentType::SUBTITLE_CONTROL;
    subtitle_rule.action = "ToggleSubtitle";
    subtitle_rule.device = "SubtitleControl";
    subtitle_rule.parameter_extractor = ExtractSubtitleParameters;
    AddKeywordRule(subtitle_rule);
    
    ESP_LOGI(TAG, "已加载 %d 个默认检测规则", keyword_rules_.size());
}

bool LocalIntentDetector::DetectIntent(const std::string& text, IntentResult& result) {
    if (!enabled_ || text.empty()) {
        return false;
    }
    
    std::string processed_text = PreprocessText(text);
    ESP_LOGD(TAG, "检测意图: %s -> %s", text.c_str(), processed_text.c_str());
    
    // 优先级匹配：首先检查是否包含特定设备关键词
    bool has_brightness_context = (text.find("亮度") != std::string::npos || 
                                   text.find("brightness") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || 
                              text.find("声音") != std::string::npos ||
                              text.find("volume") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos ||
                             text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos ||
                                    text.find("壁纸") != std::string::npos ||
                                    text.find("皮肤") != std::string::npos ||
                                    text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
    
    for (const auto& rule : keyword_rules_) {
        if (MatchKeywords(processed_text, rule.keywords)) {
            // 上下文验证：避免错误匹配
            if (rule.intent_type == IntentType::BRIGHTNESS_CONTROL && has_volume_context && !has_brightness_context) {
                ESP_LOGD(TAG, "跳过亮度规则：检测到音量上下文");
                continue;
            }
            if (rule.intent_type == IntentType::VOLUME_CONTROL && has_brightness_context && !has_volume_context) {
                ESP_LOGD(TAG, "跳过音量规则：检测到亮度上下文");
                continue;
            }
            if (rule.intent_type == IntentType::THEME_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_theme_context) {
                ESP_LOGD(TAG, "跳过主题规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::DISPLAY_MODE_CONTROL && (has_volume_context || has_brightness_context || has_theme_context) && !has_display_mode_context) {
                ESP_LOGD(TAG, "跳过显示模式规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            
            result.type = rule.intent_type;
            result.action = rule.action;
            result.device_name = rule.device;
            result.confidence = 0.9f; // 基础置信度
            
            // 调用参数提取器
            if (rule.parameter_extractor) {
                rule.parameter_extractor(processed_text, result);
            }
            
            ESP_LOGI(TAG, "检测到意图: %s.%s, 置信度: %.2f", 
                     result.device_name.c_str(), result.action.c_str(), result.confidence);
            
            if (intent_callback_) {
                intent_callback_(result);
            }
            return true;
        }
    }
    
    return false;
}

std::vector<IntentResult> LocalIntentDetector::DetectMultipleIntents(const std::string& text) {
    std::vector<IntentResult> results;
    
    if (!enabled_ || text.empty()) {
        return results;
    }
    
    std::string processed_text = PreprocessText(text);
    ESP_LOGD(TAG, "检测多个意图: %s -> %s", text.c_str(), processed_text.c_str());
    
    // 上下文分析
    bool has_brightness_context = (text.find("亮度") != std::string::npos || 
                                   text.find("brightness") != std::string::npos);
    bool has_volume_context = (text.find("音量") != std::string::npos || 
                              text.find("声音") != std::string::npos ||
                              text.find("volume") != std::string::npos);
    bool has_theme_context = (text.find("主题") != std::string::npos ||
                             text.find("字体") != std::string::npos);
    bool has_display_mode_context = (text.find("模式") != std::string::npos ||
                                    text.find("壁纸") != std::string::npos ||
                                    text.find("皮肤") != std::string::npos ||
                                    text.find("表情包") != std::string::npos);
    bool has_subtitle_context = (text.find("字幕") != std::string::npos);
    
    // 跟踪已处理的意图类型，避免重复
    std::set<IntentType> detected_types;
    
    for (const auto& rule : keyword_rules_) {
        if (MatchKeywords(processed_text, rule.keywords)) {
            // 避免同一类型的重复检测
            if (detected_types.count(rule.intent_type) > 0) {
                ESP_LOGD(TAG, "跳过重复意图类型: %d", (int)rule.intent_type);
                continue;
            }
            
            // 上下文验证：避免错误匹配
            if (rule.intent_type == IntentType::BRIGHTNESS_CONTROL && has_volume_context && !has_brightness_context) {
                ESP_LOGD(TAG, "跳过亮度规则：检测到音量上下文");
                continue;
            }
            if (rule.intent_type == IntentType::VOLUME_CONTROL && has_brightness_context && !has_volume_context) {
                ESP_LOGD(TAG, "跳过音量规则：检测到亮度上下文");
                continue;
            }
            if (rule.intent_type == IntentType::THEME_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_theme_context) {
                ESP_LOGD(TAG, "跳过主题规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::DISPLAY_MODE_CONTROL && (has_volume_context || has_brightness_context || has_theme_context) && !has_display_mode_context) {
                ESP_LOGD(TAG, "跳过显示模式规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            if (rule.intent_type == IntentType::SUBTITLE_CONTROL && (has_volume_context || has_brightness_context || has_display_mode_context) && !has_subtitle_context) {
                ESP_LOGD(TAG, "跳过字幕规则：检测到其他控制上下文");
                continue;
            }
            
            IntentResult result;
            result.type = rule.intent_type;
            result.action = rule.action;
            result.device_name = rule.device;
            result.confidence = 0.9f; // 基础置信度
            
            // 调用参数提取器
            if (rule.parameter_extractor) {
                rule.parameter_extractor(processed_text, result);
            }
            
            results.push_back(result);
            detected_types.insert(rule.intent_type);
            
            ESP_LOGI(TAG, "检测到意图 %zu: %s.%s, 置信度: %.2f", 
                     results.size(), result.device_name.c_str(), result.action.c_str(), result.confidence);
        }
    }
    
    ESP_LOGI(TAG, "总共检测到 %zu 个意图", results.size());
    return results;
}

void LocalIntentDetector::FeedAudioData(const std::vector<int16_t>& audio_data) {
    if (!enabled_) return;
    
    std::lock_guard<std::mutex> lock(audio_buffer_mutex_);
    audio_buffer_.push_back(audio_data);
    
    // 保持缓冲区大小在合理范围内（约1秒的数据）
    while (audio_buffer_.size() > 50) { // 假设20ms一帧，50帧约1秒
        audio_buffer_.pop_front();
    }
}

void LocalIntentDetector::OnIntentDetected(std::function<void(const IntentResult& result)> callback) {
    intent_callback_ = callback;
}

void LocalIntentDetector::AddKeywordRule(const KeywordRule& rule) {
    keyword_rules_.push_back(rule);
}

std::string LocalIntentDetector::PreprocessText(const std::string& text) {
    std::string result = text;
    
    // 转换为小写
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    // 移除标点符号和空格
    result.erase(std::remove_if(result.begin(), result.end(), 
                 [](char c) { return std::ispunct(c) || std::isspace(c); }), 
                 result.end());
    
    return result;
}

bool LocalIntentDetector::MatchKeywords(const std::string& text, const std::vector<std::string>& keywords) {
    for (const auto& keyword : keywords) {
        std::string processed_keyword = PreprocessText(keyword);
        if (text.find(processed_keyword) != std::string::npos) {
            ESP_LOGD(TAG, "关键词匹配: %s 在 %s 中找到", processed_keyword.c_str(), text.c_str());
            return true;
        }
    }
    return false;
}

int LocalIntentDetector::ExtractNumberStatic(const std::string& text) {
    // 简化的数字提取，避免使用regex节省栈空间
    
    // 查找阿拉伯数字 - 使用简单的字符扫描代替正则表达式
    std::string current_number;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            current_number += c;
        } else {
            if (!current_number.empty() && current_number.length() <= 3) {
                int num = std::atoi(current_number.c_str());
                if (num >= 0 && num <= 100) { // 音量范围限制
                    return num;
                }
            }
            current_number.clear();
        }
    }
    
    // 检查最后的数字
    if (!current_number.empty() && current_number.length() <= 3) {
        int num = std::atoi(current_number.c_str());
        if (num >= 0 && num <= 100) {
            return num;
        }
    }
    
    // 使用完整的中文数字表，按长度优先匹配
    static const std::vector<std::pair<std::string, int>> chinese_numbers_complete = {
        // 三字符数字（最高优先级）
        {"九十九", 99}, {"九十八", 98}, {"九十七", 97}, {"九十六", 96}, {"九十五", 95},
        {"九十四", 94}, {"九十三", 93}, {"九十二", 92}, {"九十一", 91},
        {"八十九", 89}, {"八十八", 88}, {"八十七", 87}, {"八十六", 86}, {"八十五", 85},
        {"八十四", 84}, {"八十三", 83}, {"八十二", 82}, {"八十一", 81},
        {"七十九", 79}, {"七十八", 78}, {"七十七", 77}, {"七十六", 76}, {"七十五", 75},
        {"七十四", 74}, {"七十三", 73}, {"七十二", 72}, {"七十一", 71},
        {"六十九", 69}, {"六十八", 68}, {"六十七", 67}, {"六十六", 66}, {"六十五", 65},
        {"六十四", 64}, {"六十三", 63}, {"六十二", 62}, {"六十一", 61},
        {"五十九", 59}, {"五十八", 58}, {"五十七", 57}, {"五十六", 56}, {"五十五", 55},
        {"五十四", 54}, {"五十三", 53}, {"五十二", 52}, {"五十一", 51},
        {"四十九", 49}, {"四十八", 48}, {"四十七", 47}, {"四十六", 46}, {"四十五", 45},
        {"四十四", 44}, {"四十三", 43}, {"四十二", 42}, {"四十一", 41},
        {"三十九", 39}, {"三十八", 38}, {"三十七", 37}, {"三十六", 36}, {"三十五", 35},
        {"三十四", 34}, {"三十三", 33}, {"三十二", 32}, {"三十一", 31},
        {"二十九", 29}, {"二十八", 28}, {"二十七", 27}, {"二十六", 26}, {"二十五", 25},
        {"二十四", 24}, {"二十三", 23}, {"二十二", 22}, {"二十一", 21},
        // 两字符数字
        {"一十", 10}, {"十一", 11}, {"十二", 12}, {"十三", 13}, {"十四", 14}, {"十五", 15},
        {"十六", 16}, {"十七", 17}, {"十八", 18}, {"十九", 19}, {"二十", 20},
        {"三十", 30}, {"四十", 40}, {"五十", 50}, {"六十", 60}, {"七十", 70},
        {"八十", 80}, {"九十", 90}, {"一百", 100},
        // 单字符数字（最低优先级）
        {"十", 10}, {"一", 1}, {"二", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"零", 0}
    };
    
    for (const auto& pair : chinese_numbers_complete) {
        if (text.find(pair.first) != std::string::npos) {
            return pair.second;
        }
    }
    
    return -1;
}

int LocalIntentDetector::ExtractNumberWithContext(const std::string& text, const std::vector<std::string>& context_keywords) {
    if (text.empty() || context_keywords.empty()) {
        return ExtractNumberStatic(text); // 回退到静态提取
    }
    
    ESP_LOGI(TAG, "上下文数字提取开始: '%s'", text.c_str());
    
    // 找到所有数字及其位置
    std::vector<std::pair<int, size_t>> numbers_with_positions;
    
    // 查找阿拉伯数字
    std::string current_number;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        if (c >= '0' && c <= '9') {
            current_number += c;
        } else {
            if (!current_number.empty() && current_number.length() <= 3) {
                int num = std::atoi(current_number.c_str());
                if (num >= 0 && num <= 100) {
                    // 记录数字的结束位置
                    numbers_with_positions.push_back({num, i});
                }
            }
            current_number.clear();
        }
    }
    
    // 检查最后的数字
    if (!current_number.empty() && current_number.length() <= 3) {
        int num = std::atoi(current_number.c_str());
        if (num >= 0 && num <= 100) {
            numbers_with_positions.push_back({num, text.length()});
        }
    }
    
    // 完整的0-100中文数字表 - 按长度优先排序
    static const std::vector<std::pair<std::string, int>> chinese_numbers_complete = {
        // 三字符数字
        {"九十九", 99}, {"九十八", 98}, {"九十七", 97}, {"九十六", 96}, {"九十五", 95},
        {"九十四", 94}, {"九十三", 93}, {"九十二", 92}, {"九十一", 91},
        {"八十九", 89}, {"八十八", 88}, {"八十七", 87}, {"八十六", 86}, {"八十五", 85},
        {"八十四", 84}, {"八十三", 83}, {"八十二", 82}, {"八十一", 81},
        {"七十九", 79}, {"七十八", 78}, {"七十七", 77}, {"七十六", 76}, {"七十五", 75},
        {"七十四", 74}, {"七十三", 73}, {"七十二", 72}, {"七十一", 71},
        {"六十九", 69}, {"六十八", 68}, {"六十七", 67}, {"六十六", 66}, {"六十五", 65},
        {"六十四", 64}, {"六十三", 63}, {"六十二", 62}, {"六十一", 61},
        {"五十九", 59}, {"五十八", 58}, {"五十七", 57}, {"五十六", 56}, {"五十五", 55},
        {"五十四", 54}, {"五十三", 53}, {"五十二", 52}, {"五十一", 51},
        {"四十九", 49}, {"四十八", 48}, {"四十七", 47}, {"四十六", 46}, {"四十五", 45},
        {"四十四", 44}, {"四十三", 43}, {"四十二", 42}, {"四十一", 41},
        {"三十九", 39}, {"三十八", 38}, {"三十七", 37}, {"三十六", 36}, {"三十五", 35},
        {"三十四", 34}, {"三十三", 33}, {"三十二", 32}, {"三十一", 31},
        {"二十九", 29}, {"二十八", 28}, {"二十七", 27}, {"二十六", 26}, {"二十五", 25},
        {"二十四", 24}, {"二十三", 23}, {"二十二", 22}, {"二十一", 21},
        // 两字符数字
        {"一十", 10}, {"十一", 11}, {"十二", 12}, {"十三", 13}, {"十四", 14}, {"十五", 15},
        {"十六", 16}, {"十七", 17}, {"十八", 18}, {"十九", 19}, {"二十", 20},
        {"三十", 30}, {"四十", 40}, {"五十", 50}, {"六十", 60}, {"七十", 70},
        {"八十", 80}, {"九十", 90}, {"一百", 100},
        // 单字符数字（最低优先级）
        {"十", 10}, {"一", 1}, {"二", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"零", 0}
    };
    
    // 用字符串替换的方式，优先处理长的数字，避免重叠
    std::string text_copy = text;
    std::vector<std::pair<int, size_t>> temp_numbers;
    
    for (const auto& pair : chinese_numbers_complete) {
        size_t pos = 0;
        while ((pos = text_copy.find(pair.first, pos)) != std::string::npos) {
            if (pair.second <= 100) {
                temp_numbers.push_back({pair.second, pos + pair.first.length()});
                ESP_LOGI(TAG, "找到中文数字: '%s' = %d, 原位置: %zu", 
                         pair.first.c_str(), pair.second, pos);
                
                // 用占位符替换已匹配的数字，避免重复匹配
                std::string placeholder(pair.first.length(), 'X');
                text_copy.replace(pos, pair.first.length(), placeholder);
            }
            pos += pair.first.length();
        }
    }
    
    // 将找到的中文数字添加到总列表
    for (const auto& num : temp_numbers) {
        numbers_with_positions.push_back(num);
    }
    
    if (numbers_with_positions.empty()) {
        ESP_LOGI(TAG, "未找到数字");
        return -1;
    }
    
    ESP_LOGI(TAG, "找到 %zu 个数字:", numbers_with_positions.size());
    for (size_t i = 0; i < numbers_with_positions.size(); ++i) {
        ESP_LOGI(TAG, "  数字 %zu: %d (位置: %zu)", i+1, numbers_with_positions[i].first, numbers_with_positions[i].second);
    }
    
    // 找到最近的关键词位置
    size_t closest_keyword_pos = std::string::npos;
    std::string matched_keyword;
    for (const auto& keyword : context_keywords) {
        size_t pos = text.find(keyword);
        if (pos != std::string::npos) {
            if (closest_keyword_pos == std::string::npos || pos < closest_keyword_pos) {
                closest_keyword_pos = pos;
                matched_keyword = keyword;
            }
        }
    }
    
    if (closest_keyword_pos != std::string::npos) {
        ESP_LOGI(TAG, "找到上下文关键词: '%s' 在位置 %zu", matched_keyword.c_str(), closest_keyword_pos);
    }
    
    if (closest_keyword_pos == std::string::npos) {
        ESP_LOGI(TAG, "未找到上下文关键词，使用第一个数字: %d", numbers_with_positions[0].first);
        return numbers_with_positions[0].first;
    }
    
    // 改进的数字选择算法：优先选择关键词之后最近的数字
    int best_number = numbers_with_positions[0].first;
    size_t min_score = std::numeric_limits<size_t>::max();
    
    ESP_LOGI(TAG, "关键词 '%s' 位置: %zu", matched_keyword.c_str(), closest_keyword_pos);
    
    for (const auto& num_pos : numbers_with_positions) {
        size_t distance = std::abs((long)num_pos.second - (long)closest_keyword_pos);
        
        // 计算评分：优先考虑在关键词之后的数字
        size_t score;
        if (num_pos.second > closest_keyword_pos) {
            // 数字在关键词之后：使用实际距离
            score = distance;
            ESP_LOGI(TAG, "数字 %d (位置:%zu) 在关键词之后，距离:%zu, 评分:%zu", 
                     num_pos.first, num_pos.second, distance, score);
        } else {
            // 数字在关键词之前：大幅增加评分（降低优先级）
            score = distance + 1000;
            ESP_LOGI(TAG, "数字 %d (位置:%zu) 在关键词之前，距离:%zu, 评分:%zu (惩罚)", 
                     num_pos.first, num_pos.second, distance, score);
        }
        
        if (score < min_score) {
            min_score = score;
            best_number = num_pos.first;
        }
    }
    
    ESP_LOGI(TAG, "上下文数字提取结果: 选择数字 %d (最佳评分:%zu), 总共找到 %zu 个数字", 
             best_number, min_score, numbers_with_positions.size());
    return best_number;
}

int LocalIntentDetector::ChineseNumberToInt(const std::string& chinese_num) {
    // 简化的中文数字转换实现
    if (chinese_num == "十") return 10;
    if (chinese_num == "二十") return 20;
    if (chinese_num == "三十") return 30;
    if (chinese_num == "四十") return 40;
    if (chinese_num == "五十") return 50;
    if (chinese_num == "六十") return 60;
    if (chinese_num == "七十") return 70;
    if (chinese_num == "八十") return 80;
    if (chinese_num == "九十") return 90;
    if (chinese_num == "一百") return 100;
    
    return -1;
}

void LocalIntentDetector::ExtractVolumeParameters(const std::string& text, IntentResult& result) {
    // 首先检查特殊情况
    if (text.find("最大") != std::string::npos || text.find("最响") != std::string::npos) {
        result.parameters["volume"] = "100";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到音量最大请求: 100");
        return;
    }
    
    if (text.find("最小") != std::string::npos || text.find("静音") != std::string::npos) {
        result.parameters["volume"] = "0";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到音量最小/静音请求: 0");
        return;
    }
    
    // 检查精确的相对调节（10点增减）
    if (text.find("大一点") != std::string::npos || text.find("大一些") != std::string::npos) {
        result.parameters["relative"] = "increase_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到音量大一点请求: +10");
        return;
    }
    
    if (text.find("小一点") != std::string::npos || text.find("小一些") != std::string::npos) {
        result.parameters["relative"] = "decrease_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到音量小一点请求: -10");
        return;
    }
    
    // 使用上下文相关的数字提取
    int volume = ExtractNumberWithContext(text, {"音量", "声音", "volume", "sound"});
    
    if (volume >= 0 && volume <= 100) {
        result.parameters["volume"] = std::to_string(volume);
        result.confidence = 0.95f; // 找到数字时提高置信度
        ESP_LOGI(TAG, "提取音量参数: %d", volume);
    } else {
        // 处理其他相对调节（保持原有15点增减逻辑）
        if (text.find("大") != std::string::npos || text.find("高") != std::string::npos) {
            result.parameters["relative"] = "increase";
            result.confidence = 0.8f;
        } else if (text.find("小") != std::string::npos || text.find("低") != std::string::npos) {
            result.parameters["relative"] = "decrease";
            result.confidence = 0.8f;
        }
    }
}

void LocalIntentDetector::ExtractBrightnessParameters(const std::string& text, IntentResult& result) {
    // 首先检查特殊情况
    if (text.find("最大") != std::string::npos || text.find("最亮") != std::string::npos) {
        result.parameters["brightness"] = "100";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到亮度最大请求: 100");
        return;
    }
    
    if (text.find("最小") != std::string::npos || text.find("最暗") != std::string::npos) {
        result.parameters["brightness"] = "0";
        result.confidence = 0.98f;
        ESP_LOGI(TAG, "检测到亮度最小请求: 0");
        return;
    }
    
    // 检查精确的相对调节（10点增减）
    if (text.find("大一点") != std::string::npos || text.find("亮一点") != std::string::npos || text.find("大一些") != std::string::npos) {
        result.parameters["relative"] = "increase_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到亮度大一点请求: +10");
        return;
    }
    
    if (text.find("小一点") != std::string::npos || text.find("暗一点") != std::string::npos || text.find("小一些") != std::string::npos) {
        result.parameters["relative"] = "decrease_10";
        result.confidence = 0.9f;
        ESP_LOGI(TAG, "检测到亮度小一点请求: -10");
        return;
    }
    
    // 使用上下文相关的数字提取
    int brightness = ExtractNumberWithContext(text, {"亮度", "屏幕", "brightness", "screen"});
    
    if (brightness >= 0 && brightness <= 100) {
        result.parameters["brightness"] = std::to_string(brightness);
        result.confidence = 0.95f; // 找到数字时提高置信度
        ESP_LOGI(TAG, "提取亮度参数: %d", brightness);
    } else {
        // 处理其他相对调节（保持原有20点增减逻辑）
        if (text.find("亮") != std::string::npos || 
            text.find("bright") != std::string::npos ||
            text.find("调亮") != std::string::npos ||
            text.find("变亮") != std::string::npos) {
            result.parameters["relative"] = "increase";
            result.confidence = 0.85f;
            ESP_LOGI(TAG, "检测到亮度调亮请求");
        } else if (text.find("暗") != std::string::npos ||
                   text.find("dark") != std::string::npos ||
                   text.find("调暗") != std::string::npos ||
                   text.find("变暗") != std::string::npos) {
            result.parameters["relative"] = "decrease";
            result.confidence = 0.85f;
            ESP_LOGI(TAG, "检测到亮度调暗请求");
        } else {
            // 没有找到具体数值或相对调节，使用默认亮度
            result.parameters["brightness"] = "75"; // 默认亮度75%
            result.confidence = 0.7f;
            ESP_LOGI(TAG, "使用默认亮度: 75");
        }
    }
}

void LocalIntentDetector::ExtractThemeParameters(const std::string& text, IntentResult& result) {
    // 检查暗色主题关键词
    if (text.find("黑色主题") != std::string::npos || 
        text.find("黑夜模式") != std::string::npos ||
        text.find("黑色字体") != std::string::npos ||
        text.find("黑色字幕") != std::string::npos) {
        result.parameters["theme_name"] = "dark";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到暗色主题请求: dark");
        return;
    }
    
    // 检查亮色主题关键词
    if (text.find("白色主题") != std::string::npos || 
        text.find("白天模式") != std::string::npos ||
        text.find("白色字体") != std::string::npos ||
        text.find("白色字幕") != std::string::npos) {
        result.parameters["theme_name"] = "light";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到亮色主题请求: light");
        return;
    }
    
    // 默认情况：如果没有明确指定，使用暗色主题
    result.parameters["theme_name"] = "dark";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认主题: dark");
}

void LocalIntentDetector::ExtractDisplayModeParameters(const std::string& text, IntentResult& result) {
    // 检查表情包模式关键词（优先级最高）
    if (text.find("表情包模式") != std::string::npos || 
        text.find("表情模式") != std::string::npos ||
        text.find("情绪模式") != std::string::npos ||
        text.find("切换到表情包") != std::string::npos ||
        text.find("emoji模式") != std::string::npos ||
        (text.find("表情包") != std::string::npos && 
         (text.find("模式") != std::string::npos || text.find("切换") != std::string::npos))) {
        result.action = "SetEmoticonMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到表情包模式请求");
        return;
    }
    
    // 检查静态模式关键词
    if (text.find("静态模式") != std::string::npos || 
        text.find("静态壁纸") != std::string::npos ||
        text.find("静态皮肤") != std::string::npos) {
        result.action = "SetStaticMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到静态模式请求");
        return;
    }
    
    // 检查动态模式关键词
    if (text.find("动态模式") != std::string::npos || 
        text.find("动态壁纸") != std::string::npos ||
        text.find("动态皮肤") != std::string::npos) {
        result.action = "SetAnimatedMode";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到动态模式请求");
        return;
    }
    
    // 默认情况：设置为动态模式
    result.action = "SetAnimatedMode";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认显示模式: 动态");
}

void LocalIntentDetector::ExtractSubtitleParameters(const std::string& text, IntentResult& result) {
    // 检查打开字幕关键词
    if (text.find("打开") != std::string::npos || 
        text.find("开启") != std::string::npos ||
        text.find("显示") != std::string::npos) {
        result.action = "ShowSubtitle";
        result.parameters["visible"] = "true";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到打开字幕请求");
        return;
    }
    
    // 检查关闭字幕关键词
    if (text.find("关闭") != std::string::npos || 
        text.find("隐藏") != std::string::npos ||
        text.find("关掉") != std::string::npos) {
        result.action = "HideSubtitle";
        result.parameters["visible"] = "false";
        result.confidence = 0.95f;
        ESP_LOGI(TAG, "检测到关闭字幕请求");
        return;
    }
    
    // 默认情况：切换字幕显示状态
    result.action = "ToggleSubtitle";
    result.confidence = 0.7f;
    ESP_LOGI(TAG, "使用默认字幕操作: 切换");
}

} // namespace intent
