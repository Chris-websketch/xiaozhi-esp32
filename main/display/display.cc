#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>

#include "display.h"
#include "board.h"
#include "application.h"
#include "font_awesome_symbols.h"
#include "audio_codec.h"
#include "settings.h"
#include "assets/lang_config.h"

#define TAG "Display"

Display::Display() {
    // Load theme from settings
    Settings settings("display", false);
    current_theme_name_ = settings.GetString("theme", "light");

    // Notification timer
    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            DisplayLockGuard lock(display);
            lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "notification_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    // Update display timer
    esp_timer_create_args_t update_display_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            display->Update();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "display_update_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_display_timer_args, &update_timer_));
    // 不立即启动定时器，等待音频系统初始化完成后再启动

    // Center notification timer
    esp_timer_create_args_t center_notification_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            DisplayLockGuard lock(display);
            if (display->center_notification_bg_ != nullptr) {
                lv_obj_add_flag(display->center_notification_bg_, LV_OBJ_FLAG_HIDDEN);
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "center_notification_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&center_notification_timer_args, &center_notification_timer_));

    // Create a power management lock
    auto ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "display_update", &pm_lock_);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "Power management not supported");
    } else {
        ESP_ERROR_CHECK(ret);
    }
}

Display::~Display() {
    if (notification_timer_ != nullptr) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }
    if (update_timer_ != nullptr) {
        esp_timer_stop(update_timer_);
        esp_timer_delete(update_timer_);
    }
    if (center_notification_timer_ != nullptr) {
        esp_timer_stop(center_notification_timer_);
        esp_timer_delete(center_notification_timer_);
    }

    if (network_label_ != nullptr) {
        lv_obj_del(network_label_);
        lv_obj_del(notification_label_);
        lv_obj_del(status_label_);
        lv_obj_del(mute_label_);
        lv_obj_del(battery_label_);  // 恢复电量UI显示
        lv_obj_del(battery_percentage_label_);  // 电池百分比标签清理
        lv_obj_del(emotion_label_);
    }
    if( low_battery_popup_ != nullptr ) {
        lv_obj_del(low_battery_popup_);
    }
    if (qrcode_obj_ != nullptr) {
        lv_obj_del(qrcode_obj_);
    }
    if (qrcode_hint_label_ != nullptr) {
        lv_obj_del(qrcode_hint_label_);
    }
    if (pm_lock_ != nullptr) {
        esp_pm_lock_delete(pm_lock_);
    }
}

void Display::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    if (status_label_ == nullptr) {
        return;
    }
    lv_label_set_text(status_label_, status);
    lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    DisplayLockGuard lock(this);
    if (notification_label_ == nullptr) {
        return;
    }
    lv_label_set_text(notification_label_, notification);
    lv_obj_clear_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_stop(notification_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
}

void Display::ShowCenterNotification(const std::string &notification, int duration_ms) {
    ShowCenterNotification(notification.c_str(), duration_ms);
}

