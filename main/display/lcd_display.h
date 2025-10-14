#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

class LcdDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    DisplayFonts fonts_;

    virtual void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
    
    // 天气时钟相关方法
    virtual void SetClockMode(bool enabled) override;
    virtual void UpdateClockDisplay() override;
    virtual void UpdateWeatherData() override;
    virtual void UpdateWeatherDisplay() override;
    
protected:
    // 天气时钟UI设置和回调
    void SetupWeatherClock();
    static void OnClockUpdate(lv_timer_t* timer);
    static void OnWeatherUpdate(void* param);
    
    // 天气时钟相关成员变量
    bool clock_mode_enabled_ = false;
    lv_obj_t* clock_screen_ = nullptr;
    lv_obj_t* main_screen_ = nullptr;
    
    // 时钟UI元素
    lv_obj_t* clock_time_label_ = nullptr;
    lv_obj_t* clock_date_label_ = nullptr;
    lv_obj_t* lunar_date_label_ = nullptr;
    lv_obj_t* yi_label_ = nullptr;
    lv_obj_t* ji_label_ = nullptr;
    
    // 天气UI元素
    lv_obj_t* weather_city_ = nullptr;
    lv_obj_t* weather_icon_ = nullptr;
    lv_obj_t* weather_text_ = nullptr;
    lv_obj_t* weather_temp_ = nullptr;
    lv_obj_t* temp_range_label_ = nullptr;
    lv_obj_t* weather_feels_label_ = nullptr;
    lv_obj_t* weather_humidity_label_ = nullptr;
    lv_obj_t* weather_wind_label_ = nullptr;
    lv_obj_t* weather_vis_label_ = nullptr;
    
    // 定时器
    lv_timer_t* clock_lvgl_timer_ = nullptr;
    
    // 天气数据结构
    struct WeatherData {
        bool dataValid = false;
        std::string city;
        std::string text;
        std::string temp;
        std::string feelsLike;
        std::string windDir;
        std::string windScale;
        std::string humidity;
        std::string vis;
        std::string icon;
        std::string lowTemp;
        std::string highTemp;
        std::string updateTime;
    } weather_data_;
};

// RGB LCD显示器
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiLcdDisplay : public LcdDisplay {
public:
    QspiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080LcdDisplay : public LcdDisplay {
public:
    Mcu8080LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
