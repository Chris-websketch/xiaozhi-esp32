#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>
#include <string.h>
#include <cstring>
#include <freertos/semphr.h>

#include <numeric>

#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "sph0645_audio_codec.h"
#include "system_reset.h"
#include "wifi_board.h"
#include "image_manager.h"
#include "iot_image_display.h"
#include "lvgl.h"


#define TAG "NulllabAIVox"

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This board is only supported on ESP32-S3, please use ESP32-S3 target by using 'idf.py esp32s3' and 'idf.py menuconfig' to configure the project."
#endif

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_puhui_20_4);

// 下载进度状态结构体
struct {
    bool pending;
    int progress;
    char message[64];
    SemaphoreHandle_t mutex;
} g_download_progress = {false, 0, "", NULL};

// DisplayLockGuard 已在 display/display.h 中定义，无需重复定义

// 自定义LCD显示类，继承自SpiLcdDisplay
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    // 继承构造函数
    using SpiLcdDisplay::SpiLcdDisplay;
    
    void Initialize() {
        // 调用父类初始化方法（如果存在）
        SetupUI();
        // 参考 abrobot：将基础UI容器与内容区背景设为透明（状态栏保持不透明），确保底层图片可见
        MakeBaseUiTransparent();
        
        // 创建一个用于保护下载进度状态的互斥锁
        if (g_download_progress.mutex == NULL) {
            g_download_progress.mutex = xSemaphoreCreateMutex();
        }
        
        // 创建定时器定期检查并更新下载进度显示
        lv_timer_create(ProgressTimerCallback, 100, this); // 100ms检查一次
    }
    
    // 显示或隐藏下载进度条
    void ShowDownloadProgress(bool show, int progress = 0, const char* message = nullptr) {
        if (!show || !message) {
            // 隐藏UI
            UpdateDownloadProgressUI(false, 0, nullptr);
            return;
        }

        // 显示新的圆形进度条UI
        UpdateDownloadProgressUI(true, progress, message);
    }
    
    // 获取根容器（用于将图片容器放置在内容层底部）
    lv_obj_t* GetRootContainer() { return container_; }

    // 将基础UI容器移到最前（用于确保状态栏/聊天内容在图片之上）
    void BringBaseUIToFront() {
        if (container_ != nullptr) {
            lv_obj_move_foreground(container_);
        }
    }

    // 确保聊天内容与状态栏在前（注意：不要改变flex布局中的子对象顺序）
    void EnsureForegroundUi() {
        // 若预热或下载UI可见，则保持其在最前，不调整基础UI层级
        if ((preload_progress_container_ && !lv_obj_has_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) ||
            (download_progress_container_ && !lv_obj_has_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN))) {
            return;
        }
        // 由于图片容器现在是屏幕的直接子对象，我们需要确保container_在图片之上
        BringBaseUIToFront();
        // 注意：不要在flex布局的container内移动子对象，这会破坏布局顺序
        // 状态栏应该保持在顶部，内容区在下方
    }

    // 使基础UI背景透明，确保底层图片可见（但保持状态栏不透明）
    void MakeBaseUiTransparent() {
        DisplayLockGuard lock(this);
        if (container_) lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
        if (content_) lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
        // 状态栏保持不透明，用户希望状态栏有背景
        // if (status_bar_) lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    }
    
public:
    // 修改成员变量，删除进度条相关变量
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr; // 百分比标签
    lv_obj_t* message_label_ = nullptr;          // 状态消息标签
    lv_obj_t* download_progress_arc_ = nullptr;  // 圆形进度条
    
    // 添加预加载UI相关变量
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_arc_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    
    // 用户交互禁用状态标志
    bool user_interaction_disabled_ = false;
    
private:
    
    // 创建下载进度UI
    void CreateDownloadProgressUI() {
        // 创建主容器 - 白色背景
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(download_progress_container_);
        
        // 设置白色不透明背景
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_COVER, 0);  // 完全不透明
        lv_obj_set_style_border_width(download_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(download_progress_container_, 0, 0);

        // 创建圆形进度条 - 放在屏幕正中心
        lv_obj_t* progress_arc = lv_arc_create(download_progress_container_);
        lv_obj_set_size(progress_arc, 120, 120);
        lv_arc_set_rotation(progress_arc, 270); // 从顶部开始
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_angles(progress_arc, 0, 0);
        lv_obj_center(progress_arc);
        
        // 样式设置
        lv_obj_set_style_arc_width(progress_arc, 8, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        
        lv_obj_set_style_arc_width(progress_arc, 8, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
        
        // 移除可交互性
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        // 保存进度条引用
        download_progress_arc_ = progress_arc;

        // 在进度条中心显示百分比
        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_black(), 0);  // 黑色字体配白色背景
        lv_obj_set_style_text_align(download_progress_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(download_progress_label_, "0%");
        // 将百分比标签定位到进度条中心
        lv_obj_align_to(download_progress_label_, progress_arc, LV_ALIGN_CENTER, 0, 0);

        // 状态文字 - 放在进度条下方
        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_black(), 0);  // 黑色字体配白色背景
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(message_label_, "准备中...");
        lv_obj_set_width(message_label_, LV_HOR_RES - 40);
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        // 将状态标签定位到进度条下方
        lv_obj_align_to(message_label_, progress_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        
        // 下载UI需要覆盖图片与基础UI
        lv_obj_move_foreground(download_progress_container_);
    }

    // 添加新方法直接更新UI，只在主线程中调用
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        // 使用DisplayLockGuard管理锁
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        
        // 如果容器仍不存在，直接返回
        if (download_progress_container_ == nullptr) {
            return;
        }
        
        if (show && message) {
            // 限制进度值范围
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            
            // 更新圆形进度条
            if (download_progress_arc_) {
                lv_arc_set_value(download_progress_arc_, progress);
                
                // 根据进度调整颜色 - 增加视觉反馈
                if (progress < 30) {
                    // 开始阶段 - 亮蓝色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
                } else if (progress < 70) {
                    // 中间阶段 - 青色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FFB3), LV_PART_INDICATOR);
                } else {
                    // 接近完成 - 绿色
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FF7F), LV_PART_INDICATOR);
                }
            }
            
            // 更新中心百分比显示
            if (download_progress_label_) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            
            // 精简消息显示
            if (message_label_) {
                // 简化消息文本，只显示关键信息
                char simplified_message[32];
                
                if (strstr(message, "下载") != nullptr) {
                    if (strstr(message, "图片") != nullptr) {
                        snprintf(simplified_message, sizeof(simplified_message), "下载图片资源");
                    } else if (strstr(message, "logo") != nullptr) {
                        snprintf(simplified_message, sizeof(simplified_message), "下载Logo");
                    } else {
                        snprintf(simplified_message, sizeof(simplified_message), "下载中");
                    }
                } else if (progress >= 100) {
                    snprintf(simplified_message, sizeof(simplified_message), "完成");
                } else {
                    snprintf(simplified_message, sizeof(simplified_message), "准备中");
                }
                
                lv_label_set_text(message_label_, simplified_message);
            }
            
            // 确保容器可见
            lv_obj_clear_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            
            // 确保在最顶层显示
            lv_obj_move_foreground(download_progress_container_);
            
            // 自动隐藏下载完成的UI
            if (progress >= 100) {
                // 显示完成状态1秒后自动隐藏
                lv_timer_create(HideTimerCallback, 1000, this);
            }
        } else {
            // 隐藏容器并恢复基础UI在前
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            EnsureForegroundUi();
        }
    }
    
    void SetupUI() {
        // 基础UI设置可以在这里进行
    }
    
    // 静态回调函数
    static void ProgressTimerCallback(lv_timer_t* t) {
        CustomLcdDisplay* display = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
        if (!display) return;
        
        // 检查是否有待更新的进度
        if (g_download_progress.mutex && xSemaphoreTake(g_download_progress.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (g_download_progress.pending) {
                int progress = g_download_progress.progress;
                char message[64];
                strncpy(message, g_download_progress.message, sizeof(message));
                
                // 重置标志
                g_download_progress.pending = false;
                xSemaphoreGive(g_download_progress.mutex);
                
                // 更新UI
                display->UpdateDownloadProgressUI(true, progress, message);
            } else {
                xSemaphoreGive(g_download_progress.mutex);
            }
        }
    }
    
    static void HideTimerCallback(lv_timer_t* t) {
        CustomLcdDisplay* display = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
        if (display && display->download_progress_container_) {
            lv_obj_add_flag(display->download_progress_container_, LV_OBJ_FLAG_HIDDEN);
        }
        lv_timer_del(t);
    }
    
    void SetupUI_Old() {
        // 基础UI设置可以在这里进行
    }
    
    // 创建预加载进度UI
    void CreatePreloadProgressUI() {
        // 创建主容器 - 极简设计，只包含进度条和基本文字
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(preload_progress_container_);
        
        // 设置白色不透明背景
        lv_obj_set_style_bg_color(preload_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(preload_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(preload_progress_container_, 0, 0);
        
        // 设置垂直布局，居中对齐
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(preload_progress_container_, 20, 0);

        // 创建圆形进度条 - 稍大一些，更显眼
        lv_obj_t* progress_arc = lv_arc_create(preload_progress_container_);
        lv_obj_set_size(progress_arc, 80, 80);
        lv_arc_set_rotation(progress_arc, 270); // 从顶部开始
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        
        // 设置进度条样式 - 现代简约风格
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x3A3A3C), LV_PART_MAIN); // 背景轨道
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x007AFF), LV_PART_INDICATOR); // 进度颜色
        
        // 隐藏把手，保持简约
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        
        // 保存进度条引用
        preload_progress_arc_ = progress_arc;

        // 只保留一个状态提示文字
        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(preload_message_label_, "设备正在预热中...");
        
        // 确保UI在最顶层
        lv_obj_move_foreground(preload_progress_container_);
    }
    
