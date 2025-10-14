#include "lcd_display.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"
#include <cstring>
#include "settings.h"
#include <mutex>
#include <cJSON.h>
#include <esp_wifi.h>
#include <lvgl.h>

#include "board.h"

#define TAG "LcdDisplay"

// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR       lv_color_hex(0x121212)     // Dark background
#define DARK_TEXT_COLOR             lv_color_white()           // White text
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0x1E1E1E)     // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)     // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)     // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)     // Medium gray
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)     // Light gray text
#define DARK_BORDER_COLOR           lv_color_hex(0x333333)     // Dark gray border
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)     // Red for dark mode

// Color definitions for light theme
#define LIGHT_BACKGROUND_COLOR       lv_color_white()           // White background
#define LIGHT_TEXT_COLOR             lv_color_black()           // Black text
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)     // Light gray background
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)     // WeChat green
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()           // White
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)     // Light gray
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)     // Dark gray text
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)     // Light gray border
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()           // Black for light mode

// Theme color structure
struct ThemeColors {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
};

// Define dark theme colors
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR
};

// Define light theme colors
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR
};

// Current theme - initialize based on default config
static ThemeColors current_theme = LIGHT_THEME;


LV_FONT_DECLARE(font_awesome_30_4);

SpiLcdDisplay::SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }

#if CONFIG_BOARD_TYPE_ESP32S3_Touch_LCD_1_46A
#else
    SetupUI();
#endif

}

// RGB LCD实现
RgbLcdDisplay::RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplay(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;
    
    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }

    SetupUI();
}

LcdDisplay::~LcdDisplay() {
    // 然后再清理 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
}