void Display::ShowCenterNotification(const char* notification, int duration_ms) {
    ESP_LOGI(TAG, "ShowCenterNotification called: %s, duration: %dms", notification, duration_ms);
    DisplayLockGuard lock(this);
    if (center_notification_label_ == nullptr || center_notification_bg_ == nullptr) {
        ESP_LOGE(TAG, "Center notification objects not initialized! label=%p, bg=%p", 
                 center_notification_label_, center_notification_bg_);
        return;
    }
    ESP_LOGI(TAG, "Setting notification text and showing popup");
    lv_label_set_text(center_notification_label_, notification);
    lv_obj_clear_flag(center_notification_bg_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_stop(center_notification_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(center_notification_timer_, duration_ms * 1000));
    ESP_LOGI(TAG, "Center notification displayed successfully");
}

void Display::Update() {
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();

    {
        DisplayLockGuard lock(this);
        if (mute_label_ == nullptr) {
            return;
        }

        // 如果静音状态改变，则更新图标
        if (codec->output_volume() == 0 && !muted_) {
            muted_ = true;
            lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);
        } else if (codec->output_volume() > 0 && muted_) {
            muted_ = false;
            lv_label_set_text(mute_label_, "");
        }
    }

    esp_pm_lock_acquire(pm_lock_);
    // 电池图标更新已移至时钟页面(Tab2)的定时器中处理
    int battery_level;
    bool charging, discharging;
    const char* icon = nullptr;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        if (charging) {
            icon = FONT_AWESOME_BATTERY_CHARGING;
        } else {
            const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_1,    // 20-39%
                FONT_AWESOME_BATTERY_2,    // 40-59%
                FONT_AWESOME_BATTERY_3,    // 60-79%
                FONT_AWESOME_BATTERY_FULL, // 80-99%
                FONT_AWESOME_BATTERY_FULL, // 100%
            };
            icon = levels[battery_level / 20];
        }
        // 电池图标现在通过时钟页面定时器更新，这里只处理低电量弹窗

        if (low_battery_popup_ != nullptr) {
            if (strcmp(icon, FONT_AWESOME_BATTERY_EMPTY) == 0 && discharging) {
                if (lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // 如果低电量提示框隐藏，则显示
                    lv_obj_clear_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                    auto& app = Application::GetInstance();
                    app.PlaySound(Lang::Sounds::P3_LOW_BATTERY);
                }
            } else {
                // Hide the low battery popup when the battery is not empty
                if (!lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // 如果低电量提示框显示，则隐藏
                    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    // 升级固件时，不读取 4G 网络状态，避免占用 UART 资源
    auto device_state = Application::GetInstance().GetDeviceState();
    static const std::vector<DeviceState> allowed_states = {
        kDeviceStateIdle,
        kDeviceStateStarting,
        kDeviceStateWifiConfiguring,
        kDeviceStateListening,
        kDeviceStateActivating,
    };
    if (std::find(allowed_states.begin(), allowed_states.end(), device_state) != allowed_states.end()) {
        icon = board.GetNetworkStateIcon();
        if (network_label_ != nullptr && icon != nullptr && network_icon_ != icon) {
            DisplayLockGuard lock(this);
            network_icon_ = icon;
            lv_label_set_text(network_label_, network_icon_);
        }
    }

    esp_pm_lock_release(pm_lock_);
}

void Display::StartUpdateTimer() {
    if (update_timer_ != nullptr) {
        ESP_LOGI(TAG, "启动显示更新定时器");
        ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer_, 1000000));
    }
}

void Display::SetEmotion(const char* emotion) {
    // 此方法已被禁用，不再使用表情符号
    DisplayLockGuard lock(this);
    // 该方法现在不执行任何操作
}

void Display::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_label_set_text(emotion_label_, icon);
}

void Display::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    
    // 检查字幕是否启用
    if (!subtitle_enabled_) {
        // 字幕关闭时，隐藏chat_message_label_
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    
    // 字幕启用时，显示并设置内容
    lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(chat_message_label_, content);
}

void Display::SetTheme(const std::string& theme_name) {
    current_theme_name_ = theme_name;
    Settings settings("display", true);
    settings.SetString("theme", theme_name);
}