public:
    // 更新预加载进度UI
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        // 使用DisplayLockGuard管理锁
        DisplayLockGuard lock(this);
        
        // 如果容器不存在但需要显示，创建UI
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
            DisableUserInteraction(); // 禁用用户交互
        }
        
        // 如果容器仍不存在，直接返回
        if (preload_progress_container_ == nullptr) {
            return;
        }
        
        if (show) {
            // 更新圆形进度条 - 极简版本，只显示进度
            if (preload_progress_arc_ && total > 0) {
                int progress_value = (current * 100) / total;
                if (progress_value > 100) progress_value = 100;
                if (progress_value < 0) progress_value = 0;
                lv_arc_set_value(preload_progress_arc_, progress_value);
                
                // 保持简约的蓝色，不做复杂的颜色变化
                lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
            }
            
            // 保持状态文字不变，简约显示
            if (preload_message_label_ != nullptr) {
                lv_label_set_text(preload_message_label_, "设备正在预热中...");
            }
            
        // 确保容器可见并覆盖在最顶层
        lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(preload_progress_container_);
        } else {
            // 隐藏容器并恢复基础UI层级与交互
            ESP_LOGI(TAG, "预加载完成，隐藏预加载UI容器");
            if (preload_progress_container_) {
                lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI(TAG, "预加载UI容器已隐藏");
            }
            EnsureForegroundUi();
            EnableUserInteraction();
        }
    }

