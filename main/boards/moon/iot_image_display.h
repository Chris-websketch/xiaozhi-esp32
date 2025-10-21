/*
 * @FilePath: \xiaozhi-esp32\main\boards\moon\iot_image_display.h
 */
#pragma once

namespace iot {

// 定义图片显示模式
enum ImageDisplayMode {
    MODE_ANIMATED = 0,  // 动画模式（根据音频状态自动播放动画）
    MODE_STATIC = 1,    // 静态模式（显示logo.h中的图片）
    MODE_EMOTICON = 2   // 表情包模式（根据AI回复情绪显示表情包）
};

// 定义表情类型枚举
enum EmotionType {
    EMOTION_HAPPY = 0,      // 😄 开心
    EMOTION_SAD = 1,        // 😢 悲伤
    EMOTION_ANGRY = 2,      // 😠 生气
    EMOTION_FEARFUL = 3,    // 😨 恐惧
    EMOTION_DISGUSTED = 4,  // 🤢 厌恶
    EMOTION_SURPRISED = 5,  // 😲 惊讶
    EMOTION_CALM = 6,       // 😐 平静
    EMOTION_UNKNOWN = 7     // 未识别
};

// 声明全局变量，以便在其他文件中使用
extern "C" {
    extern volatile ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;
    extern volatile EmotionType g_current_emotion;
    extern const unsigned char* g_emoticon_images[7];
    
    // 表情包加载函数
    void LoadAllEmoticons();
}

} // namespace iot
