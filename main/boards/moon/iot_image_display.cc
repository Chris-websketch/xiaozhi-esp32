/*
 * @Date: 2025-04-10 10:00:00
 * @LastEditors: Claude
 * @LastEditTime: 2025-04-10 10:00:00
 * @FilePath: \xiaozhi-esp32\main\boards\moon\iot_image_display.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "settings.h"
#include <esp_log.h>
#include "iot_image_display.h"  // 引入头文件
#include "image_manager.h"  // 引入图片资源管理器
#include <stdlib.h>
#include <string.h>
#include <esp_heap_caps.h>  // 用于heap_caps_malloc

#define TAG "ImageDisplay"

namespace iot {

// 全局变量实现
extern "C" {
    // 默认是动画模式
    volatile ImageDisplayMode g_image_display_mode = MODE_ANIMATED;
    // 静态图片指针，初始为nullptr，将在运行时设置
    const unsigned char* g_static_image = nullptr;
    // 当前表情状态，默认为平静
    volatile EmotionType g_current_emotion = EMOTION_CALM;
    // 表情包图片数组 - 动态从LittleFS加载
    const unsigned char* g_emoticon_images[6] = {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };
}

// 加载单个表情包文件
static uint8_t* LoadEmoticonFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "无法打开表情包文件: %s", path);
        return nullptr;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // 验证大小（应该是115200字节）
    if (size != 115200) {
        ESP_LOGW(TAG, "表情包文件大小不正确: %s (%zu bytes)", path, size);
        fclose(f);
        return nullptr;
    }
    
    // 分配内存并读取 - 优先使用PSRAM
    uint8_t* data = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data) {
        ESP_LOGE(TAG, "无法为表情包分配内存: %s", path);
        fclose(f);
        return nullptr;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    
    if (read != size) {
        ESP_LOGW(TAG, "表情包读取不完整: %s", path);
        free(data);
        return nullptr;
    }
    
    ESP_LOGI(TAG, "成功加载表情包: %s", path);
    return data;
}

// 加载所有表情包
void LoadAllEmoticons() {
    const char* emoticon_paths[6] = {
        "/resources/emoticons/happy.bin",
        "/resources/emoticons/sad.bin",
        "/resources/emoticons/angry.bin",
        "/resources/emoticons/surprised.bin",
        "/resources/emoticons/calm.bin",
        "/resources/emoticons/shy.bin"
    };
    
    ESP_LOGI(TAG, "开始加载表情包资源...");
    int success_count = 0;
    
    for (int i = 0; i < 6; i++) {
        g_emoticon_images[i] = LoadEmoticonFile(emoticon_paths[i]);
        if (g_emoticon_images[i]) {
            success_count++;
        }
    }
    
    ESP_LOGI(TAG, "表情包加载完成: %d/6 成功", success_count);
}

// 图片显示控制类
class ImageDisplay : public Thing {
private:
    ImageDisplayMode display_mode_ = MODE_ANIMATED;

public:
    ImageDisplay() : Thing("ImageDisplay", "显示模式，可以切换动画、静态logo或表情包模式") {
        // 从图片资源管理器获取logo图片
        auto& image_manager = ImageResourceManager::GetInstance();
        g_static_image = image_manager.GetLogoImage();
        
        if (g_static_image) {
            ESP_LOGI(TAG, "成功获取网络下载的logo图片");
        } else {
            ESP_LOGW(TAG, "暂时无法获取logo图片，可能需要等待下载完成");
        }
        
        // 表情包将在图片播放任务启动时预加载（避免切换时阻塞）
        ESP_LOGI(TAG, "表情包将在图片任务启动时预加载");
    
        // 从系统配置中读取显示模式
        Settings settings("image_display");
        int mode = settings.GetInt("display_mode", MODE_ANIMATED);
        display_mode_ = static_cast<ImageDisplayMode>(mode);
        g_image_display_mode = display_mode_;

        ESP_LOGI(TAG, "当前图片显示模式: %d", display_mode_);
        
        // 定义设备的属性
        properties_.AddNumberProperty("display_mode", "显示模式(0=动画,1=静态logo,2=表情包)", [this]() -> int {
            return static_cast<int>(display_mode_);
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("SetAnimatedMode", "设置为动画模式（说话时播放动画）", ParameterList(), 
            [this](const ParameterList& parameters) {
                display_mode_ = MODE_ANIMATED;
                g_image_display_mode = MODE_ANIMATED;
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", MODE_ANIMATED);
                
                ESP_LOGI(TAG, "已设置为动画模式");
        });

        methods_.AddMethod("SetStaticMode", "设置为静态模式（固定显示logo图片）", ParameterList(), 
            [this](const ParameterList& parameters) {
                display_mode_ = MODE_STATIC;
                g_image_display_mode = MODE_STATIC;
                
                // 重新获取logo图片（可能在初始化后才下载完成）
                auto& image_manager = ImageResourceManager::GetInstance();
                const uint8_t* logo = image_manager.GetLogoImage();
                if (logo) {
                    g_static_image = logo;
                    ESP_LOGI(TAG, "已更新logo图片");
                }
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", MODE_STATIC);
                
                ESP_LOGI(TAG, "已设置为静态logo模式");
        });

        methods_.AddMethod("SetEmoticonMode", "设置为表情包模式（根据AI回复情绪显示表情包）", ParameterList(), 
            [this](const ParameterList& parameters) {
                display_mode_ = MODE_EMOTICON;
                g_image_display_mode = MODE_EMOTICON;
                
                // 初始化为平静表情
                g_current_emotion = EMOTION_CALM;
                
                // 验证表情包已加载（启动时已预加载）
                int loaded_count = 0;
                for (int i = 0; i < 6; i++) {
                    if (g_emoticon_images[i] != nullptr) {
                        loaded_count++;
                    }
                }
                
                ESP_LOGI(TAG, "表情包加载状态: %d/6", loaded_count);
                
                if (loaded_count < 6) {
                    ESP_LOGW(TAG, "⚠️ 部分表情包未加载，可能影响显示效果");
                }
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", MODE_EMOTICON);
                
                ESP_LOGI(TAG, "已设置为表情包模式");
        });

        methods_.AddMethod("ToggleDisplayMode", "切换图片显示模式", ParameterList(), 
            [this](const ParameterList& parameters) {
                // 在三种模式之间循环切换：动画 -> 静态 -> 表情包 -> 动画
                if (display_mode_ == MODE_ANIMATED) {
                    display_mode_ = MODE_STATIC;
                    g_image_display_mode = MODE_STATIC;
                    
                    // 重新获取logo图片
                    auto& image_manager = ImageResourceManager::GetInstance();
                    const uint8_t* logo = image_manager.GetLogoImage();
                    if (logo) {
                        g_static_image = logo;
                    }
                    ESP_LOGI(TAG, "已切换到静态logo模式");
                } else if (display_mode_ == MODE_STATIC) {
                    display_mode_ = MODE_EMOTICON;
                    g_image_display_mode = MODE_EMOTICON;
                    g_current_emotion = EMOTION_CALM;
                    ESP_LOGI(TAG, "已切换到表情包模式");
                } else {
                    display_mode_ = MODE_ANIMATED;
                    g_image_display_mode = MODE_ANIMATED;
                    ESP_LOGI(TAG, "已切换到动画模式");
                }
                
                // 保存设置
                Settings settings("image_display", true);
                settings.SetInt("display_mode", static_cast<int>(display_mode_));
        });
    }
    
    // 提供方法在图片下载完成后更新logo图片
    void UpdateLogoImage() {
        auto& image_manager = ImageResourceManager::GetInstance();
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已更新");
        }
    }
};

} // namespace iot

DECLARE_THING(ImageDisplay);