bool LcdDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplay::Unlock() {
    lvgl_port_unlock();
}

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_style_border_color(container_, current_theme.border, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
    lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
    
    /* Content - Chat area */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, 10, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0); // Background for chat area
    lv_obj_set_style_border_color(content_, current_theme.border, 0); // Border color for chat area

    // Enable scrolling for chat content
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    
    // Create a flex container for chat messages
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(content_, 10, 0); // Space between messages

    // We'll create chat messages dynamically in SetChatMessage
    chat_message_label_ = nullptr;

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 10, 0);
    lv_obj_set_style_pad_right(status_bar_, 10, 0);
    lv_obj_set_style_pad_top(status_bar_, 2, 0);
    lv_obj_set_style_pad_bottom(status_bar_, 2, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    // 设置状态栏的内容垂直居中
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 移除表情符号标签

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
    lv_obj_set_style_margin_left(network_label_, 5, 0); // 添加左边距，与前面的元素分隔

    // 电池标签 - 显示在状态栏最右边
    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);
    lv_obj_set_style_margin_left(battery_label_, 5, 0); // 添加左边距，与前面的元素分隔

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // 创建中央通知弹窗
    // 半透明背景遮罩层
    center_notification_bg_ = lv_obj_create(screen);
    lv_obj_set_size(center_notification_bg_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(center_notification_bg_, 0, 0);
    lv_obj_set_style_bg_color(center_notification_bg_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(center_notification_bg_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(center_notification_bg_, 0, 0);
    lv_obj_set_scrollbar_mode(center_notification_bg_, LV_SCROLLBAR_MODE_OFF);

    // 弹窗容器（固定白底黑字，不受主题影响）
    center_notification_popup_ = lv_obj_create(center_notification_bg_);
    lv_obj_set_size(center_notification_popup_, LV_HOR_RES * 0.85, LV_SIZE_CONTENT);
    lv_obj_align(center_notification_popup_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_notification_popup_, lv_color_white(), 0);
    lv_obj_set_style_radius(center_notification_popup_, 15, 0);
    lv_obj_set_style_pad_all(center_notification_popup_, 20, 0);
    lv_obj_set_style_border_width(center_notification_popup_, 2, 0);
    lv_obj_set_style_border_color(center_notification_popup_, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_shadow_width(center_notification_popup_, 20, 0);
    lv_obj_set_style_shadow_color(center_notification_popup_, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(center_notification_popup_, LV_OPA_30, 0);
    lv_obj_set_scrollbar_mode(center_notification_popup_, LV_SCROLLBAR_MODE_OFF);

    // 文本标签（固定黑色）
    center_notification_label_ = lv_label_create(center_notification_popup_);
    lv_label_set_long_mode(center_notification_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(center_notification_label_, LV_HOR_RES * 0.85 - 40);
    lv_obj_set_style_text_align(center_notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(center_notification_label_, lv_color_black(), 0);
    lv_label_set_text(center_notification_label_, "");
    lv_obj_center(center_notification_label_);

    // 初始隐藏
    lv_obj_add_flag(center_notification_bg_, LV_OBJ_FLAG_HIDDEN);
}

#define  MAX_MESSAGES 20
void LcdDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (content_ == nullptr) {
        return;
    }
    
    //避免出现空的消息框
    if(strlen(content) == 0) return;
    
    // 检查消息数量是否超过限制
    uint32_t child_count = lv_obj_get_child_cnt(content_);
    if (child_count >= MAX_MESSAGES) {
        // 删除最早的消息（第一个子对象）
        lv_obj_t* first_child = lv_obj_get_child(content_, 0);
        lv_obj_t* last_child = lv_obj_get_child(content_, child_count - 1);
        if (first_child != nullptr) {
            lv_obj_del(first_child);
        }
        // Scroll to the last message immediately
        if (last_child != nullptr) {
            lv_obj_scroll_to_view_recursive(last_child, LV_ANIM_OFF);
        }
    }
    
    // Create a message bubble
    lv_obj_t* msg_bubble = lv_obj_create(content_);
    lv_obj_set_style_radius(msg_bubble, 8, 0);
    lv_obj_set_scrollbar_mode(msg_bubble, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(msg_bubble, 1, 0);
    lv_obj_set_style_border_color(msg_bubble, current_theme.border, 0);
    lv_obj_set_style_pad_all(msg_bubble, 8, 0);

    // Create the message text
    lv_obj_t* msg_text = lv_label_create(msg_bubble);
    lv_label_set_text(msg_text, content);
    
    // 计算文本实际宽度
    lv_coord_t text_width = lv_txt_get_width(content, strlen(content), fonts_.text_font, 0);

    // 计算气泡宽度
    lv_coord_t max_width = LV_HOR_RES * 85 / 100 - 16;  // 屏幕宽度的85%
    lv_coord_t min_width = 20;  
    lv_coord_t bubble_width;
    
    // 确保文本宽度不小于最小宽度
    if (text_width < min_width) {
        text_width = min_width;
    }

    // 如果文本宽度小于最大宽度，使用文本宽度
    if (text_width < max_width) {
        bubble_width = text_width; 
    } else {
        bubble_width = max_width;
    }
    
    // 设置消息文本的宽度
    lv_obj_set_width(msg_text, bubble_width);  // 减去padding
    lv_label_set_long_mode(msg_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg_text, fonts_.text_font, 0);

    // 设置气泡宽度
    lv_obj_set_width(msg_bubble, bubble_width);
    lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);

    // Set alignment and style based on message role
    if (strcmp(role, "user") == 0) {
        // User messages are right-aligned with green background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.user_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"user");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "assistant") == 0) {
        // Assistant messages are left-aligned with white background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.assistant_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"assistant");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    } else if (strcmp(role, "system") == 0) {
        // System messages are center-aligned with light gray background
        lv_obj_set_style_bg_color(msg_bubble, current_theme.system_bubble, 0);
        // Set text color for contrast
        lv_obj_set_style_text_color(msg_text, current_theme.system_text, 0);
        
        // 设置自定义属性标记气泡类型
        lv_obj_set_user_data(msg_bubble, (void*)"system");
        
        // Set appropriate width for content
        lv_obj_set_width(msg_bubble, LV_SIZE_CONTENT);
        lv_obj_set_height(msg_bubble, LV_SIZE_CONTENT);
        
        // Don't grow
        lv_obj_set_style_flex_grow(msg_bubble, 0, 0);
    }
    
    // Create a full-width container for user messages to ensure right alignment
    if (strcmp(role, "user") == 0) {
        // Create a full-width container
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // Make container transparent and borderless
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // Move the message bubble into this container
        lv_obj_set_parent(msg_bubble, container);
        
        // Right align the bubble in the container
        lv_obj_align(msg_bubble, LV_ALIGN_RIGHT_MID, -25, 0);
        
        // Auto-scroll to this container
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else if (strcmp(role, "system") == 0) {
        // 为系统消息创建全宽容器以确保居中对齐
        lv_obj_t* container = lv_obj_create(content_);
        lv_obj_set_width(container, LV_HOR_RES);
        lv_obj_set_height(container, LV_SIZE_CONTENT);
        
        // 使容器透明且无边框
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // 将消息气泡移入此容器
        lv_obj_set_parent(msg_bubble, container);
        
        // 将气泡居中对齐在容器中
        lv_obj_align(msg_bubble, LV_ALIGN_CENTER, 0, 0);
        
        // 自动滚动底部
        lv_obj_scroll_to_view_recursive(container, LV_ANIM_ON);
    } else {
        // For assistant messages
        // Left align assistant messages
        lv_obj_align(msg_bubble, LV_ALIGN_LEFT_MID, 0, 0);

        // Auto-scroll to the message bubble
        lv_obj_scroll_to_view_recursive(msg_bubble, LV_ANIM_ON);
    }
    
    // Store reference to the latest message label
    chat_message_label_ = msg_text;
}
#else
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_style_border_color(container_, current_theme.border, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, fonts_.text_font->line_height);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
    lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_style_pad_all(content_, 5, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
    lv_obj_set_style_border_color(content_, current_theme.border, 0); // Border color for content

    // 新增：启用纵向滚动并隐藏滚动条，防止内容撑大布局
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

        // 移除表情标签创建

    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9); // 限制宽度为屏幕宽度的 90%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐
    lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);
    lv_obj_set_style_pad_left(status_bar_, 2, 0);
    lv_obj_set_style_pad_right(status_bar_, 2, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, current_theme.text, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);

    // 电池标签 - 显示在状态栏最右边
    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // 创建中央通知弹窗
    // 半透明背景遮罩层
    center_notification_bg_ = lv_obj_create(screen);
    lv_obj_set_size(center_notification_bg_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(center_notification_bg_, 0, 0);
    lv_obj_set_style_bg_color(center_notification_bg_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(center_notification_bg_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(center_notification_bg_, 0, 0);
    lv_obj_set_scrollbar_mode(center_notification_bg_, LV_SCROLLBAR_MODE_OFF);

    // 弹窗容器（固定白底黑字，不受主题影响）
    center_notification_popup_ = lv_obj_create(center_notification_bg_);
    lv_obj_set_size(center_notification_popup_, LV_HOR_RES * 0.85, LV_SIZE_CONTENT);
    lv_obj_align(center_notification_popup_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_notification_popup_, lv_color_white(), 0);
    lv_obj_set_style_radius(center_notification_popup_, 15, 0);
    lv_obj_set_style_pad_all(center_notification_popup_, 20, 0);
    lv_obj_set_style_border_width(center_notification_popup_, 2, 0);
    lv_obj_set_style_border_color(center_notification_popup_, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_shadow_width(center_notification_popup_, 20, 0);
    lv_obj_set_style_shadow_color(center_notification_popup_, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(center_notification_popup_, LV_OPA_30, 0);
    lv_obj_set_scrollbar_mode(center_notification_popup_, LV_SCROLLBAR_MODE_OFF);

    // 文本标签（固定黑色）
    center_notification_label_ = lv_label_create(center_notification_popup_);
    lv_label_set_long_mode(center_notification_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(center_notification_label_, LV_HOR_RES * 0.85 - 40);
    lv_obj_set_style_text_align(center_notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(center_notification_label_, lv_color_black(), 0);
    lv_label_set_text(center_notification_label_, "");
    lv_obj_center(center_notification_label_);

    // 初始隐藏
    lv_obj_add_flag(center_notification_bg_, LV_OBJ_FLAG_HIDDEN);
}
#endif

void LcdDisplay::SetEmotion(const char* emotion) {
    // 此方法已被禁用，不再使用表情符号
    DisplayLockGuard lock(this);
    // 该方法现在不执行任何操作
}

void LcdDisplay::SetIcon(const char* icon) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr) {
        return;
    }
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_4, 0);
    lv_label_set_text(emotion_label_, icon);
}

void LcdDisplay::SetTheme(const std::string& theme_name) {
    DisplayLockGuard lock(this);
    
    if (theme_name == "dark" || theme_name == "DARK") {
        current_theme = DARK_THEME;
    } else if (theme_name == "light" || theme_name == "LIGHT") {
        current_theme = LIGHT_THEME;
    } else {
        // Invalid theme name, return false
        ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
        return;
    }
    
    // Get the active screen
    lv_obj_t* screen = lv_screen_active();
    
    // Update the screen colors
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    
    // Update container colors
    if (container_ != nullptr) {
        lv_obj_set_style_bg_color(container_, current_theme.background, 0);
        lv_obj_set_style_border_color(container_, current_theme.border, 0);
    }
    
    // Update status bar colors
    if (status_bar_ != nullptr) {
        lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        
        // Update status bar elements
        if (network_label_ != nullptr) {
            lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
        }
        if (status_label_ != nullptr) {
            lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
        }
        if (mute_label_ != nullptr) {
            lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);
        }
        if (battery_label_ != nullptr) {
            lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);
        }
    }
    
    // Update content area colors
    if (content_ != nullptr) {
        lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
        lv_obj_set_style_border_color(content_, current_theme.border, 0);
        
        // If we have the chat message style, update all message bubbles
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        // Iterate through all children of content (message containers or bubbles)
        uint32_t child_count = lv_obj_get_child_cnt(content_);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* obj = lv_obj_get_child(content_, i);
            if (obj == nullptr) continue;
            
            lv_obj_t* bubble = nullptr;
            
            // 检查这个对象是容器还是气泡
            // 如果是容器（用户或系统消息），则获取其子对象作为气泡
            // 如果是气泡（助手消息），则直接使用
            if (lv_obj_get_child_cnt(obj) > 0) {
                // 可能是容器，检查它是否为用户或系统消息容器
                // 用户和系统消息容器是透明的
                lv_opa_t bg_opa = lv_obj_get_style_bg_opa(obj, 0);
                if (bg_opa == LV_OPA_TRANSP) {
                    // 这是用户或系统消息的容器
                    bubble = lv_obj_get_child(obj, 0);
                } else {
                    // 这可能是助手消息的气泡自身
                    bubble = obj;
                }
            } else {
                // 没有子元素，可能是其他UI元素，跳过
                continue;
            }
            
            if (bubble == nullptr) continue;
            
            // 使用保存的用户数据来识别气泡类型
            void* bubble_type_ptr = lv_obj_get_user_data(bubble);
            if (bubble_type_ptr != nullptr) {
                const char* bubble_type = static_cast<const char*>(bubble_type_ptr);
                
                // 根据气泡类型应用正确的颜色
                if (strcmp(bubble_type, "user") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (strcmp(bubble_type, "assistant") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0); 
                } else if (strcmp(bubble_type, "system") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 根据气泡类型设置文本颜色
                        if (strcmp(bubble_type, "system") == 0) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            } else {
                // 如果没有标记，回退到之前的逻辑（颜色比较）
                // ...保留原有的回退逻辑...
                lv_color_t bg_color = lv_obj_get_style_bg_color(bubble, 0);
            
                // 改进bubble类型检测逻辑，不仅使用颜色比较
                bool is_user_bubble = false;
                bool is_assistant_bubble = false;
                bool is_system_bubble = false;
            
                // 检查用户bubble
                if (lv_color_eq(bg_color, DARK_USER_BUBBLE_COLOR) || 
                    lv_color_eq(bg_color, LIGHT_USER_BUBBLE_COLOR) ||
                    lv_color_eq(bg_color, current_theme.user_bubble)) {
                    is_user_bubble = true;
                }
                // 检查系统bubble
                else if (lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                         lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR) ||
                         lv_color_eq(bg_color, current_theme.system_bubble)) {
                    is_system_bubble = true;
                }
                // 剩余的都当作助手bubble处理
                else {
                    is_assistant_bubble = true;
                }
            
                // 根据bubble类型应用正确的颜色
                if (is_user_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (is_assistant_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0);
                } else if (is_system_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 回退到颜色检测逻辑
                        if (lv_color_eq(bg_color, current_theme.system_bubble) ||
                            lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                            lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR)) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            }
        }
#else
        // Simple UI mode - just update the main chat message
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        }
        
        if (emotion_label_ != nullptr) {
            lv_obj_set_style_text_color(emotion_label_, current_theme.text, 0);
        }
#endif
    }
    
    // Update low battery popup
    if (low_battery_popup_ != nullptr) {
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    }

    // No errors occurred. Save theme to settings
    Display::SetTheme(theme_name);
}

// ==================== 天气时钟功能实现 ====================

// 农历缓存结构
struct LunarCache {
    std::string date;
    std::string lunar_str;
    std::string ganzhi_year;
};

// 全局变量
static LunarCache g_lunar_cache;
static std::mutex g_lunar_mutex;
static std::vector<std::string> g_lunar_yi;
static std::vector<std::string> g_lunar_ji;

// 声明天气图标和时钟字体
LV_IMAGE_DECLARE(sun);
LV_IMAGE_DECLARE(cloud);
LV_IMAGE_DECLARE(rain);
LV_IMAGE_DECLARE(Snow);
LV_IMAGE_DECLARE(fog);
LV_IMAGE_DECLARE(Dust);
LV_IMAGE_DECLARE(hail);
LV_IMAGE_DECLARE(thunder);
LV_IMAGE_DECLARE(Negative);

LV_FONT_DECLARE(time50);
LV_FONT_DECLARE(time40);
LV_FONT_DECLARE(time30);
LV_FONT_DECLARE(time20);

// 天气图标映射
static const struct {
    const char* code;
    const lv_image_dsc_t* image;
} weather_icons[] = {
    {"100", &sun},
    {"101", &cloud},
    {"104", &cloud},
    {"300", &rain},
    {"302", &thunder},
    {"305", &rain},
    {"306", &rain},
    {"307", &rain},
    {"400", &Snow},
    {"401", &Snow},
    {"501", &fog},
    {"502", &Dust},
    {"503", &Dust},
    {"504", &fog},
    {"999", &Negative},
};

// 获取天气图标
static const lv_image_dsc_t* getWeatherIcon(const std::string& code) {
    for (const auto& item : weather_icons) {
        if (item.code == code) {
            return item.image;
        }
    }
    return &cloud;
}

// 农历获取任务
static void LunarFetchTask(void*) {
    ESP_LOGI(TAG, "Lunar fetch task started, waiting for WiFi connection...");
    
    // 等待WiFi连接（最多30秒）
    int wait_count = 0;
    while (wait_count < 30) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected, starting lunar data fetch");
            break;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        wait_count++;
    }
    
    if (wait_count >= 30) {
        ESP_LOGW(TAG, "WiFi not connected after 30 seconds, will retry later");
    }
    
    while (1) {
        wifi_ap_record_t ap_info;
        bool wifi_connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
        
        if (!wifi_connected) {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }
        
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        char ymd[16];
        strftime(ymd, sizeof(ymd), "%Y-%m-%d", timeinfo);
        std::string today = ymd;
        
        bool need_query = false;
        {
            std::lock_guard<std::mutex> lock(g_lunar_mutex);
            if (g_lunar_cache.date != today) {
                need_query = true;
            }
        }
        
        if (need_query) {
            std::string alapi_token = "aswutdoxli64xvjqy3rxswygvdb75u";
            std::string url = "https://v2.alapi.cn/api/lunar?token=" + alapi_token;
            
            ESP_LOGI(TAG, "Fetching lunar data from: %s", url.c_str());
            
            auto http = Board::GetInstance().CreateHttp();
            if (http && http->Open("GET", url)) {
                std::string response = http->GetBody();
                http->Close();
                delete http;
                
                ESP_LOGI(TAG, "Lunar API response: %s", response.c_str());
                
                cJSON* root = cJSON_Parse(response.c_str());
                if (root) {
                    // 检查API返回状态
                    cJSON* code = cJSON_GetObjectItem(root, "code");
                    if (code && code->valueint != 200) {
                        ESP_LOGE(TAG, "Lunar API error code: %d", code->valueint);
                        cJSON* msg = cJSON_GetObjectItem(root, "msg");
                        if (msg && msg->valuestring) {
                            ESP_LOGE(TAG, "Lunar API error msg: %s", msg->valuestring);
                        }
                        cJSON_Delete(root);
                        vTaskDelay(60000 / portTICK_PERIOD_MS);
                        continue;
                    }
                    
                    cJSON* data = cJSON_GetObjectItem(root, "data");
                    if (data) {
                        cJSON* lunar_month = cJSON_GetObjectItem(data, "lunar_month_chinese");
                        cJSON* lunar_day = cJSON_GetObjectItem(data, "lunar_day_chinese");
                        
                        if (!lunar_month || !lunar_month->valuestring || !lunar_day || !lunar_day->valuestring) {
                            ESP_LOGE(TAG, "Lunar data missing month or day field");
                            cJSON_Delete(root);
                            vTaskDelay(60000 / portTICK_PERIOD_MS);
                            continue;
                        }
                        
                        std::string lunar_str = std::string(lunar_month->valuestring) + lunar_day->valuestring;
                        
                        cJSON* ganzhi_year_json = cJSON_GetObjectItem(data, "ganzhi_year");
                        std::string ganzhi_year = "";
                        if (ganzhi_year_json && ganzhi_year_json->valuestring) {
                            ganzhi_year = ganzhi_year_json->valuestring;
                        }
                        
                        cJSON* yi_arr = cJSON_GetObjectItem(data, "yi");
                        cJSON* ji_arr = cJSON_GetObjectItem(data, "ji");
                        
                        std::lock_guard<std::mutex> lock(g_lunar_mutex);
                        g_lunar_yi.clear();
                        g_lunar_ji.clear();
                        
                        if (yi_arr && cJSON_IsArray(yi_arr)) {
                            for (int i = 0; i < cJSON_GetArraySize(yi_arr); ++i) {
                                cJSON* item = cJSON_GetArrayItem(yi_arr, i);
                                if (item && item->valuestring) {
                                    g_lunar_yi.push_back(item->valuestring);
                                }
                            }
                        }
                        
                        if (ji_arr && cJSON_IsArray(ji_arr)) {
                            for (int i = 0; i < cJSON_GetArraySize(ji_arr); ++i) {
                                cJSON* item = cJSON_GetArrayItem(ji_arr, i);
                                if (item && item->valuestring) {
                                    g_lunar_ji.push_back(item->valuestring);
                                }
                            }
                        }
                        
                        g_lunar_cache.date = today;
                        g_lunar_cache.lunar_str = lunar_str;
                        g_lunar_cache.ganzhi_year = ganzhi_year;
                        
                        ESP_LOGI(TAG, "Lunar data updated: %s %s", ganzhi_year.c_str(), lunar_str.c_str());
                    } else {
                        ESP_LOGE(TAG, "Lunar API: data field not found");
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGE(TAG, "Failed to parse lunar JSON response");
                }
            } else {
                ESP_LOGE(TAG, "Failed to open lunar API connection");
            }
        }
        
        vTaskDelay(24 * 60 * 60 * 1000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief 初始化天气时钟界面
 * 
 * 创建一个包含以下区域的全屏天气时钟界面：
 * 1. 顶部区域（高40px）：左侧显示城市名称，右侧显示日期星期
 * 2. 主时间卡片（高95px）：大字号时间显示 + 宜忌信息 + 农历日期
 * 3. 天气信息卡片（高65px）：天气图标、天气描述、当前温度、温度范围
 * 4. 详细参数区域（高70px）：体感温度、湿度、风力、风向四个指标
 * 
 * UI布局采用LVGL的flex布局和align对齐方式，整体采用卡片式设计
 */
void LcdDisplay::SetupWeatherClock() {
    // 防止重复创建：如果时钟屏幕已存在则直接返回
    if (clock_screen_ != nullptr) {
        return;
    }
    
    // 获取显示锁，确保LVGL操作的线程安全
    DisplayLockGuard lock(this);
    
    // 保存当前活动的主屏幕引用，用于后续切换回主界面
    main_screen_ = lv_screen_active();
    
    // ========== 创建时钟屏幕基础容器 ==========
    clock_screen_ = lv_obj_create(NULL);  // 创建独立的屏幕对象（不依附于其他父对象）
    lv_obj_set_style_bg_color(clock_screen_, lv_color_white(), 0);  // 设置白色背景
    lv_obj_set_scrollbar_mode(clock_screen_, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    
    // ========== 1. 顶部信息栏：城市名称和日期星期 ==========
    // 创建顶部容器（透明背景，无边框）
    lv_obj_t* top_area = lv_obj_create(clock_screen_);
    lv_obj_set_size(top_area, 240, 40);  // 全屏宽度240px，高度40px
    lv_obj_set_style_bg_opa(top_area, LV_OPA_0, 0);  // 完全透明背景
    lv_obj_set_style_border_width(top_area, 0, 0);  // 无边框
    lv_obj_set_scrollbar_mode(top_area, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_align(top_area, LV_ALIGN_TOP_MID, 0, 10);  // 顶部居中对齐，向下偏移10px
    
    // 左侧：城市名称标签（通过天气API获取）
    weather_city_ = lv_label_create(top_area);
    lv_obj_set_style_text_font(weather_city_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_city_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_city_, "加载中...");  // 初始占位文本
    lv_obj_align(weather_city_, LV_ALIGN_LEFT_MID, 10, 0);  // 左侧居中对齐，右移10px
    
    // 右侧：日期星期标签（格式：MM月DD日 周X）
    clock_date_label_ = lv_label_create(top_area);
    lv_obj_set_style_text_font(clock_date_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(clock_date_label_, lv_color_make(80, 80, 80), 0);  // 中灰色文字
    lv_label_set_text(clock_date_label_, "12月25日 周一");  // 初始占位文本
    lv_obj_align(clock_date_label_, LV_ALIGN_RIGHT_MID, -10, 0);  // 右侧居中对齐，左移10px
    
    // ========== 2. 主时间卡片：时间显示 + 黄历宜忌 + 农历日期 ==========
    // 创建主卡片容器（浅灰色背景，圆角，带阴影）
    lv_obj_t* main_card = lv_obj_create(clock_screen_);
    lv_obj_set_size(main_card, 220, 95);  // 宽220px，高95px
    lv_obj_set_style_bg_color(main_card, lv_color_make(245, 245, 245), 0);  // 浅灰色背景
    lv_obj_set_style_radius(main_card, 15, 0);  // 圆角半径15px
    lv_obj_set_style_shadow_width(main_card, 10, 0);  // 阴影扩散宽度10px
    lv_obj_set_style_shadow_opa(main_card, LV_OPA_10, 0);  // 阴影透明度10%（轻微阴影）
    lv_obj_set_style_shadow_ofs_y(main_card, 3, 0);  // 阴影向下偏移3px（营造立体感）
    lv_obj_set_style_border_width(main_card, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(main_card, 0, 0);  // 无内边距（子元素手动定位）
    lv_obj_set_scrollbar_mode(main_card, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_align(main_card, LV_ALIGN_TOP_MID, 0, 50);  // 顶部居中，向下偏移50px
    
    // ------ 2.1 时间和宜忌的水平容器（flex行布局）------
    lv_obj_t* time_row = lv_obj_create(main_card);
    lv_obj_set_size(time_row, 210, 45);  // 宽210px，高45px
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(time_row, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(time_row, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(time_row, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);  // 设置为水平flex布局
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 主轴左对齐，交叉轴居中
    lv_obj_align(time_row, LV_ALIGN_TOP_LEFT, 10, 5);  // 左上角对齐，右移10px下移5px
    
    // 当前时间显示（大字号，格式：HH:MM）
    clock_time_label_ = lv_label_create(time_row);
    lv_obj_set_style_text_font(clock_time_label_, &time40, 0);  // 使用40号时间字体
    lv_obj_set_style_text_color(clock_time_label_, lv_color_make(30, 30, 30), 0);  // 深色文字
    lv_label_set_text(clock_time_label_, "00:00");  // 初始占位文本
    lv_obj_set_width(clock_time_label_, 105);  // 固定宽度105px（容纳HH:MM）
    lv_obj_set_style_text_align(clock_time_label_, LV_TEXT_ALIGN_CENTER, 0);  // 时间文字居中对齐
    
    // 黄历宜忌的垂直容器（flex列布局）
    lv_obj_t* yi_ji_col = lv_obj_create(time_row);
    lv_obj_set_size(yi_ji_col, 90, 45);  // 宽90px，高45px
    lv_obj_set_style_bg_opa(yi_ji_col, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(yi_ji_col, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(yi_ji_col, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(yi_ji_col, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(yi_ji_col, LV_FLEX_FLOW_COLUMN);  // 设置为垂直flex布局
    lv_obj_set_flex_align(yi_ji_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);  // 主轴和交叉轴都靠上对齐
    lv_obj_set_style_pad_row(yi_ji_col, -8, 0);  // 行间距为-5px（紧凑排列）

    // "宜"标签（绿色，显示当日宜做之事）
    yi_label_ = lv_label_create(yi_ji_col);
    lv_obj_set_style_text_font(yi_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(yi_label_, lv_color_make(30, 120, 30), 0);  // 深绿色文字（寓意吉利）
    lv_label_set_text(yi_label_, "宜：- -");  // 初始占位文本
    lv_obj_set_width(yi_label_, 90);  // 宽度90px
    lv_obj_set_style_text_align(yi_label_, LV_TEXT_ALIGN_LEFT, 0);  // 左对齐
    
    // "忌"标签（红色，显示当日忌做之事）
    ji_label_ = lv_label_create(yi_ji_col);
    lv_obj_set_style_text_font(ji_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(ji_label_, lv_color_make(180, 30, 30), 0);  // 深红色文字（寓意警示）
    lv_label_set_text(ji_label_, "忌：- -");  // 初始占位文本
    lv_obj_set_width(ji_label_, 90);  // 宽度90px
    lv_obj_set_style_text_align(ji_label_, LV_TEXT_ALIGN_LEFT, 0);  // 左对齐
    
    // ------ 2.2 农历日期标签（位于时间行下方）------
    lunar_date_label_ = lv_label_create(main_card);
    lv_obj_set_style_text_font(lunar_date_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(lunar_date_label_, lv_color_make(100, 100, 100), 0);  // 中灰色文字
    lv_obj_set_style_pad_all(lunar_date_label_, 0, 0);  // 设置内边距为0
    lv_label_set_text(lunar_date_label_, "腊月廿三");  // 初始占位文本
    lv_obj_set_width(lunar_date_label_, 200);  // 宽度200px
    lv_label_set_long_mode(lunar_date_label_, LV_LABEL_LONG_SCROLL);  // 文字过长时滚动显示
    lv_obj_set_style_text_align(lunar_date_label_, LV_TEXT_ALIGN_CENTER, 0);  // 居中对齐
    lv_obj_align_to(lunar_date_label_, main_card, LV_ALIGN_BOTTOM_MID, 0, -5);  // 相对main_card底部居中，向上偏移5px
    
    // ========== 3. 天气信息卡片：图标 + 天气 + 温度 + 温度范围 ==========
    // 创建天气卡片容器（白色背景，圆角，浅色边框）
    lv_obj_t* weather_card = lv_obj_create(clock_screen_);
    lv_obj_set_size(weather_card, 220, 65);  // 宽220px，高65px
    lv_obj_set_style_bg_color(weather_card, lv_color_white(), 0);  // 白色背景
    lv_obj_set_style_radius(weather_card, 12, 0);  // 圆角半径12px
    lv_obj_set_style_border_width(weather_card, 1, 0);  // 边框宽度1px
    lv_obj_set_style_border_color(weather_card, lv_color_make(230, 230, 230), 0);  // 浅灰色边框
    lv_obj_set_style_pad_all(weather_card, 8, 0);  // 内边距8px
    lv_obj_set_scrollbar_mode(weather_card, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_align(weather_card, LV_ALIGN_TOP_MID, 0, 155);  // 顶部居中，向下偏移155px
    
    // 设置flex布局：水平均匀分布所有天气元素
    lv_obj_set_flex_flow(weather_card, LV_FLEX_FLOW_ROW);  // 水平flex布局
    lv_obj_set_flex_align(weather_card, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 均匀分布，垂直居中
    
    // 天气图标（45x45px，根据天气类型动态切换）
    weather_icon_ = lv_image_create(weather_card);
    lv_image_set_src(weather_icon_, &cloud);  // 默认显示云朵图标
    lv_obj_set_size(weather_icon_, 45, 45);  // 图标尺寸45x45px
    
    // 天气描述文字（如"多云"、"晴"、"雨"等）
    weather_text_ = lv_label_create(weather_card);
    lv_obj_set_style_text_font(weather_text_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_text_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_text_, "多云");  // 初始占位文本
    
    // 当前温度（大字号显示，带°符号）
    weather_temp_ = lv_label_create(weather_card);
    lv_obj_set_style_text_font(weather_temp_, &time30, 0);  // 使用30号时间字体（较大）
    lv_obj_set_style_text_color(weather_temp_, lv_color_make(30, 30, 30), 0);  // 深色文字，突出显示
    lv_label_set_text(weather_temp_, "22°");  // 初始占位文本
    
    // 温度范围（格式：最低温/最高温）
    lv_obj_t* temp_range = lv_label_create(weather_card);
    lv_obj_set_style_text_font(temp_range, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(temp_range, lv_color_make(120, 120, 120), 0);  // 浅灰色文字（次要信息）
    lv_label_set_text(temp_range, "15°/25°");  // 初始占位文本
    temp_range_label_ = temp_range;  // 保存成员变量引用
    
    // ========== 4. 详细天气参数区域：体感/湿度/风力/风向 ==========
    // 创建底部详情容器（浅灰色背景，圆角）
    lv_obj_t* detail_container = lv_obj_create(clock_screen_);
    lv_obj_set_size(detail_container, 220, 70);  // 宽220px，高70px
    lv_obj_set_style_bg_color(detail_container, lv_color_make(250, 250, 250), 0);  // 极浅灰色背景
    lv_obj_set_style_radius(detail_container, 10, 0);  // 圆角半径10px
    lv_obj_set_style_border_width(detail_container, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(detail_container, 10, 0);  // 内边距10px
    lv_obj_set_scrollbar_mode(detail_container, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_align(detail_container, LV_ALIGN_BOTTOM_MID, 0, -30);  // 底部居中，向上偏移30px
    
    // 设置flex布局：水平均匀分布四个指标
    lv_obj_set_flex_flow(detail_container, LV_FLEX_FLOW_ROW);  // 水平flex布局
    lv_obj_set_flex_align(detail_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 均匀分布，垂直居中
    
    // ------ 4.1 体感温度模块 ------
    lv_obj_t* feels_container = lv_obj_create(detail_container);
    lv_obj_set_size(feels_container, 50, 50);  // 宽50px，高50px
    lv_obj_set_style_bg_opa(feels_container, LV_OPA_0, 0);  // 透明背景
    lv_obj_set_style_border_width(feels_container, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(feels_container, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(feels_container, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(feels_container, LV_FLEX_FLOW_COLUMN);  // 垂直flex布局
    lv_obj_set_flex_align(feels_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 垂直水平居中
    lv_obj_set_style_pad_row(feels_container, -1, 0);  // 行间距-1px（紧凑排列）
    
    // 体感温度标题
    lv_obj_t* feels_title = lv_label_create(feels_container);
    lv_obj_set_style_text_font(feels_title, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(feels_title, lv_color_make(100, 100, 100), 0);  // 中灰色文字
    lv_label_set_text(feels_title, "体感");  // 固定标题文本
    
    // 体感温度数值
    weather_feels_label_ = lv_label_create(feels_container);
    lv_obj_set_style_text_font(weather_feels_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_feels_label_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_feels_label_, "26°");  // 初始占位文本
    
    // ------ 4.2 湿度模块 ------
    lv_obj_t* humidity_container = lv_obj_create(detail_container);
    lv_obj_set_size(humidity_container, 50, 50);  // 宽50px，高50px
    lv_obj_set_style_bg_opa(humidity_container, LV_OPA_0, 0);  // 透明背景
    lv_obj_set_style_border_width(humidity_container, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(humidity_container, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(humidity_container, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(humidity_container, LV_FLEX_FLOW_COLUMN);  // 垂直flex布局
    lv_obj_set_flex_align(humidity_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 垂直水平居中
    lv_obj_set_style_pad_row(humidity_container, -1, 0);  // 行间距-1px（紧凑排列）
    
    // 湿度标题
    lv_obj_t* humidity_title = lv_label_create(humidity_container);
    lv_obj_set_style_text_font(humidity_title, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(humidity_title, lv_color_make(100, 100, 100), 0);  // 中灰色文字
    lv_label_set_text(humidity_title, "湿度");  // 固定标题文本
    
    // 湿度数值（百分比）
    weather_humidity_label_ = lv_label_create(humidity_container);
    lv_obj_set_style_text_font(weather_humidity_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_humidity_label_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_humidity_label_, "72%");  // 初始占位文本
    
    // ------ 4.3 风力模块 ------
    lv_obj_t* wind_container = lv_obj_create(detail_container);
    lv_obj_set_size(wind_container, 50, 50);  // 宽50px，高50px
    lv_obj_set_style_bg_opa(wind_container, LV_OPA_0, 0);  // 透明背景
    lv_obj_set_style_border_width(wind_container, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(wind_container, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(wind_container, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(wind_container, LV_FLEX_FLOW_COLUMN);  // 垂直flex布局
    lv_obj_set_flex_align(wind_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 垂直水平居中
    lv_obj_set_style_pad_row(wind_container, -1, 0);  // 行间距-1px（紧凑排列）
    
    // 风力标题
    lv_obj_t* wind_title = lv_label_create(wind_container);
    lv_obj_set_style_text_font(wind_title, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(wind_title, lv_color_make(100, 100, 100), 0);  // 中灰色文字
    lv_label_set_text(wind_title, "风力");  // 固定标题文本
    
    // 风力等级（如"3级"）
    weather_wind_label_ = lv_label_create(wind_container);
    lv_obj_set_style_text_font(weather_wind_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_wind_label_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_wind_label_, "3级");  // 初始占位文本
    
    // ------ 4.4 风向模块 ------
    lv_obj_t* vis_container = lv_obj_create(detail_container);
    lv_obj_set_size(vis_container, 50, 50);  // 宽50px，高50px
    lv_obj_set_style_bg_opa(vis_container, LV_OPA_0, 0);  // 透明背景
    lv_obj_set_style_border_width(vis_container, 0, 0);  // 无边框
    lv_obj_set_style_pad_all(vis_container, 0, 0);  // 无内边距
    lv_obj_set_scrollbar_mode(vis_container, LV_SCROLLBAR_MODE_OFF);  // 禁用滚动条
    lv_obj_set_flex_flow(vis_container, LV_FLEX_FLOW_COLUMN);  // 垂直flex布局
    lv_obj_set_flex_align(vis_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 垂直水平居中
    lv_obj_set_style_pad_row(vis_container, -1, 0);  // 行间距-1px（紧凑排列）
    
    // 风向标题
    lv_obj_t* vis_title = lv_label_create(vis_container);
    lv_obj_set_style_text_font(vis_title, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(vis_title, lv_color_make(100, 100, 100), 0);  // 中灰色文字
    lv_label_set_text(vis_title, "风向");  // 固定标题文本
    
    // 风向文字（如"东风"、"西北风"等）
    weather_vis_label_ = lv_label_create(vis_container);
    lv_obj_set_style_text_font(weather_vis_label_, fonts_.text_font, 0);  // 使用标准文本字体
    lv_obj_set_style_text_color(weather_vis_label_, lv_color_make(60, 60, 60), 0);  // 深灰色文字
    lv_label_set_text(weather_vis_label_, "东风");  // 初始占位文本
    
    ESP_LOGI(TAG, "Weather clock UI setup completed");  // 记录UI初始化完成日志
    
    // ========== 启动农历数据获取后台任务 ==========
    // 使用静态变量确保农历任务只创建一次（避免重复创建）
    static bool lunar_task_created = false;
    if (!lunar_task_created) {
        // 创建FreeRTOS任务：LunarFetchTask
        // 参数：任务函数、任务名称、栈大小4KB、任务参数、优先级3、任务句柄
        BaseType_t ret = xTaskCreate(LunarFetchTask, "LunarFetchTask", 4096, nullptr, 3, nullptr);
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "Lunar fetch task created successfully (stack: 4KB)");
            lunar_task_created = true;  // 标记任务已创建
        } else {
            ESP_LOGE(TAG, "Failed to create lunar fetch task!");
        }
    }
}

void LcdDisplay::OnClockUpdate(lv_timer_t* timer) {
    LcdDisplay* display = static_cast<LcdDisplay*>(lv_timer_get_user_data(timer));
    if (display) {
        display->UpdateClockDisplay();
    }
}

void LcdDisplay::OnWeatherUpdate(void* param) {
    LcdDisplay* display = static_cast<LcdDisplay*>(param);
    if (display) {
        display->UpdateWeatherData();
        display->UpdateWeatherDisplay();
    }
    vTaskDelete(NULL);
}

void LcdDisplay::SetClockMode(bool enabled) {
    if (enabled == clock_mode_enabled_) {
        return;
    }
    
    clock_mode_enabled_ = enabled;
    
    if (enabled) {
        {
            DisplayLockGuard lock(this);
            
            // 创建时钟UI（如果还未创建）
            SetupWeatherClock();
            
            // 切换到时钟屏幕
            if (clock_screen_ != nullptr) {
                lv_screen_load(clock_screen_);
            }
            
            // 创建时钟更新定时器（1秒更新一次）
            if (clock_lvgl_timer_ == nullptr) {
                clock_lvgl_timer_ = lv_timer_create(OnClockUpdate, 1000, this);
            }
        }
        
        // 立即更新显示（会在UpdateClockDisplay中触发天气更新）
        UpdateClockDisplay();
        
        ESP_LOGI(TAG, "Clock mode enabled");
    } else {
        DisplayLockGuard lock(this);
        
        // 删除时钟更新定时器
        if (clock_lvgl_timer_ != nullptr) {
            lv_timer_del(clock_lvgl_timer_);
            clock_lvgl_timer_ = nullptr;
        }
        
        // 切换回主屏幕
        if (main_screen_ != nullptr) {
            lv_screen_load(main_screen_);
        }
        
        ESP_LOGI(TAG, "Clock mode disabled");
    }
}

void LcdDisplay::UpdateClockDisplay() {
    if (!clock_mode_enabled_ || clock_screen_ == nullptr) {
        return;
    }
    
    DisplayLockGuard lock(this);
    
    // 1. 更新时间显示
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", timeinfo);
    if (clock_time_label_) {
        lv_label_set_text(clock_time_label_, time_str);
    }
    
    // 2. 更新日期和星期
    if (clock_date_label_ != nullptr) {
        char date_str[64];
        const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
        strftime(date_str, sizeof(date_str), "%m月%d日", timeinfo);
        std::string full_date = std::string(date_str) + " 周" + weekdays[timeinfo->tm_wday];
        lv_label_set_text(clock_date_label_, full_date.c_str());
    }
    
    // 3. 获取并显示农历信息和干支年
    if (lunar_date_label_ != nullptr) {
        std::lock_guard<std::mutex> lock_lunar(g_lunar_mutex);
        std::string lunar_display;
        if (!g_lunar_cache.ganzhi_year.empty()) {
            lunar_display = g_lunar_cache.ganzhi_year + "年 " + g_lunar_cache.lunar_str;
        } else if (!g_lunar_cache.lunar_str.empty()) {
            lunar_display = g_lunar_cache.lunar_str;
        } else {
            lunar_display = "加载中...";
        }
        lv_label_set_text(lunar_date_label_, lunar_display.c_str());
    }
    
    // 4. 显示宜/忌
    if (yi_label_ && ji_label_) {
        std::string yi_str = "宜：- -";
        std::string ji_str = "忌：- -";
        {
            std::lock_guard<std::mutex> lock_lunar(g_lunar_mutex);
            if (!g_lunar_yi.empty()) {
                yi_str = "宜：" + g_lunar_yi[0];
            }
            if (!g_lunar_ji.empty()) {
                ji_str = "忌：" + g_lunar_ji[0];
            }
        }
        lv_label_set_text(yi_label_, yi_str.c_str());
        lv_label_set_text(ji_label_, ji_str.c_str());
    }
    
    // 5. 每小时触发天气更新（通过独立任务）
    static time_t last_weather_update = 0;
    static bool weather_task_created = false;
    
    // 第一次调用（立即触发）或每小时更新一次
    if (!weather_task_created || (now - last_weather_update) >= (60 * 60)) {
        BaseType_t ret = xTaskCreate(OnWeatherUpdate, "WeatherUpdate", 6144, this, 2, NULL);
        if (ret == pdPASS) {
            if (!weather_task_created) {
                ESP_LOGI(TAG, "Initial weather update task created (stack: 6KB)");
                weather_task_created = true;
            } else {
                ESP_LOGI(TAG, "Hourly weather update task created (stack: 6KB)");
            }
            last_weather_update = now;
        } else {
            if (!weather_task_created) {
                ESP_LOGE(TAG, "Failed to create initial weather update task");
            } else {
                ESP_LOGE(TAG, "Failed to create hourly weather update task");
            }
        }
    }
}

void LcdDisplay::UpdateWeatherData() {
    ESP_LOGI(TAG, "Updating weather data");
    
    // 第一步：通过IP定位获取城市编码
    auto http = Board::GetInstance().CreateHttp();
    std::string adcode = "";
    
    if (http && http->Open("GET", "https://restapi.amap.com/v3/ip?key=eac978c2fa1693791f287a32528e6d7e")) {
        std::string response = http->GetBody();
        http->Close();
        
        ESP_LOGI(TAG, "IP location response: %s", response.c_str());
        
        cJSON *root = cJSON_Parse(response.c_str());
        if (root) {
            cJSON *status = cJSON_GetObjectItem(root, "status");
            if (status && status->valuestring && strcmp(status->valuestring, "1") != 0) {
                ESP_LOGE(TAG, "IP location API error");
                cJSON *info = cJSON_GetObjectItem(root, "info");
                if (info && info->valuestring) {
                    ESP_LOGE(TAG, "Error info: %s", info->valuestring);
                }
            } else {
                cJSON *adcode_json = cJSON_GetObjectItem(root, "adcode");
                if (adcode_json && adcode_json->valuestring) {
                    adcode = adcode_json->valuestring;
                    ESP_LOGI(TAG, "Got adcode: %s", adcode.c_str());
                }
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Failed to parse IP location JSON");
        }
    } else {
        ESP_LOGE(TAG, "Failed to open IP location API");
    }
    
    if (http) {
        delete http;
        http = nullptr;
    }
    
    // 如果IP定位失败，使用默认值
    if (adcode.empty()) {
        adcode = "110105";
        ESP_LOGW(TAG, "IP location failed, using default adcode: %s", adcode.c_str());
    }
    
    // 第二步：使用城市编码获取天气预报
    http = Board::GetInstance().CreateHttp();
    std::string weather_url = "https://restapi.amap.com/v3/weather/weatherInfo?key=eac978c2fa1693791f287a32528e6d7e&city=" 
                             + adcode + "&extensions=all";
    
    ESP_LOGI(TAG, "Fetching weather forecast from: %s", weather_url.c_str());
    
    if (http && http->Open("GET", weather_url)) {
        std::string response = http->GetBody();
        http->Close();
        
        ESP_LOGI(TAG, "Weather forecast response: %s", response.c_str());
        
        cJSON *root = cJSON_Parse(response.c_str());
        if (root) {
            // 检查状态
            cJSON *status = cJSON_GetObjectItem(root, "status");
            if (status && status->valuestring && strcmp(status->valuestring, "1") != 0) {
                ESP_LOGE(TAG, "Weather API error");
                cJSON *info = cJSON_GetObjectItem(root, "info");
                if (info && info->valuestring) {
                    ESP_LOGE(TAG, "Error info: %s", info->valuestring);
                }
                cJSON_Delete(root);
                if (http) delete http;
                return;
            }
            cJSON *forecasts = cJSON_GetObjectItem(root, "forecasts");
            if (forecasts && cJSON_IsArray(forecasts) && cJSON_GetArraySize(forecasts) > 0) {
                cJSON *forecast = cJSON_GetArrayItem(forecasts, 0);
                
                // 获取城市名称
                cJSON *city = cJSON_GetObjectItem(forecast, "city");
                if (city && city->valuestring) {
                    weather_data_.city = city->valuestring;
                    ESP_LOGI(TAG, "City: %s", weather_data_.city.c_str());
                }
                
                // 获取今日天气
                cJSON *casts = cJSON_GetObjectItem(forecast, "casts");
                if (casts && cJSON_IsArray(casts) && cJSON_GetArraySize(casts) > 0) {
                    cJSON *today = cJSON_GetArrayItem(casts, 0);
                    
                    cJSON *dayweather = cJSON_GetObjectItem(today, "dayweather");
                    cJSON *daytemp = cJSON_GetObjectItem(today, "daytemp");
                    cJSON *nighttemp = cJSON_GetObjectItem(today, "nighttemp");
                    cJSON *daywind = cJSON_GetObjectItem(today, "daywind");
                    cJSON *daypower = cJSON_GetObjectItem(today, "daypower");
                    
                    if (dayweather && dayweather->valuestring) {
                        weather_data_.text = dayweather->valuestring;
                    }
                    if (daytemp && daytemp->valuestring) {
                        weather_data_.highTemp = daytemp->valuestring;
                    }
                    if (nighttemp && nighttemp->valuestring) {
                        weather_data_.lowTemp = nighttemp->valuestring;
                    }
                    if (daywind && daywind->valuestring) {
                        weather_data_.windDir = daywind->valuestring;
                    }
                    if (daypower && daypower->valuestring) {
                        weather_data_.windScale = daypower->valuestring;
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    
    if (http) {
        delete http;
        http = nullptr;
    }
    
    // 第三步：获取实况天气数据
    http = Board::GetInstance().CreateHttp();
    std::string live_weather_url = "https://restapi.amap.com/v3/weather/weatherInfo?key=eac978c2fa1693791f287a32528e6d7e&city=" 
                                   + adcode + "&extensions=base";
    
    ESP_LOGI(TAG, "Fetching live weather from: %s", live_weather_url.c_str());
    
    if (http && http->Open("GET", live_weather_url)) {
        std::string response = http->GetBody();
        http->Close();
        
        ESP_LOGI(TAG, "Live weather response: %s", response.c_str());
        
        cJSON *root = cJSON_Parse(response.c_str());
        if (root) {
            // 检查状态
            cJSON *status = cJSON_GetObjectItem(root, "status");
            if (status && status->valuestring && strcmp(status->valuestring, "1") != 0) {
                ESP_LOGE(TAG, "Live weather API error");
                cJSON *info = cJSON_GetObjectItem(root, "info");
                if (info && info->valuestring) {
                    ESP_LOGE(TAG, "Error info: %s", info->valuestring);
                }
                cJSON_Delete(root);
                if (http) delete http;
                return;
            }
            cJSON *lives = cJSON_GetObjectItem(root, "lives");
            if (lives && cJSON_IsArray(lives) && cJSON_GetArraySize(lives) > 0) {
                cJSON *live = cJSON_GetArrayItem(lives, 0);
                
                cJSON *temperature = cJSON_GetObjectItem(live, "temperature");
                cJSON *humidity = cJSON_GetObjectItem(live, "humidity");
                cJSON *weather = cJSON_GetObjectItem(live, "weather");
                
                if (temperature && temperature->valuestring) {
                    weather_data_.temp = temperature->valuestring;
                    weather_data_.feelsLike = temperature->valuestring;
                }
                if (humidity && humidity->valuestring) {
                    weather_data_.humidity = humidity->valuestring;
                }
                if (weather && weather->valuestring) {
                    std::string weather_str = weather->valuestring;
                    
                    // 根据天气描述映射图标代码
                    if (weather_str.find("晴") != std::string::npos) {
                        weather_data_.icon = "100";
                    } else if (weather_str.find("多云") != std::string::npos) {
                        weather_data_.icon = "101";
                    } else if (weather_str.find("阴") != std::string::npos) {
                        weather_data_.icon = "104";
                    } else if (weather_str.find("雨") != std::string::npos) {
                        weather_data_.icon = "305";
                    } else if (weather_str.find("雪") != std::string::npos) {
                        weather_data_.icon = "400";
                    } else if (weather_str.find("雾") != std::string::npos || weather_str.find("霾") != std::string::npos) {
                        weather_data_.icon = "501";
                    } else {
                        weather_data_.icon = "999";
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    
    if (http) {
        delete http;
    }
    
    weather_data_.dataValid = true;
    ESP_LOGI(TAG, "Weather data updated: %s, %s°, %s/%s°", 
             weather_data_.city.c_str(), weather_data_.temp.c_str(),
             weather_data_.lowTemp.c_str(), weather_data_.highTemp.c_str());
}

void LcdDisplay::UpdateWeatherDisplay() {
    if (!weather_data_.dataValid) {
        ESP_LOGW(TAG, "Weather data not valid, skipping display update");
        return;
    }
    
    if (!clock_mode_enabled_ || clock_screen_ == nullptr) {
        ESP_LOGW(TAG, "Clock mode not enabled or screen not ready, skipping weather display update");
        return;
    }
    
    DisplayLockGuard lock(this);
    
    ESP_LOGI(TAG, "Updating weather UI elements...");
    
    // 更新城市名称
    if (weather_city_) {
        lv_label_set_text(weather_city_, weather_data_.city.c_str());
        ESP_LOGI(TAG, "City updated: %s", weather_data_.city.c_str());
    } else {
        ESP_LOGW(TAG, "weather_city_ is null!");
    }
    
    // 更新天气描述
    if (weather_text_) {
        lv_label_set_text(weather_text_, weather_data_.text.c_str());
        ESP_LOGI(TAG, "Weather text updated: %s", weather_data_.text.c_str());
    } else {
        ESP_LOGW(TAG, "weather_text_ is null!");
    }
    
    // 更新当前温度
    if (weather_temp_) {
        std::string current_temp = weather_data_.temp + "°";
        lv_label_set_text(weather_temp_, current_temp.c_str());
        ESP_LOGI(TAG, "Temperature updated: %s", current_temp.c_str());
    } else {
        ESP_LOGW(TAG, "weather_temp_ is null!");
    }
    
    // 更新温度范围
    if (temp_range_label_) {
        std::string temp_range = weather_data_.lowTemp + "°/" + weather_data_.highTemp + "°";
        lv_label_set_text(temp_range_label_, temp_range.c_str());
    }
    
    // 更新天气图标
    if (weather_icon_) {
        const lv_image_dsc_t* icon = getWeatherIcon(weather_data_.icon);
        lv_image_set_src(weather_icon_, icon);
        ESP_LOGI(TAG, "Weather icon updated: %s", weather_data_.icon.c_str());
    } else {
        ESP_LOGW(TAG, "weather_icon_ is null!");
    }
    
    // 更新详细参数
    if (weather_feels_label_) {
        lv_label_set_text(weather_feels_label_, (weather_data_.feelsLike + "°").c_str());
    }
    if (weather_humidity_label_) {
        lv_label_set_text(weather_humidity_label_, (weather_data_.humidity + "%").c_str());
    }
    if (weather_wind_label_) {
        lv_label_set_text(weather_wind_label_, (weather_data_.windScale + "级").c_str());
    }
    if (weather_vis_label_) {
        lv_label_set_text(weather_vis_label_, weather_data_.windDir.c_str());
    }
    
    ESP_LOGI(TAG, "Weather display updated");
}