void Display::SetSubtitleEnabled(bool enabled) {
    subtitle_enabled_ = enabled;
    
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }
    
    if (enabled) {
        // 启用字幕，显示标签
        lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        // 禁用字幕，隐藏标签
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Display::CreateCanvas() {
    DisplayLockGuard lock(this);
    
    // 如果已经有画布，先销毁
    if (canvas_ != nullptr) {
        DestroyCanvas();
    }
    
    // 创建画布所需的缓冲区
    // 每个像素2字节(RGB565)
    size_t buf_size = width_ * height_ * 2;  // RGB565: 2 bytes per pixel
    
    // 分配内存，优先使用PSRAM
    canvas_buffer_ = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (canvas_buffer_ == nullptr) {
        ESP_LOGE("Display", "Failed to allocate canvas buffer");
        return;
    }
    
    // 获取活动屏幕
    lv_obj_t* screen = lv_screen_active();
    
    // 创建画布对象
    canvas_ = lv_canvas_create(screen);
    if (canvas_ == nullptr) {
        ESP_LOGE("Display", "Failed to create canvas");
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
        return;
    }
    
    // 初始化画布
    lv_canvas_set_buffer(canvas_, canvas_buffer_, width_, height_, LV_COLOR_FORMAT_RGB565);
    
    // 设置画布位置为全屏
    lv_obj_set_pos(canvas_, 0, 25);
    lv_obj_set_size(canvas_, width_, height_ - 25);
    
    // 设置画布为透明
    lv_canvas_fill_bg(canvas_, lv_color_make(0, 0, 0), LV_OPA_TRANSP);
    
    // 设置画布为顶层
    lv_obj_move_foreground(canvas_);
    
    ESP_LOGI("Display", "Canvas created successfully");
}

void Display::DestroyCanvas() {
    DisplayLockGuard lock(this);
    
    if (canvas_ != nullptr) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }
    
    if (canvas_buffer_ != nullptr) {
        heap_caps_free(canvas_buffer_);
        canvas_buffer_ = nullptr;
    }
    
    ESP_LOGI("Display", "Canvas destroyed");
}

void Display::DrawImageOnCanvas(int x, int y, int width, int height, const uint8_t* img_data) {
    DisplayLockGuard lock(this);
    
    // 确保有画布
    if (canvas_ == nullptr) {
        ESP_LOGE("Display", "Canvas not created");
        return;
    }
    
    
    // 创建一个描述器来映射图像数据
    const lv_image_dsc_t img_dsc = {
        .header = {
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RGB565,
            .flags = 0,
            .w = (uint32_t)width,
            .h = (uint32_t)height,
            .stride = (uint32_t)(width * 2),  // RGB565: 2 bytes per pixel
            .reserved_2 = 0,
        },
        .data_size = (uint32_t)(width * height * 2),  // RGB565: 2 bytes per pixel
        .data = img_data,
        .reserved = NULL
    };
    
    // 使用图层绘制图像到画布上
    lv_layer_t layer;
    lv_canvas_init_layer(canvas_, &layer);
    
    lv_draw_image_dsc_t draw_dsc;
    lv_draw_image_dsc_init(&draw_dsc);
    draw_dsc.src = &img_dsc;
    
    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = x + width - 1;
    area.y2 = y + height - 1;
    
    lv_draw_image(&layer, &draw_dsc, &area);
    lv_canvas_finish_layer(canvas_, &layer);
    
    // 确保画布在最上层
    lv_obj_move_foreground(canvas_);
    
    ESP_LOGI("Display", "Image drawn on canvas at x=%d, y=%d, w=%d, h=%d", x, y, width, height);
}