private:    
    // 禁用用户交互
    void DisableUserInteraction() {
        user_interaction_disabled_ = true;
        ESP_LOGI(TAG, "用户交互已禁用（预热模式）");
    }
    
    // 恢复用户交互
    void EnableUserInteraction() {
        user_interaction_disabled_ = false;
        ESP_LOGI(TAG, "用户交互已恢复");
    }
};

class NulllabAIVox : public WifiBoard {
 private:
  Button boot_button_;
  Button volume_up_button_;
  Button volume_down_button_;
  uint32_t current_band_ = UINT32_MAX;
  CustomLcdDisplay* display_;
  adc_oneshot_unit_handle_t battery_adc_handle_ = nullptr;
  std::vector<int32_t> battery_adc_samples_;
  
  // 资源检查协调机制
  SemaphoreHandle_t resource_check_semaphore_ = nullptr;
  bool resource_update_in_progress_ = false;

  void InitializeSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
    buscfg.miso_io_num = GPIO_NUM_NC;
    buscfg.sclk_io_num = DISPLAY_CLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
  }

  void InitializeLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;
    // 液晶屏控制IO初始化
    ESP_LOGD(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = DISPLAY_SPI_MODE;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

    // 初始化液晶屏驱动芯片
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
    panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
    panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
    gc9a01_vendor_config_t gc9107_vendor_config = {
        .init_cmds = gc9107_lcd_init_cmds,
        .init_cmds_size =
            sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    };
#else
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif

    esp_lcd_panel_reset(panel);

    esp_lcd_panel_init(panel);
    esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef LCD_TYPE_GC9A01_SERIAL
    panel_config.vendor_config = &gc9107_vendor_config;
#endif
    display_ = new CustomLcdDisplay(
        panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
        DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
        {
            .text_font = &font_puhui_16_4,
            .icon_font = &font_awesome_16_4,
        });
    
    // 初始化显示器，设置UI背景为透明
    display_->Initialize();
  }

  void InitializeButtons() {
    boot_button_.OnClick([this]() {
      auto& app = Application::GetInstance();
      if (app.GetDeviceState() == kDeviceStateStarting &&
          !WifiStation::GetInstance().IsConnected()) {
        ResetWifiConfiguration();
      }
      app.ToggleChatState();
    });

    volume_up_button_.OnClick([this]() {
      auto codec = GetAudioCodec();
      auto volume = codec->output_volume() + 10;
      if (volume > 100) {
        volume = 100;
      }
      codec->SetOutputVolume(volume);
      GetDisplay()->ShowNotification(Lang::Strings::VOLUME +
                                     std::to_string(volume));
    });

    volume_up_button_.OnLongPress([this]() {
      GetAudioCodec()->SetOutputVolume(100);
      GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
    });

    volume_down_button_.OnClick([this]() {
      auto codec = GetAudioCodec();
      auto volume = codec->output_volume() - 10;
      if (volume < 0) {
        volume = 0;
      }
      codec->SetOutputVolume(volume);
      GetDisplay()->ShowNotification(Lang::Strings::VOLUME +
                                     std::to_string(volume));
    });

    volume_down_button_.OnLongPress([this]() {
      GetAudioCodec()->SetOutputVolume(0);
      GetDisplay()->ShowNotification(Lang::Strings::MUTED);
    });
  }

  // 物联网初始化，添加对 AI 可见设备
  void InitializeIot() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    thing_manager.AddThing(iot::CreateThing("Speaker"));
    thing_manager.AddThing(iot::CreateThing("Screen"));
    thing_manager.AddThing(iot::CreateThing("Lamp"));
    thing_manager.AddThing(iot::CreateThing("ImageDisplay"));
  }
  
  // 初始化图片资源管理器
  void InitializeImageResources() {
    auto& image_manager = ImageResourceManager::GetInstance();
    
    esp_err_t result = image_manager.Initialize();
    if (result != ESP_OK) {
      ESP_LOGE(TAG, "图片资源管理器初始化失败");
    }
  }
  
  // 检查图片资源更新
  void CheckImageResources() {
    ESP_LOGI(TAG, "资源检查任务开始");
    
    auto& image_manager = ImageResourceManager::GetInstance();
    
    // 等待WiFi连接
    auto& wifi = WifiStation::GetInstance();
    while (!wifi.IsConnected()) {
      ESP_LOGI(TAG, "等待WiFi连接以检查图片资源...");
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    ESP_LOGI(TAG, "开始检查图片资源更新...");
    
    // 设置API URL（需要根据实际情况配置）
    const char* API_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
    const char* VERSION_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
    
    // 一次性检查并更新所有资源（动画图片和logo）
    esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(API_URL, VERSION_URL);
    
    if (all_resources_result == ESP_OK) {
      ESP_LOGI(TAG, "图片资源检查/下载成功，有资源更新");
      resource_update_in_progress_ = true;  // 标记有资源更新
    } else if (all_resources_result == ESP_ERR_NOT_FOUND) {
      ESP_LOGI(TAG, "所有图片资源已是最新版本，无需更新");
      resource_update_in_progress_ = false;  // 无更新
    } else {
      ESP_LOGE(TAG, "图片资源检查/下载失败");
      resource_update_in_progress_ = false;  // 失败也继续预热
    }
    
    // 更新静态logo图片
    const uint8_t* logo = image_manager.GetLogoImage();
    if (logo) {
      iot::g_static_image = logo;
      ESP_LOGI(TAG, "logo图片已设置");
    } else {
      ESP_LOGW(TAG, "未能获取logo图片，将使用默认显示");
    }
    
    // 资源检查完成，释放信号量允许预热开始
    ESP_LOGI(TAG, "图片资源检查完成，释放信号量");
    if (resource_check_semaphore_) {
      xSemaphoreGive(resource_check_semaphore_);
    }
  }
  
  // 启动图片循环显示任务
  void StartImageSlideshow() {
    // 设置图片资源管理器的进度回调
    auto& image_manager = ImageResourceManager::GetInstance();
    
    // 设置下载进度回调函数，更新UI进度条
    image_manager.SetDownloadProgressCallback([this](int current, int total, const char* message) {
      if (display_) {
        // 计算正确的百分比并传递
        int percent = (total > 0) ? (current * 100 / total) : 0;
        
        // 简化：直接调用显示方法
        display_->ShowDownloadProgress(message != nullptr, percent, message);
      }
    });
    
    // 设置预加载进度回调函数，更新预加载UI进度
    image_manager.SetPreloadProgressCallback([this](int current, int total, const char* message) {
      if (display_) {
        // 确保UI更新在主线程中执行，避免线程安全问题
        auto& app = Application::GetInstance();
        // 创建消息的副本以避免异步执行时指针失效
        std::string msg_copy = message ? std::string(message) : std::string();
        bool has_message = (message != nullptr);
        app.Schedule([this, current, total, msg_copy, has_message]() {
          // 在主线程中执行UI更新
          const char* msg_ptr = has_message ? msg_copy.c_str() : nullptr;
          display_->UpdatePreloadProgressUI(has_message, current, total, msg_ptr);
        });
      }
    });
    
    // 创建图片播放任务
    xTaskCreate(ImageSlideshowTask, "image_slideshow", 8192, this, 5, NULL);
    
    // 创建资源检查任务（高优先级，确保优先执行）
    xTaskCreate([](void* arg) {
      NulllabAIVox* board = static_cast<NulllabAIVox*>(arg);
      board->CheckImageResources();
      vTaskDelete(NULL);
    }, "check_resources", 8192, this, 6, NULL);  // 优先级6，高于图片播放任务
  }
  
  // 图片循环显示任务实现
  static void ImageSlideshowTask(void* arg) {
    NulllabAIVox* board = static_cast<NulllabAIVox*>(arg);
    Display* display = board->GetDisplay();
    auto& app = Application::GetInstance();
    auto& image_manager = ImageResourceManager::GetInstance();
    
    ESP_LOGI(TAG, "🎬 图片播放任务启动");
    
    // 等待系统初始化完成
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 图像显示相关变量（LVGL v9 图像描述符）
    static lv_image_dsc_t img_dsc = {
      .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = (uint32_t)DISPLAY_WIDTH,
        .h = (uint32_t)DISPLAY_HEIGHT,
        .stride = (uint32_t)(DISPLAY_WIDTH * 2),
        .reserved_2 = 0,
      },
      .data_size = (uint32_t)(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2),
      .data = NULL,
      .reserved = NULL,
    };
    
    lv_obj_t* img_container = nullptr;
    lv_obj_t* img_obj = nullptr;
    
    // 创建图像显示容器
    {
      DisplayLockGuard lock(display);
      // 直接在屏幕上创建图片容器，而不是在container_下，这样图片会在所有UI后面
      img_container = lv_obj_create(lv_scr_act());
      lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
      lv_obj_center(img_container);
      lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
      lv_obj_set_style_pad_all(img_container, 0, 0);
      lv_obj_set_style_border_width(img_container, 0, 0);
      
      // 移除滑动条和交互功能
      lv_obj_clear_flag(img_container, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(img_container, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_scrollbar_mode(img_container, LV_SCROLLBAR_MODE_OFF);
      
      // 移除所有内边距和边框，确保图片完全填充
      lv_obj_set_style_outline_width(img_container, 0, 0);
      lv_obj_set_style_shadow_width(img_container, 0, 0);
      // 将图片容器置于屏幕最底层
      lv_obj_move_to_index(img_container, 0);
      
      // 确保基础UI容器在图片之上
      CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
      if (customDisplay) {
        customDisplay->BringBaseUIToFront();
      }
      
      ESP_LOGI(TAG, "图片容器已创建在屏幕层级，基础UI已移到前景");
      
      // 创建图像对象
      img_obj = lv_img_create(img_container);
      lv_obj_center(img_obj);
      
      // 确保图像对象也没有交互功能和滑动条
      lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(img_obj, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_pad_all(img_obj, 0, 0);
      lv_obj_set_style_border_width(img_obj, 0, 0);
      
      ESP_LOGI(TAG, "图像对象创建完成，无滑动条和交互功能");
    }
    
    // 显示初始图片
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 尝试从资源管理器获取logo图片
    const uint8_t* logo = image_manager.GetLogoImage();
    if (logo) {
      iot::g_static_image = logo;
      ESP_LOGI(TAG, "已从资源管理器快速获取logo图片");
    }
    
    // 创建异步预加载任务，不阻塞主流程
    xTaskCreate([](void* param) {
      NulllabAIVox* board = static_cast<NulllabAIVox*>(param);
      auto& img_mgr = ImageResourceManager::GetInstance();
      CustomLcdDisplay* display = static_cast<CustomLcdDisplay*>(board->GetDisplay());
      
      // 等待资源检查完成
      ESP_LOGI(TAG, "等待图片资源检查完成...");
      if (board->resource_check_semaphore_) {
        // 等待资源检查完成，会被阻塞直到资源检查任务给出信号量
        xSemaphoreTake(board->resource_check_semaphore_, portMAX_DELAY);
        ESP_LOGI(TAG, "资源检查已完成，开始预热流程");
      }
      
      // 如果检查到有资源更新下载，则跳过预热
      if (board->resource_update_in_progress_) {
        ESP_LOGI(TAG, "检测到图片资源有更新，跳过预热过程");
        // 不显示预热UI，直接退出
        vTaskDelete(NULL);
        return;
      }
      
      // 先显示预加载UI
      if (display) {
        auto& app = Application::GetInstance();
        app.Schedule([display]() {
          ESP_LOGI(TAG, "开始显示预加载UI");
          display->UpdatePreloadProgressUI(true, 0, 100, "设备正在预热中...");
        });
        vTaskDelay(pdMS_TO_TICKS(500)); // 确保UI有时间显示
      }
      
      // 简化音频系统检查，减少等待时间
      auto& app_preload = Application::GetInstance();
      int preload_wait = 0;
      while (preload_wait < 3) { // 增加到3秒，给用户更多时间看到预热界面
        if (app_preload.GetDeviceState() == kDeviceStateIdle && app_preload.IsAudioQueueEmpty()) {
          break;
        }
        ESP_LOGI(TAG, "预加载检查音频状态... (%d/3秒)", preload_wait + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1秒检查间隔
        preload_wait++;
      }
      
      ESP_LOGI(TAG, "开始异步预加载剩余图片...");
      esp_err_t preload_result = img_mgr.PreloadRemainingImages();
      
      // PreloadRemainingImages() 内部已通过回调处理所有UI更新，包括隐藏
      // 这里只需要打印结果日志，不再重复操作UI
      if (preload_result == ESP_OK) {
        ESP_LOGI(TAG, "图片预加载完成，动画播放将更加流畅");
      } else if (preload_result == ESP_ERR_NO_MEM) {
        ESP_LOGW(TAG, "内存不足，跳过图片预加载，将继续使用按需加载策略");
      } else {
        ESP_LOGW(TAG, "图片预加载失败，将继续使用按需加载策略");
      }
      
      // 隐藏预加载UI
      // 由 ImageResourceManager 在预加载结束时通过回调隐藏预加载UI，这里不再重复隐藏
      
      // 任务完成，删除自己
      vTaskDelete(NULL);
    }, "async_preload", 8192, static_cast<void*>(board), 4, NULL);  // 优先级4，低于资源检查任务
    
    // 立即尝试显示静态图片
    if (iot::g_image_display_mode == iot::MODE_STATIC && iot::g_static_image) {
      DisplayLockGuard lock(display);
      
      // 设置容器层级（图片在最底层），随后确保UI在前
      lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_to_index(img_container, 0);
      {
        CustomLcdDisplay* cd = static_cast<CustomLcdDisplay*>(display);
        if (cd) cd->BringBaseUIToFront();
      }
      
      img_dsc.data = iot::g_static_image;
      lv_img_set_src(img_obj, &img_dsc);
      ESP_LOGI(TAG, "开机立即显示logo图片");
    } else {
      // 否则尝试使用资源管理器中的图片
      const auto& imageArray = image_manager.GetImageArray();
      if (!imageArray.empty()) {
        const uint8_t* currentImage = imageArray[0];
        if (currentImage) {
          DisplayLockGuard lock(display);
        
        // 设置容器层级（图片在最底层），随后确保UI在前
          lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
          lv_obj_move_to_index(img_container, 0);
        {
          CustomLcdDisplay* cd = static_cast<CustomLcdDisplay*>(display);
          if (cd) cd->BringBaseUIToFront();
        }
          
          img_dsc.data = currentImage;
          lv_img_set_src(img_obj, &img_dsc);
          ESP_LOGI(TAG, "开机立即显示存储的图片");
        }
      }
    }
    
    // 主循环变量
    size_t currentIndex = 0;
    TickType_t lastUpdateTime = xTaskGetTickCount();
    bool directionForward = true;
    bool isAudioPlaying = false;
    DeviceState previousState = app.GetDeviceState();
    bool pendingAnimationStart = false;
    TickType_t stateChangeTime = 0;
    
    while (true) {
      // 获取图片数组
      const auto& imageArray = image_manager.GetImageArray();
      
      // 检查预加载UI是否可见
      bool isPreloadUIVisible = false;
      CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
      if (customDisplay && customDisplay->preload_progress_container_ &&
          !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
        isPreloadUIVisible = true;
      }
      
      // 如果没有图片资源，等待一段时间后重试
      if (imageArray.empty()) {
        ESP_LOGW(TAG, "图片资源未加载，等待中...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
      
      // 确保currentIndex在有效范围内
      if (currentIndex >= imageArray.size()) {
        currentIndex = 0;
      }
      
      // 获取当前设备状态
      DeviceState currentState = app.GetDeviceState();
      TickType_t currentTime = xTaskGetTickCount();
      
      // 根据显示模式和设备状态决定是否播放动画
      bool shouldAnimate = false;
      
      if (iot::g_image_display_mode == iot::MODE_ANIMATED) {
        // 动画模式：只在说话时播放动画，且预加载UI不可见时
        shouldAnimate = (currentState == kDeviceStateSpeaking) && !isPreloadUIVisible;
      }
      
      // 检测到状态变为Speaking
      if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
        pendingAnimationStart = true;
        stateChangeTime = currentTime;
        directionForward = true;
        ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
      }
      
      // 退出说话状态
      if (previousState == kDeviceStateSpeaking && currentState != kDeviceStateSpeaking) {
        isAudioPlaying = false;
        ESP_LOGI(TAG, "退出说话状态，停止动画");
      }
      
      // 延迟启动动画
      if (pendingAnimationStart && (currentTime - stateChangeTime >= pdMS_TO_TICKS(500))) {
        currentIndex = 1;
        directionForward = true;
        
        if (currentIndex < imageArray.size()) {
          const uint8_t* currentImage = imageArray[currentIndex];
          
          if (currentImage) {
            DisplayLockGuard lock(display);
            
            // 确保图片容器可见并处于合适层级（参考abrobot设置），随后确保UI在前
            lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_move_to_index(img_container, 0);  // 移到底层，避免遮挡状态栏
            if (customDisplay) {
              customDisplay->BringBaseUIToFront();
            }
            
            lv_obj_t* img_obj_inner = lv_obj_get_child(img_container, 0);
            if (img_obj_inner) {
              img_dsc.data = currentImage;
              lv_img_set_src(img_obj_inner, &img_dsc);
              lv_obj_center(img_obj_inner);
            }
          }
          
          ESP_LOGI(TAG, "开始播放动画，与音频同步");
          lastUpdateTime = currentTime;
          isAudioPlaying = true;
          pendingAnimationStart = false;
        }
      }
      
      // 动画播放逻辑
      if (shouldAnimate && !pendingAnimationStart && (currentTime - lastUpdateTime >= pdMS_TO_TICKS(150))) {
        // 根据方向更新索引
        if (directionForward) {
          currentIndex++;
          if (currentIndex >= imageArray.size() - 1) {
            directionForward = false;
          }
        } else {
          currentIndex--;
          if (currentIndex <= 0) {
            directionForward = true;
            currentIndex = 0;
          }
        }
        
        // 确保索引在有效范围内
        if (currentIndex < imageArray.size()) {
          const uint8_t* currentImage = imageArray[currentIndex];
          if (currentImage) {
            DisplayLockGuard lock(display);
            
            // 确保图片容器可见并处于合适层级，随后确保UI在前
            lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_move_to_index(img_container, 0);
            if (customDisplay) {
              customDisplay->BringBaseUIToFront();
            }
            
            lv_obj_t* img_obj_inner = lv_obj_get_child(img_container, 0);
            if (img_obj_inner) {
              img_dsc.data = currentImage;
              lv_img_set_src(img_obj_inner, &img_dsc);
              lv_obj_center(img_obj_inner);
            }
          }
        }
        
        lastUpdateTime = currentTime;
      }
      
      // 静态模式处理 - 只在预加载UI不可见时显示
      if (iot::g_image_display_mode == iot::MODE_STATIC && !isPreloadUIVisible) {
        static bool lastWasStaticMode = false;
        static const uint8_t* lastStaticImage = nullptr;
        bool isStaticMode = true;
        
        const uint8_t* staticImage = iot::g_static_image;
        if (staticImage) {
          DisplayLockGuard lock(display);
          
          // 确保图片容器可见并处于合适层级，随后确保UI在前
          lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
          lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
          lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
          lv_obj_move_to_index(img_container, 0);
          if (customDisplay) {
            customDisplay->BringBaseUIToFront();
          }
          
          lv_obj_t* img_obj_inner = lv_obj_get_child(img_container, 0);
          if (img_obj_inner) {
            img_dsc.data = staticImage;
            lv_img_set_src(img_obj_inner, &img_dsc);
            lv_obj_center(img_obj_inner);
          }
          
          if (isStaticMode != lastWasStaticMode || staticImage != lastStaticImage) {
            ESP_LOGI(TAG, "显示logo图片");
            lastWasStaticMode = isStaticMode;
            lastStaticImage = staticImage;
          }
        }
      }
      
      previousState = currentState;
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }

 public:
  NulllabAIVox()
      : boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
    
    // 创建资源检查协调信号量，初始值为0，确保预热任务等待资源检查完成
    resource_check_semaphore_ = xSemaphoreCreateBinary();
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_9),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_config));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 1));

    adc_channel_t channel;
    adc_unit_t adc_unit;
    ESP_ERROR_CHECK(
        adc_oneshot_io_to_channel(GPIO_NUM_10, &adc_unit, &channel));

    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(&adc_init_config, &battery_adc_handle_));

    adc_oneshot_chan_cfg_t adc_chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(battery_adc_handle_, channel,
                                               &adc_chan_config));

    InitializeSpi();
    InitializeLcdDisplay();
    InitializeButtons();
    InitializeIot();
    InitializeImageResources();
    StartImageSlideshow();
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      GetBacklight()->RestoreBrightness();
    }
  }

  virtual Led* GetLed() {
    static SingleLed led(BUILTIN_LED_GPIO);
    return &led;
  }

  virtual AudioCodec* GetAudioCodec() override {
    static Sph0645AudioCodec audio_codec(
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
        AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
        AUDIO_I2S_MIC_GPIO_DIN);
    return &audio_codec;
  }

  virtual Display* GetDisplay() override { return display_; }

  virtual Backlight* GetBacklight() override {
    if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
      static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN,
                                    DISPLAY_BACKLIGHT_OUTPUT_INVERT);
      return &backlight;
    }
    return nullptr;
  }

  bool GetBatteryLevel(int& level, bool& charging, bool& discharging) {
    constexpr uint32_t kMinAdcValue = 2048;
    constexpr uint32_t kMaxAdcValue = 2330;
    constexpr uint32_t kTotalBands = 10;
    constexpr uint32_t kAdcRangePerBand =
        (kMaxAdcValue - kMinAdcValue) / kTotalBands;
    constexpr uint32_t kHysteresisOffset = kAdcRangePerBand / 2;

    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 0));
    int adc_value = 0;
    adc_channel_t channel;
    adc_unit_t adc_unit;
    ESP_ERROR_CHECK(
        adc_oneshot_io_to_channel(GPIO_NUM_10, &adc_unit, &channel));
    ESP_ERROR_CHECK(adc_oneshot_read(battery_adc_handle_, channel, &adc_value));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_9, 1));

    adc_value = std::clamp<int>(adc_value, kMinAdcValue, kMaxAdcValue);

    battery_adc_samples_.push_back(adc_value);
    if (battery_adc_samples_.size() > 10) {
      battery_adc_samples_.erase(battery_adc_samples_.begin());
    }

    int32_t sum = std::accumulate(battery_adc_samples_.begin(),
                                  battery_adc_samples_.end(), 0);
    adc_value = sum / battery_adc_samples_.size();

    if (current_band_ == UINT32_MAX) {
      // Initialize the current band based on the initial ADC value
      current_band_ = (adc_value - kMinAdcValue) / kAdcRangePerBand;
      if (current_band_ >= kTotalBands) {
        current_band_ = kTotalBands - 1;
      }
    } else {
      const int32_t lower_threshold =
          kMinAdcValue + current_band_ * kAdcRangePerBand - kHysteresisOffset;
      const int32_t upper_threshold = kMinAdcValue +
                                      (current_band_ + 1) * kAdcRangePerBand +
                                      kHysteresisOffset;

      if (adc_value < lower_threshold && current_band_ > 0) {
        --current_band_;
      } else if (adc_value > upper_threshold &&
                 current_band_ < kTotalBands - 1) {
        ++current_band_;
      }
    }

    level = current_band_ * 100 / (kTotalBands - 1);
    charging = false;
    discharging = true;

    return true;
  }
  
  // 析构函数，清理资源
  ~NulllabAIVox() {
    if (resource_check_semaphore_) {
      vSemaphoreDelete(resource_check_semaphore_);
      resource_check_semaphore_ = nullptr;
    }
  }
};

DECLARE_BOARD(NulllabAIVox);