void Display::ShowQRCode(const char* data, int x, int y, int size) {
    DisplayLockGuard lock(this);
    
    if (!lock.IsLocked()) {
        ESP_LOGE(TAG, "Failed to lock display for QR code");
        return;
    }
    
#if LV_USE_QRCODE
    // 如果已存在二维码对象，先销毁
    if (qrcode_obj_ != nullptr) {
        lv_obj_del(qrcode_obj_);
        qrcode_obj_ = nullptr;
    }
    
    // 计算合适的二维码大小（缩小版本）
    int qr_size = size;
    if (qr_size <= 0) {
        // 根据屏幕尺寸自动计算
        int min_dimension = (width_ < height_) ? width_ : height_;
        if (min_dimension >= 240) {
            qr_size = 90;  // 大屏：90x90（原120，缩小25%）
        } else if (min_dimension >= 160) {
            qr_size = 75;  // 中屏：75x75（原100，缩小25%）
        } else {
            qr_size = min_dimension * 50 / 100;  // 小屏：占50%（原70%）
        }
    }
    
    // 创建二维码对象
    lv_obj_t* screen = lv_screen_active();
    qrcode_obj_ = lv_qrcode_create(screen);
    
    if (qrcode_obj_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create QR code object");
        return;
    }
    
    // 设置二维码大小
    lv_qrcode_set_size(qrcode_obj_, qr_size);
    
    // 设置颜色（黑白）
    lv_qrcode_set_dark_color(qrcode_obj_, lv_color_black());
    lv_qrcode_set_light_color(qrcode_obj_, lv_color_white());
    
    // 更新二维码数据
    lv_result_t result = lv_qrcode_update(qrcode_obj_, data, strlen(data));
    if (result != LV_RESULT_OK) {
        ESP_LOGE(TAG, "Failed to update QR code data");
        lv_obj_del(qrcode_obj_);
        qrcode_obj_ = nullptr;
        return;
    }
    
    // 设置位置
    if (x < 0 || y < 0) {
        // 水平居中，垂直向上偏移（减少偏移量，使二维码更靠下）
        int pos_x = (width_ - qr_size) / 2;
        int pos_y = (height_ - qr_size) / 2 - 25;  // 向上移动15像素（原30像素）
        // 确保不超出屏幕顶部
        if (pos_y < 25) {
            pos_y = 25;  // 留出顶部状态栏空间
        }
        lv_obj_set_pos(qrcode_obj_, pos_x, pos_y);
    } else {
        lv_obj_set_pos(qrcode_obj_, x, y);
    }
    
    // 添加白色边框
    lv_obj_set_style_border_color(qrcode_obj_, lv_color_white(), 0);
    lv_obj_set_style_border_width(qrcode_obj_, 5, 0);
    
    // 创建提示文字标签（在二维码上方）
    if (qrcode_hint_label_ != nullptr) {
        lv_obj_del(qrcode_hint_label_);
        qrcode_hint_label_ = nullptr;
    }
    
    qrcode_hint_label_ = lv_label_create(screen);
    if (qrcode_hint_label_ != nullptr) {
        lv_label_set_text(qrcode_hint_label_, Lang::Strings::SCAN_QR_CODE);
        lv_obj_set_style_text_color(qrcode_hint_label_, lv_color_black(), 0);  // 改为黑色
        lv_obj_set_style_text_align(qrcode_hint_label_, LV_TEXT_ALIGN_CENTER, 0);
        
        // 禁止换行，强制在一行显示
        lv_label_set_long_mode(qrcode_hint_label_, LV_LABEL_LONG_CLIP);
        
        // 居中对齐，放在二维码上方紧贴
        lv_obj_set_width(qrcode_hint_label_, LV_SIZE_CONTENT);  // 自动宽度，根据内容
        lv_obj_align_to(qrcode_hint_label_, qrcode_obj_, LV_ALIGN_OUT_TOP_MID, 0, 9);
        
        // 确保在顶层
        lv_obj_move_foreground(qrcode_hint_label_);
        
        ESP_LOGI(TAG, "QR code hint label created");
    }
    
    // 确保二维码在顶层
    lv_obj_move_foreground(qrcode_obj_);
    
    ESP_LOGI(TAG, "QR code displayed: size=%d, data=%s", qr_size, data);
#else
    ESP_LOGW(TAG, "QR code support not enabled (LV_USE_QRCODE=0)");
#endif
}

void Display::HideQRCode() {
    DisplayLockGuard lock(this);
    
#if LV_USE_QRCODE
    if (qrcode_obj_ != nullptr) {
        lv_obj_del(qrcode_obj_);
        qrcode_obj_ = nullptr;
        ESP_LOGI(TAG, "QR code hidden");
    }
    
    if (qrcode_hint_label_ != nullptr) {
        lv_obj_del(qrcode_hint_label_);
        qrcode_hint_label_ = nullptr;
        ESP_LOGI(TAG, "QR code hint label hidden");
    }
#endif
}
