#include "core/lv_obj_pos.h"  
#include "font/lv_font.h"     
#include "misc/lv_types.h"    
#include "wifi_board.h"       
#include "audio_codecs/es8311_audio_codec.h"  
#include "display/lcd_display.h"  
#include "system_reset.h"      
#include "application.h"       
#include "button.h"            
#include "config.h"            
#include "iot/thing_manager.h"  
#include "lunar_calendar.h"     
#include "sdkconfig.h"         
#include "power_save_timer.h"  
#include <esp_log.h>           
#include "i2c_device.h"        
#include <driver/i2c_master.h>  
#include <driver/ledc.h>        
#include <wifi_station.h>       
#include <esp_lcd_panel_io.h>   
#include <esp_lcd_panel_ops.h>  
#include <esp_timer.h>          
#include "lcd_display.h"        
#include <iot_button.h>         
#include <cstring>              
#include <sys/stat.h>           
#include "esp_lcd_gc9a01.h"     
#include <font_awesome_symbols.h>  
#include "assets/lang_config.h"    
#include <esp_http_client.h>      
#include <cJSON.h>                
#include "power_manager.h"        
#include "power_save_timer.h"     
#include <esp_sleep.h>            
#include <esp_pm.h>               
#include <esp_wifi.h>             
#include <esp_littlefs.h>         
#include "button.h"               
#include "settings.h"             
#include "iot_image_display.h"  
#include "image_manager.h"  
#include "image_resource/network/downloader.h"  
#include "config/resource_config.h"  
#include "ui/music_player_ui.h"  
#include "ui/mqtt_music_handler.h"  
#include "iot/things/music_player.h"  
#include <esp_random.h>  
#define TAG "abrobot-1.28tft-wifi"  
extern "C" {
    extern volatile iot::ImageDisplayMode g_image_display_mode;
    extern const unsigned char* g_static_image;  
}
class CustomBoard;
void ResetPowerSaveTimer();
LV_FONT_DECLARE(lunar);      
LV_FONT_DECLARE(time70);     
LV_FONT_DECLARE(time50);     
LV_FONT_DECLARE(time40);     
LV_FONT_DECLARE(font_puhui_20_4);    
LV_FONT_DECLARE(font_awesome_20_4);  
LV_FONT_DECLARE(font_awesome_30_4);  
#define WALLPAPER_URL_CACHE_FILE "/model/wallpaper_urls.json"
#define WALLPAPER_BASE_PATH "/model/wallpapers/"
#define WALLPAPER_API_URL "http://110.42.35.132:8333/urls"
#define DARK_BACKGROUND_COLOR       lv_color_hex(0)           
#define DARK_TEXT_COLOR             lv_color_black()          
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0)           
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)    
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)    
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)    
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)    
#define DARK_BORDER_COLOR           lv_color_hex(0)           
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)    
#define LIGHT_BACKGROUND_COLOR       lv_color_hex(0)          
#define LIGHT_TEXT_COLOR             lv_color_white()          
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)    
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)    
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()          
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)    
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)    
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)    
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()          
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
static ThemeColors current_theme = DARK_THEME;
static struct {
    bool pending;
    int progress;
    char message[64];
    SemaphoreHandle_t mutex;
} g_download_progress = {false, 0, "", NULL};
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    lv_timer_t *idle_timer_ = nullptr;  
    lv_timer_t* sleep_timer_ = nullptr;  
    bool is_sleeping_ = false;           
    int normal_brightness_ = 70;         
    bool is_light_sleeping_ = false;     
    MusicPlayerUI* music_player_ui_ = nullptr;  
    bool music_player_active_ = false;          
    lv_obj_t * tab1 = nullptr;          
    lv_obj_t * tab2 = nullptr;          
    lv_obj_t * tab3 = nullptr;          
    lv_obj_t * tabview_ = nullptr;      
    lv_obj_t * tab3_time_label_ = nullptr;      
    lv_obj_t * tab3_date_label_ = nullptr;      
    lv_obj_t * tab3_weekday_label_ = nullptr;   
    lv_obj_t * tab3_mode_label_ = nullptr;      
    lv_obj_t * bg_img = nullptr;        
    lv_obj_t * bg_img2 = nullptr;       
    uint8_t bg_index = 1;               
    lv_obj_t * bg_switch_btn = nullptr; 
    lv_obj_t * subtitle_container_ = nullptr;  
    lv_obj_t* tab2_bg_img_ = nullptr;           
    uint8_t* bg_images_data_[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};  
    lv_image_dsc_t bg_images_desc_[6];          
    std::vector<std::string> wallpaper_urls_;   
    int current_bg_index_ = -1;                 
    lv_timer_t* subtitle_scroll_timer_ = nullptr;  
    lv_coord_t subtitle_scroll_pos_ = 0;           
    lv_coord_t subtitle_max_scroll_ = 0;           
    bool subtitle_scrolling_ = false;              
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy) 
        : SpiLcdDisplay(io_handle, panel_handle,
                    width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    {
                        .text_font = &font_puhui_20_4,       
                        .icon_font = &font_awesome_20_4,     
                    }) {
        MountModelPartition();
        DisplayLockGuard lock(this);  
        SetupUI();                    
        music_player_config_t config = getDefaultMusicPlayerConfig();
        music_player_ui_ = music_player_ui_create(&config);
        if (!music_player_ui_) {
            ESP_LOGE(TAG, "创建音乐播放器UI失败");
        } else {
            lv_obj_t* screen = lv_screen_active();
            music_player_error_t ret = music_player_ui_->Initialize(screen, &config);
            if (ret != MUSIC_PLAYER_OK) {
                ESP_LOGE(TAG, "初始化音乐播放器UI失败: %d", ret);
                music_player_ui_destroy(music_player_ui_);
                music_player_ui_ = nullptr;
            } else {
                extern MusicPlayerUI* g_music_player_instance;
                g_music_player_instance = music_player_ui_;
                ESP_LOGI(TAG, "音乐播放器UI初始化成功");
            }
        }
        if (g_download_progress.mutex == NULL) {
            g_download_progress.mutex = xSemaphoreCreateMutex();
        }
        lv_timer_create([](lv_timer_t* timer) {
            CustomLcdDisplay* display = (CustomLcdDisplay*)lv_timer_get_user_data(timer);
            if (!display) return;
            if (g_download_progress.mutex && xSemaphoreTake(g_download_progress.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (g_download_progress.pending) {
                    int progress = g_download_progress.progress;
                    char message[64];
                    strncpy(message, g_download_progress.message, sizeof(message));
                    g_download_progress.pending = false;
                    xSemaphoreGive(g_download_progress.mutex);
                    display->UpdateDownloadProgressUI(true, progress, message);
                } else {
                    xSemaphoreGive(g_download_progress.mutex);
                }
            }
        }, 100, this); 
        LoadBackgroundImages();
    }
    static void SubtitleScrollTimerCallback(lv_timer_t* timer) {
        auto* display = static_cast<CustomLcdDisplay*>(timer->user_data);
        if (!display || !display->subtitle_container_ || !display->chat_message_label_) {
            return;
        }
        lv_coord_t current_scroll = lv_obj_get_scroll_y(display->subtitle_container_);
        lv_coord_t next_scroll = current_scroll + 1;
        if (next_scroll >= display->subtitle_max_scroll_) {
            if (display->subtitle_scroll_pos_ == 0) {
                display->subtitle_scroll_pos_ = 1;  
                lv_timer_set_period(timer, 2000);   
                return;
            } else {
                lv_obj_scroll_to_y(display->subtitle_container_, 0, LV_ANIM_OFF);
                display->subtitle_scroll_pos_ = 0;
                lv_timer_set_period(timer, 30);  
                return;
            }
        }
        lv_obj_scroll_to_y(display->subtitle_container_, next_scroll, LV_ANIM_OFF);
    }
    ~CustomLcdDisplay() {
        if (music_player_ui_) {
            extern MusicPlayerUI* g_music_player_instance;
            g_music_player_instance = nullptr;  
            music_player_ui_destroy(music_player_ui_);
            music_player_ui_ = nullptr;
        }
        if (subtitle_scroll_timer_) {
            lv_timer_del(subtitle_scroll_timer_);
            subtitle_scroll_timer_ = nullptr;
        }
        if (subtitle_container_) {
            lv_obj_del(subtitle_container_);
            subtitle_container_ = nullptr;
        }
        if (idle_timer_) {
            lv_timer_del(idle_timer_);
            idle_timer_ = nullptr;
        }
        if (sleep_timer_) {
            lv_timer_del(sleep_timer_);
            sleep_timer_ = nullptr;
        }
        FreeWallpaperData();
    }
    void LoadBackgroundImages() {
        ESP_LOGI(TAG, "=== 初始化Tab2背景图像 ===");
        for (int i = 0; i < 6; i++) {
            char filepath[128];
            snprintf(filepath, sizeof(filepath), "%sclock_%d.bin", WALLPAPER_BASE_PATH, i + 1);
            if (LoadWallpaperFromFile(i, filepath)) {
                ESP_LOGI(TAG, "✓ 壁纸 %d 加载成功: %dx%d, 大小=%lu字节", 
                    i + 1,
                    (int)bg_images_desc_[i].header.w, 
                    (int)bg_images_desc_[i].header.h, 
                    (unsigned long)bg_images_desc_[i].data_size);
            } else {
                ESP_LOGW(TAG, "✗ 壁纸 %d 加载失败，文件: %s", i + 1, filepath);
            }
        }
        ESP_LOGI(TAG, "=== Tab2背景图像初始化完成 ===");
    }
    void FreeWallpaperData() {
        for (int i = 0; i < 6; i++) {
            if (bg_images_data_[i]) {
                free(bg_images_data_[i]);
                bg_images_data_[i] = nullptr;
            }
            memset(&bg_images_desc_[i], 0, sizeof(lv_image_dsc_t));
        }
    }
    bool LoadWallpaperFromFile(int index, const char* filepath) {
        if (index < 0 || index >= 6) {
            return false;
        }
        FILE* f = fopen(filepath, "rb");
        if (!f) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        size_t file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t* data = (uint8_t*)malloc(file_size);
        if (!data) {
            fclose(f);
            ESP_LOGE(TAG, "无法分配内存用于壁纸 %d，大小=%zu", index + 1, file_size);
            return false;
        }
        size_t read_size = fread(data, 1, file_size, f);
        fclose(f);
        if (read_size != file_size) {
            free(data);
            ESP_LOGE(TAG, "读取壁纸文件失败 %d", index + 1);
            return false;
        }
        if (bg_images_data_[index]) {
            free(bg_images_data_[index]);
        }
        bg_images_data_[index] = data;
        bg_images_desc_[index].header.magic = LV_IMAGE_HEADER_MAGIC;  
        bg_images_desc_[index].header.cf = LV_COLOR_FORMAT_RGB565;
        bg_images_desc_[index].header.flags = 0;
        bg_images_desc_[index].header.w = 240;
        bg_images_desc_[index].header.h = 240;
        bg_images_desc_[index].header.stride = 240 * 2;  
        bg_images_desc_[index].header.reserved_2 = 0;
        bg_images_desc_[index].data_size = file_size;
        bg_images_desc_[index].data = data;
        bg_images_desc_[index].reserved = NULL;
        return true;
    }
    bool MountModelPartition() {
        static bool model_mounted = false;
        if (model_mounted) {
            return true;  
        }
        esp_vfs_littlefs_conf_t conf = {
            .base_path = "/model",
            .partition_label = "model",
            .format_if_mount_failed = true,  
            .dont_mount = false,
        };
        esp_err_t ret = esp_vfs_littlefs_register(&conf);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "model 分区已挂载到 /model");
            model_mounted = true;
            return true;
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGI(TAG, "model 分区已经挂载");
            model_mounted = true;
            return true;
        } else {
            ESP_LOGE(TAG, "挂载 model 分区失败: %s", esp_err_to_name(ret));
            return false;
        }
    }
    bool CheckAndDownloadWallpapers() {
        ESP_LOGI(TAG, "=== 开始检查壁纸资源 ===");
        if (!MountModelPartition()) {
            return false;
        }
        if (!WifiStation::GetInstance().IsConnected()) {
            ESP_LOGW(TAG, "WiFi未连接，跳过壁纸下载");
            return false;
        }
        std::vector<std::string> cached_urls;
        FILE* f = fopen(WALLPAPER_URL_CACHE_FILE, "r");
        if (f) {
            char buffer[1024];
            size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
            fclose(f);
            if (len > 0) {
                buffer[len] = '\0';
                cJSON* root = cJSON_Parse(buffer);
                if (root) {
                    cJSON* urls_array = cJSON_GetObjectItem(root, "urls");
                    if (urls_array && cJSON_IsArray(urls_array)) {
                        int array_size = cJSON_GetArraySize(urls_array);
                        for (int i = 0; i < array_size; i++) {
                            cJSON* url_item = cJSON_GetArrayItem(urls_array, i);
                            if (cJSON_IsString(url_item)) {
                                cached_urls.push_back(url_item->valuestring);
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
        ESP_LOGI(TAG, "本地缓存壁纸URL数量: %d", cached_urls.size());
        auto http = Board::GetInstance().CreateHttp();
        if (!http) {
            ESP_LOGE(TAG, "无法创建HTTP客户端");
            return false;
        }
        http->SetHeader("Accept", "application/json");
        if (!http->Open("GET", WALLPAPER_API_URL)) {
            ESP_LOGE(TAG, "无法连接到壁纸API: %s", WALLPAPER_API_URL);
            delete http;
            return false;
        }
        std::string response = http->GetBody();
        http->Close();
        delete http;
        if (response.empty()) {
            ESP_LOGE(TAG, "壁纸API返回空响应");
            return false;
        }
        ESP_LOGI(TAG, "壁纸API响应: %s", response.c_str());
        cJSON* root = cJSON_Parse(response.c_str());
        if (!root) {
            ESP_LOGE(TAG, "解析壁纸API响应失败");
            return false;
        }
        cJSON* urls_array = cJSON_GetObjectItem(root, "urls");
        if (!urls_array || !cJSON_IsArray(urls_array)) {
            ESP_LOGE(TAG, "壁纸API响应格式错误");
            cJSON_Delete(root);
            return false;
        }
        std::vector<std::string> new_urls;
        int array_size = cJSON_GetArraySize(urls_array);
        for (int i = 0; i < array_size && i < 6; i++) {
            cJSON* url_item = cJSON_GetArrayItem(urls_array, i);
            if (cJSON_IsString(url_item)) {
                new_urls.push_back(url_item->valuestring);
            }
        }
        cJSON_Delete(root);
        if (new_urls.size() != 6) {
            ESP_LOGE(TAG, "壁纸URL数量不正确: %d（期望6张）", new_urls.size());
            return false;
        }
        bool need_update = (cached_urls.size() != new_urls.size());
        if (!need_update) {
            for (size_t i = 0; i < new_urls.size(); i++) {
                if (cached_urls[i] != new_urls[i]) {
                    need_update = true;
                    break;
                }
            }
        }
        if (!need_update) {
            ESP_LOGI(TAG, "壁纸URL未变化，检查本地文件完整性...");
            for (int i = 0; i < 6; i++) {
                char filepath[128];
                snprintf(filepath, sizeof(filepath), "%sclock_%d.bin", WALLPAPER_BASE_PATH, i + 1);
                FILE* f = fopen(filepath, "rb");
                if (!f) {
                    ESP_LOGW(TAG, "壁纸文件 %d 不存在，需要重新下载", i + 1);
                    need_update = true;
                    break;
                }
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fclose(f);
                if (size != 115200) {
                    ESP_LOGW(TAG, "壁纸文件 %d 大小异常: %ld字节（期望115200），需要重新下载", i + 1, size);
                    need_update = true;
                    break;
                }
            }
        }
        if (!need_update) {
            ESP_LOGI(TAG, "壁纸URL未变化且本地文件完整，无需下载");
            return false;  
        }
        ESP_LOGI(TAG, "检测到壁纸需要更新（URL变化或文件不完整），开始下载...");
        mkdir(WALLPAPER_BASE_PATH, 0755);
        ShowDownloadProgress(true, 0, Lang::Strings::DELETING_WALLPAPERS);
        for (int i = 0; i < 6; i++) {
            char filepath[128];
            snprintf(filepath, sizeof(filepath), "%sclock_%d.bin", WALLPAPER_BASE_PATH, i + 1);
            remove(filepath);  
        }
        ShowDownloadProgress(true, 0, Lang::Strings::DOWNLOADING_WALLPAPERS);
        const ImageResource::ResourceConfig* resource_config = &ImageResource::ConfigManager::GetInstance().get_config();
        ImageResource::Downloader* downloader = new ImageResource::Downloader(resource_config);
        downloader->SetProgressCallback([this](int current, int total, const char* message) {
            int percent = (total > 0) ? (current * 100 / total) : 0;
            this->ShowDownloadProgress(true, percent, message);
        });
        bool download_success = true;
        for (int i = 0; i < 6; i++) {
            char filepath[128];
            snprintf(filepath, sizeof(filepath), "%sclock_%d.bin", WALLPAPER_BASE_PATH, i + 1);
            ESP_LOGI(TAG, "下载壁纸 %d: %s", i + 1, new_urls[i].c_str());
            esp_err_t err = downloader->DownloadFile(new_urls[i].c_str(), filepath, i + 1, 6);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "下载壁纸 %d 失败", i + 1);
                download_success = false;
                break;
            }
        }
        delete downloader;
        if (!download_success) {
            ESP_LOGE(TAG, "壁纸下载失败，清理已下载的不完整文件");
            ShowDownloadProgress(true, 0, "下载失败，清理文件...");
            for (int i = 0; i < 6; i++) {
                char filepath[128];
                snprintf(filepath, sizeof(filepath), "%sclock_%d.bin", WALLPAPER_BASE_PATH, i + 1);
                remove(filepath);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));  
            ShowDownloadProgress(false, 0, nullptr);
            return false;
        }
        cJSON* cache_root = cJSON_CreateObject();
        cJSON* cache_urls = cJSON_CreateArray();
        for (const auto& url : new_urls) {
            cJSON_AddItemToArray(cache_urls, cJSON_CreateString(url.c_str()));
        }
        cJSON_AddItemToObject(cache_root, "urls", cache_urls);
        char* json_str = cJSON_Print(cache_root);
        f = fopen(WALLPAPER_URL_CACHE_FILE, "w");
        if (f) {
            fprintf(f, "%s", json_str);
            fclose(f);
        }
        cJSON_Delete(cache_root);
        free(json_str);
        ESP_LOGI(TAG, "壁纸下载完成，已保存缓存");
        ShowDownloadProgress(true, 100, "壁纸下载完成！");
        vTaskDelay(pdMS_TO_TICKS(1000));  
        ShowDownloadProgress(false, 0, nullptr);
        wallpaper_urls_ = new_urls;
        return true;
    }
    void RandomizeTab2Background() {
        if (!tab2_bg_img_) {
            ESP_LOGW(TAG, "Tab2背景图像对象未初始化");
            return;
        }
        uint32_t random_choice = esp_random() % 8;
        DisplayLockGuard lock(this);
        if (random_choice >= 6) {
            lv_obj_add_flag(tab2_bg_img_, LV_OBJ_FLAG_HIDDEN);
            current_bg_index_ = -1;
            ESP_LOGI(TAG, "Tab2背景: 纯黑色");
        } else {
            current_bg_index_ = (int)random_choice;
            if (bg_images_data_[current_bg_index_] && bg_images_desc_[current_bg_index_].data) {
                ESP_LOGI(TAG, "→ 设置图片源: 壁纸%d (指针=%p, 大小=%lu)", 
                    current_bg_index_ + 1,
                    (void*)bg_images_data_[current_bg_index_],
                    (unsigned long)bg_images_desc_[current_bg_index_].data_size);
                lv_img_set_src(tab2_bg_img_, &bg_images_desc_[current_bg_index_]);
                lv_obj_clear_flag(tab2_bg_img_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_background(tab2_bg_img_);  
                lv_obj_invalidate(tab2_bg_img_);  
                ESP_LOGI(TAG, "✓ Tab2背景: 壁纸 %d 已设置，图像对象=%p", 
                    current_bg_index_ + 1, (void*)tab2_bg_img_);
                ESP_LOGI(TAG, "  图像尺寸: %ldx%ld, 隐藏标志: %d", 
                    (long)lv_obj_get_width(tab2_bg_img_), 
                    (long)lv_obj_get_height(tab2_bg_img_),
                    lv_obj_has_flag(tab2_bg_img_, LV_OBJ_FLAG_HIDDEN) ? 1 : 0);
            } else {
                lv_obj_add_flag(tab2_bg_img_, LV_OBJ_FLAG_HIDDEN);
                current_bg_index_ = -1;
                ESP_LOGW(TAG, "Tab2背景壁纸 %u 不可用，使用纯黑背景", (unsigned int)random_choice);
            }
        }
    }
    void ShowMusicPlayer(const char* album_cover_path = nullptr, 
                        const char* title = "未知歌曲", 
                        const char* artist = "未知艺术家",
                        uint32_t duration_ms = 30000) {
        if (!music_player_ui_) {
            ESP_LOGE(TAG, "音乐播放器UI未初始化");
            return;
        }
        if (album_cover_path) {
            ESP_LOGI(TAG, "专辑封面路径: %s (暂未实现文件加载)", album_cover_path);
        }
        music_player_ui_->SetSongInfo(title, artist);
        music_player_ui_->Show(duration_ms);
        music_player_active_ = true;
        ESP_LOGI(TAG, "音乐播放器UI已显示: %s - %s (持续时间: %lu ms)", title, artist, duration_ms);
    }
    void HideMusicPlayer() {
        if (music_player_ui_ && music_player_active_) {
            music_player_ui_->Hide();
            music_player_active_ = false;
            ESP_LOGI(TAG, "音乐播放器UI已隐藏");
        }
    }
    void UpdateMusicSpectrum(const float* spectrum_data, size_t spectrum_size) {
    }
    bool IsMusicPlayerActive() const {
        return music_player_active_;
    }
    void SetIdle(bool status) override 
    {
        if (status == false)
        {
            if (idle_timer_ != nullptr) {
                lv_timer_del(idle_timer_);  
                idle_timer_ = nullptr;
            }
            if (sleep_timer_ != nullptr) {
                lv_timer_del(sleep_timer_);
                sleep_timer_ = nullptr;
            }
            if (is_sleeping_) {
                static auto& board = Board::GetInstance();
                auto backlight = board.GetBacklight();
                if (backlight) {
                    backlight->SetBrightness(normal_brightness_);
                }
                is_sleeping_ = false;
                ESP_LOGI(TAG, "用户交互唤醒设备，恢复亮度到 %d", normal_brightness_);
            }
            if (tabview_ != nullptr) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1 || active_tab == 2) {  
                    ESP_LOGI(TAG, "用户交互唤醒，从时钟页面切换回主页面");
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);  
                }
            }
            return;
        } 
    if (user_interaction_disabled_) {
        ESP_LOGI(TAG, "用户交互已禁用，暂不启用空闲定时器");
        return;
    }
    if (idle_timer_ != nullptr) {
        lv_timer_del(idle_timer_);
        idle_timer_ = nullptr;
    }
    auto& app = Application::GetInstance();
    DeviceState currentState = app.GetDeviceState();
    bool download_ui_is_active_and_visible = false;
    if (download_progress_container_ != nullptr &&
        !lv_obj_has_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
        download_ui_is_active_and_visible = true;
    }
    bool preload_ui_is_active_and_visible = false;
    if (preload_progress_container_ != nullptr &&
        !lv_obj_has_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
        preload_ui_is_active_and_visible = true;
    }
    ESP_LOGI(TAG, "SetIdle(true) 状态检查: 设备状态=%d, 下载UI可见=%s, 预加载UI可见=%s", 
            currentState, download_ui_is_active_and_visible ? "是" : "否", 
            preload_ui_is_active_and_visible ? "是" : "否");
    if (currentState == kDeviceStateStarting || 
        currentState == kDeviceStateWifiConfiguring ||
        currentState == kDeviceStateActivating ||
        currentState == kDeviceStateUpgrading ||
        download_ui_is_active_and_visible ||
        preload_ui_is_active_and_visible) { 
        ESP_LOGI(TAG, "设备处于启动/配置/激活/升级状态或下载/预加载UI可见，暂不启用空闲定时器");
        return;
    }
        ESP_LOGI(TAG, "创建空闲定时器，15秒后切换到时钟页面");
        idle_timer_ = lv_timer_create([](lv_timer_t * t) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_timer_get_user_data(t);
            if (!display) return;
            auto& app = Application::GetInstance();
            DeviceState currentState = app.GetDeviceState();
            bool download_ui_active = false;
            if (display->download_progress_container_ != nullptr &&
                !lv_obj_has_flag(display->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                download_ui_active = true;
            }
            bool preload_ui_active = false;
            if (display->preload_progress_container_ != nullptr &&
                !lv_obj_has_flag(display->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                preload_ui_active = true;
            }
            if (currentState == kDeviceStateStarting || 
                currentState == kDeviceStateWifiConfiguring ||
                download_ui_active ||
                preload_ui_active ||
                display->user_interaction_disabled_) {
                ESP_LOGW(TAG, "空闲定时器触发时检测到阻塞条件，取消切换: 状态=%d, 下载UI=%s, 预加载UI=%s, 交互禁用=%s", 
                        currentState, download_ui_active ? "可见" : "隐藏", 
                        preload_ui_active ? "可见" : "隐藏", 
                        display->user_interaction_disabled_ ? "是" : "否");
                lv_timer_del(t);
                display->idle_timer_ = nullptr;
                return;
            }
            if (display->tabview_) {
                ESP_LOGI(TAG, "空闲定时器触发，切换到时钟页面");
                uint32_t current_tab = lv_tabview_get_tab_act(display->tabview_);
                ESP_LOGI(TAG, "当前活动Tab: %lu, 目标Tab: 1", (unsigned long)current_tab);
                display->RandomizeTab2Background();
                lv_lock();
                ESP_LOGI(TAG, "执行Tab切换到索引2（时钟页面）");
                lv_tabview_set_act(display->tabview_, 2, LV_ANIM_OFF);  
                uint32_t new_tab = lv_tabview_get_tab_act(display->tabview_);
                if (new_tab != 2) {
                    ESP_LOGW(TAG, "Tab切换可能失败: 期望=2, 实际=%lu, 重试切换", (unsigned long)new_tab);
                    lv_tabview_set_act(display->tabview_, 2, LV_ANIM_OFF);  
                    new_tab = lv_tabview_get_tab_act(display->tabview_);
                }
                lv_obj_invalidate(display->tabview_);
                ESP_LOGI(TAG, "已强制刷新tabview，当前Tab: %lu", (unsigned long)new_tab);
                lv_obj_move_foreground(display->tab2);
                if (display->GetCanvas() != nullptr) {
                    lv_obj_move_background(display->GetCanvas());
                }
                lv_refr_now(lv_disp_get_default());
                ESP_LOGI(TAG, "已执行立即显示刷新");
                lv_unlock();  
                uint32_t final_tab = lv_tabview_get_tab_act(display->tabview_);
                if (final_tab == 2) {
                    ESP_LOGI(TAG, "✅ Tab切换验证成功: 当前活动Tab = %lu (Tab2时钟页面)", (unsigned long)final_tab);
                } else {
                    ESP_LOGE(TAG, "❌ Tab切换验证失败: 期望=2, 实际=%lu", (unsigned long)final_tab);
                }
                ResetPowerSaveTimer();
                ESP_LOGI(TAG, "强化Tab切换完成，已重置省电定时器");
            }
            lv_timer_del(t);
            display->idle_timer_ = nullptr;
            ESP_LOGI(TAG, "等待PowerSaveTimer管理后续睡眠流程");
        }, 15000, this);  
    }
    void EnterSleepMode() {
        if (is_sleeping_) return;  
        ESP_LOGI(TAG, "进入睡眠模式 - 降低屏幕亮度到1");
        auto& board = Board::GetInstance();
        auto backlight = board.GetBacklight();
        if (backlight) {
            normal_brightness_ = backlight->brightness();
            backlight->SetBrightness(1);  
        }
        is_sleeping_ = true;
        if (sleep_timer_) {
            lv_timer_del(sleep_timer_);
            sleep_timer_ = nullptr;
        }
    }
    void ExitSleepMode() {
        if (!is_sleeping_) return;
        ESP_LOGI(TAG, "退出睡眠模式 - 恢复屏幕亮度到 %d", normal_brightness_);
        auto& board = Board::GetInstance();
        auto backlight = board.GetBacklight();
        if (backlight) {
            backlight->SetBrightness(normal_brightness_);
        }
        is_sleeping_ = false;
    }
    void StartSleepTimer() {
        ESP_LOGD(TAG, "StartSleepTimer已废弃，由PowerSaveTimer管理");
    }
    void StopSleepTimer() {
        ESP_LOGD(TAG, "StopSleepTimer已废弃，由PowerSaveTimer管理");
    }
    void SetLightSleeping(bool sleeping) {
        is_light_sleeping_ = sleeping;
        ESP_LOGD(TAG, "浅睡眠状态更新: %s", sleeping ? "进入" : "退出");
    }
    bool IsLightSleeping() const {
        return is_light_sleeping_;
    }
    void SetChatMessage(const char* role, const char* content) override{
        DisplayLockGuard lock(this);  
        if (chat_message_label_ == nullptr) {
            return;  
        }
        lv_label_set_text(chat_message_label_, content);  
        if (subtitle_container_ != nullptr) {
            if (!subtitle_enabled_) {
                lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            bool has_content = false;
            if (content != nullptr && strlen(content) > 0) {
                std::string content_str(content);
                if (content_str.find_first_not_of(" \t\n\r") != std::string::npos) {
                    has_content = true;
                }
            }
            if (has_content) {
                lv_obj_clear_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                if (subtitle_scroll_timer_ != nullptr) {
                    lv_timer_del(subtitle_scroll_timer_);
                    subtitle_scroll_timer_ = nullptr;
                    subtitle_scrolling_ = false;
                }
                lv_obj_update_layout(chat_message_label_);
                lv_obj_update_layout(subtitle_container_);
                lv_coord_t label_height = lv_obj_get_height(chat_message_label_);
                lv_coord_t container_height = lv_obj_get_content_height(subtitle_container_);
                if (label_height > container_height) {
                    subtitle_max_scroll_ = label_height - container_height;
                    subtitle_scroll_pos_ = 0;
                    subtitle_scrolling_ = true;
                    lv_obj_scroll_to_y(subtitle_container_, 0, LV_ANIM_OFF);
                    subtitle_scroll_timer_ = lv_timer_create(SubtitleScrollTimerCallback, 30, this);
                    ESP_LOGI(TAG, "启动字幕循环滚动: 标签高度=%ld, 容器高度=%ld, 最大滚动=%ld", 
                             (long)label_height, (long)container_height, (long)subtitle_max_scroll_);
                } else {
                    lv_obj_scroll_to_y(subtitle_container_, 0, LV_ANIM_OFF);
                    subtitle_scrolling_ = false;
                }
            } else {
                lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                if (subtitle_scroll_timer_ != nullptr) {
                    lv_timer_del(subtitle_scroll_timer_);
                    subtitle_scroll_timer_ = nullptr;
                    subtitle_scrolling_ = false;
                }
            }
        }
        if (std::string(content).find(Lang::Strings::CONNECT_TO_HOTSPOT) != std::string::npos) {
            lv_obj_t* wifi_hint = lv_label_create(tab2);
            lv_obj_set_size(wifi_hint, LV_HOR_RES * 0.8, LV_SIZE_CONTENT);
            lv_obj_align(wifi_hint, LV_ALIGN_CENTER, 0, -20);
            lv_obj_set_style_text_font(wifi_hint, fonts_.text_font, 0);
            lv_obj_set_style_text_color(wifi_hint, lv_color_hex(0xFF9500), 0); 
            lv_obj_set_style_text_align(wifi_hint, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_text(wifi_hint, "请连接热点进行WiFi配置\n设备尚未连接网络");
            lv_obj_set_style_bg_color(wifi_hint, lv_color_hex(0x222222), 0);
            lv_obj_set_style_bg_opa(wifi_hint, LV_OPA_70, 0);
            lv_obj_set_style_radius(wifi_hint, 10, 0);
            lv_obj_set_style_pad_all(wifi_hint, 10, 0);
        }
    }
    void SetSubtitleEnabled(bool enabled) override {
        Display::SetSubtitleEnabled(enabled);
        DisplayLockGuard lock(this);
        if (subtitle_container_ == nullptr) {
            return;
        }
        if (enabled) {
        } else {
            lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
            if (subtitle_scroll_timer_ != nullptr) {
                lv_timer_del(subtitle_scroll_timer_);
                subtitle_scroll_timer_ = nullptr;
                subtitle_scrolling_ = false;
            }
        }
        ESP_LOGI(TAG, "字幕容器状态已设置为: %s", enabled ? "启用" : "禁用");
    }
    void SetupTab1() {
        DisplayLockGuard lock(this);
        lv_obj_set_style_text_font(tab1, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab1, current_theme.text, 0);
        lv_obj_set_style_bg_color(tab1, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab1, LV_OPA_0, 0);  
        container_ = lv_obj_create(tab1);
        lv_obj_set_style_bg_color(container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(container_, LV_OPA_0, 0);  
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(container_, -7, -7);
        lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        lv_obj_move_foreground(container_);
        status_bar_ = lv_obj_create(container_);
        lv_obj_set_size(status_bar_, LV_HOR_RES - 40, fonts_.text_font->line_height);  
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  
        lv_obj_set_style_radius(status_bar_, 0, 0);
        lv_obj_set_style_bg_color(status_bar_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_0, 0);  
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        content_ = lv_obj_create(container_);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(content_, 0, 0);
        lv_obj_set_width(content_, LV_HOR_RES);
        lv_obj_set_style_pad_all(content_, 5, 0);
        lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(content_, LV_OPA_0, 0);  
        lv_obj_set_style_border_width(content_, 0, 0);   
        lv_obj_set_height(content_, LV_VER_RES - fonts_.text_font->line_height - 10); 
        lv_obj_set_scroll_dir(content_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY);
        subtitle_container_ = lv_obj_create(tab1);
        lv_obj_set_size(subtitle_container_, LV_HOR_RES * 0.85, LV_VER_RES * 0.35);  
        lv_obj_align(subtitle_container_, LV_ALIGN_BOTTOM_MID, 0, 5);  
        lv_obj_set_style_bg_color(subtitle_container_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(subtitle_container_, LV_OPA_30, 0);  
        lv_obj_set_style_border_width(subtitle_container_, 2, 0);  
        lv_obj_set_style_border_color(subtitle_container_, lv_color_white(), 0);  
        lv_obj_set_style_pad_all(subtitle_container_, 10, 0);  
        lv_obj_set_style_radius(subtitle_container_, 10, 0);  
        lv_obj_set_scroll_dir(subtitle_container_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(subtitle_container_, LV_SCROLLBAR_MODE_OFF);  
        lv_obj_move_foreground(subtitle_container_);
        chat_message_label_ = lv_label_create(subtitle_container_);
        lv_label_set_text(chat_message_label_, "");
        lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.85 - 20);  
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);  
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_0, 0);  
        lv_obj_set_style_text_line_space(chat_message_label_, -15, 0);  
        lv_obj_align(chat_message_label_, LV_ALIGN_TOP_MID, 0, 0);  
        lv_obj_add_flag(subtitle_container_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);  
        lv_obj_set_style_pad_all(status_bar_, 0, 0);  
        lv_obj_set_style_border_width(status_bar_, 0, 0);  
        lv_obj_set_style_pad_column(status_bar_, 2, 0);  
        lv_obj_set_style_pad_left(status_bar_, 65, 0);  
        lv_obj_set_style_pad_right(status_bar_, 10, 0);  
        network_label_ = lv_label_create(status_bar_);
        lv_label_set_text(network_label_, "");
        lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
        lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
        lv_obj_set_style_pad_right(network_label_, 1, 0);  
        notification_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(notification_label_, 1);  
        lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);  
        lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);  
        lv_label_set_text(notification_label_, "");  
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);  
        status_label_ = lv_label_create(status_bar_);
        lv_obj_set_flex_grow(status_label_, 1);  
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);  
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_LEFT, 0);  
        lv_obj_set_style_text_color(status_label_, current_theme.text, 0);  
        lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);  
        mute_label_ = lv_label_create(status_bar_);
        lv_label_set_text(mute_label_, "");  
        lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);  
        lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);  
        low_battery_popup_ = lv_obj_create(tab1);
        lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);  
        lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);  
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);  
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  
        lv_obj_set_style_radius(low_battery_popup_, 10, 0);  
        lv_obj_t* low_battery_label = lv_label_create(low_battery_popup_);
        lv_label_set_text(low_battery_label, Lang::Strings::BATTERY_NEED_CHARGE);  
        lv_obj_set_style_text_color(low_battery_label, lv_color_white(), 0);  
        lv_obj_center(low_battery_label);  
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);  
    }
    void SetupTab2() {
        lv_obj_set_style_text_font(tab2, fonts_.text_font, 0);  
        lv_obj_set_style_text_color(tab2, lv_color_white(), 0);  
        lv_obj_set_style_bg_color(tab2, lv_color_black(), 0);  
        lv_obj_set_style_bg_opa(tab2, LV_OPA_COVER, 0);  
        tab2_bg_img_ = lv_img_create(tab2);
        lv_obj_set_size(tab2_bg_img_, 240, 240);  
        lv_obj_align(tab2_bg_img_, LV_ALIGN_CENTER, 0, 0);  
        lv_obj_add_flag(tab2_bg_img_, LV_OBJ_FLAG_HIDDEN);  
        lv_obj_set_style_bg_opa(tab2_bg_img_, LV_OPA_TRANSP, 0);  
        lv_obj_move_background(tab2_bg_img_);  
        ESP_LOGI(TAG, "Tab2背景图像对象已创建，指针=%p", (void*)tab2_bg_img_);
        lv_obj_t *second_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(second_label, &time40, 0);  
        lv_obj_set_style_text_color(second_label, lv_color_white(), 0);  
        lv_obj_set_style_bg_opa(second_label, LV_OPA_TRANSP, 0);  
        lv_obj_align(second_label, LV_ALIGN_TOP_MID, 0, 10);  
        lv_label_set_text(second_label, "00");  
        lv_obj_t *date_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(date_label, fonts_.text_font, 0);  
        lv_obj_set_style_text_color(date_label, lv_color_white(), 0);  
        lv_obj_set_style_bg_opa(date_label, LV_OPA_TRANSP, 0);  
        lv_label_set_text(date_label, "01-01");  
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, -60, 35);  
        lv_obj_t *weekday_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(weekday_label, fonts_.text_font, 0);  
        lv_obj_set_style_text_color(weekday_label, lv_color_white(), 0);  
        lv_obj_set_style_bg_opa(weekday_label, LV_OPA_TRANSP, 0);  
        lv_label_set_text(weekday_label, "星期一");  
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 60, 35);  
        lv_obj_t *time_container = lv_obj_create(tab2);
        lv_obj_remove_style_all(time_container);  
        lv_obj_set_size(time_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  
        lv_obj_set_style_pad_all(time_container, 0, 0);  
        lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, 0);  
        lv_obj_set_style_border_width(time_container, 0, 0);  
        lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_ROW);  
        lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  
        lv_obj_align(time_container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_t *hour_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(hour_label, &time70, 0);  
        lv_obj_set_style_text_color(hour_label, lv_color_white(), 0);  
        lv_obj_set_style_bg_opa(hour_label, LV_OPA_TRANSP, 0);  
        lv_label_set_text(hour_label, "00 :");  
        lv_obj_t *minute_label = lv_label_create(time_container);
        lv_obj_set_style_text_font(minute_label, &time70, 0);  
        lv_obj_set_style_text_color(minute_label, lv_color_hex(0xFFA500), 0);  
        lv_obj_set_style_bg_opa(minute_label, LV_OPA_TRANSP, 0);  
        lv_label_set_text(minute_label, " 00");  
        lv_obj_t *lunar_label = lv_label_create(tab2);
        lv_obj_set_style_text_font(lunar_label, &lunar, 0);  
        lv_obj_set_style_text_color(lunar_label, lv_color_white(), 0);  
        lv_obj_set_style_bg_opa(lunar_label, LV_OPA_TRANSP, 0);  
        lv_obj_set_width(lunar_label, LV_HOR_RES * 0.8);  
        lv_label_set_long_mode(lunar_label, LV_LABEL_LONG_WRAP);  
        lv_obj_set_style_text_align(lunar_label, LV_TEXT_ALIGN_CENTER, 0);  
        lv_label_set_text(lunar_label, "农历癸卯年正月初一");  
        lv_obj_align(lunar_label, LV_ALIGN_BOTTOM_MID, 0, -36);  
        lv_obj_t* battery_container = lv_obj_create(tab2);
        lv_obj_set_size(battery_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);  
        lv_obj_set_style_bg_opa(battery_container, LV_OPA_TRANSP, 0);  
        lv_obj_set_style_border_opa(battery_container, LV_OPA_TRANSP, 0);  
        lv_obj_set_style_pad_all(battery_container, 0, 0);  
        lv_obj_set_flex_flow(battery_container, LV_FLEX_FLOW_ROW);  
        lv_obj_set_flex_align(battery_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  
        lv_obj_set_style_pad_column(battery_container, 4, 0);  
        lv_obj_align_to(battery_container, lunar_label, LV_ALIGN_OUT_BOTTOM_MID, -30, 8);  
        battery_label_ = lv_label_create(battery_container);
        lv_label_set_text(battery_label_, "");  
        lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);  
        lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);  
        battery_percentage_label_ = lv_label_create(battery_container);
        lv_label_set_text(battery_percentage_label_, "");  
        lv_obj_set_style_text_font(battery_percentage_label_, &font_puhui_20_4, 0);  
        lv_obj_set_style_text_color(battery_percentage_label_, lv_color_white(), 0);  
        static lv_obj_t* hour_lbl = hour_label;
        static lv_obj_t* minute_lbl = minute_label;
        static lv_obj_t* second_lbl = second_label;
        static lv_obj_t* date_lbl = date_label;
        static lv_obj_t* weekday_lbl = weekday_label;
        static lv_obj_t* lunar_lbl = lunar_label;
        lv_timer_create([](lv_timer_t *t) {
            CustomLcdDisplay* display_instance = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(t));
            if (!display_instance) return;
            static int tab3_update_counter = 0;
            bool should_update_tab3 = false;
            if (display_instance->tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(display_instance->tabview_);
                if (active_tab == 1) {  // 超级省电页面（tab2, 索引1）使用低频更新
                    tab3_update_counter++;
                    if (tab3_update_counter >= 30) {
                        should_update_tab3 = true;
                        tab3_update_counter = 0;
                    }
                    if (!should_update_tab3) {
                        return;
                    }
                } else {
                    tab3_update_counter = 0;
                }
            }
            if (!hour_lbl || !minute_lbl || !second_lbl || 
                !date_lbl || !weekday_lbl || !lunar_lbl) return;
            static auto& board = Board::GetInstance();  
            auto display = board.GetDisplay();
            if (!display) return;
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            static GeoLocationInfo location_cache; 
            static bool location_initialized = false;
            static bool wifi_was_connected = false;
            bool wifi_connected = WifiStation::GetInstance().IsConnected();
            if (wifi_connected && !location_initialized) {
                if (!wifi_was_connected) {
                    ESP_LOGI("ClockTimer", "WiFi connected, attempting to get geolocation for timezone");
                    location_cache = SystemInfo::GetCountryInfo();
                    if (location_cache.is_valid) {
                        location_initialized = true;
                        ESP_LOGI("ClockTimer", "Clock timezone initialized for country %s (UTC%+d)", 
                                 location_cache.country_code.c_str(), location_cache.timezone_offset);
                    } else {
                        ESP_LOGD("ClockTimer", "Geolocation not available yet, using Beijing time");
                    }
                }
            }
            wifi_was_connected = wifi_connected;
            if (location_cache.is_valid && location_cache.timezone_offset != 8) {
                timeinfo = SystemInfo::ConvertFromBeijingTime(timeinfo, location_cache.timezone_offset);
                ESP_LOGD("ClockTimer", "Time converted from Beijing to local timezone UTC%+d", 
                         location_cache.timezone_offset);
            }
            int battery_level;
            bool charging, discharging;
            const char* icon = nullptr;
            if (board.GetBatteryLevel(battery_level, charging, discharging)) {
                ESP_LOGD("ClockTimer", "电池状态 - 电量: %d%%, 充电: %s, 放电: %s", 
                        battery_level, charging ? "是" : "否", discharging ? "是" : "否");
                if (charging) {
                    icon = FONT_AWESOME_BATTERY_CHARGING;
                } else {
                    const char* levels[] = {
                        FONT_AWESOME_BATTERY_EMPTY, 
                        FONT_AWESOME_BATTERY_1,     
                        FONT_AWESOME_BATTERY_2,     
                        FONT_AWESOME_BATTERY_3,     
                        FONT_AWESOME_BATTERY_FULL,  
                        FONT_AWESOME_BATTERY_FULL,  
                    };
                    icon = levels[battery_level / 20];
                }
            }
            {
                DisplayLockGuard lock_guard(display);  
                if (!lock_guard.IsLocked()) {
                    ESP_LOGD("ClockTimer", "无法获取显示锁，跳过本次时钟更新");
                    return;
                }
                char hour_str[6];
                char minute_str[3];
                char second_str[3];
                sprintf(hour_str, "%02d : ", timeinfo.tm_hour);  
                sprintf(minute_str, "%02d", timeinfo.tm_min);    
                sprintf(second_str, "%02d", timeinfo.tm_sec);    
                lv_label_set_text(hour_lbl, hour_str);    
                lv_label_set_text(minute_lbl, minute_str); 
                lv_label_set_text(second_lbl, second_str); 
                char year_str[12];
                snprintf(year_str, sizeof(year_str), "%d", timeinfo.tm_year + 1900);  
                char date_str[25];
                snprintf(date_str, sizeof(date_str), "%d/%d", timeinfo.tm_mon + 1, timeinfo.tm_mday);  
                const char *weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
                lv_label_set_text(date_lbl, date_str);  
                if (timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                    lv_label_set_text(weekday_lbl, weekdays[timeinfo.tm_wday]);  
                }
                std::string lunar_date = LunarCalendar::GetLunarDate(
                    timeinfo.tm_year + 1900,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_mday
                );
                lv_label_set_text(lunar_lbl, lunar_date.c_str());  
                if (icon && display_instance->battery_label_) {
                    lv_label_set_text(display_instance->battery_label_, icon);  
                    ESP_LOGD("ClockTimer", "电池图标已更新: %s", icon);  
                }
                if (display_instance->battery_percentage_label_) {
                    char battery_text[8];
                    snprintf(battery_text, sizeof(battery_text), "%d%%", battery_level);
                    lv_label_set_text(display_instance->battery_percentage_label_, battery_text);  
                    ESP_LOGD("ClockTimer", "电池百分比已更新: %s", battery_text);  
                }
                if (display_instance->tab3_time_label_) {
                    char time_str[8];
                    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
                    lv_label_set_text(display_instance->tab3_time_label_, time_str);
                }
                if (display_instance->tab3_date_label_) {
                    char date_str[32];
                    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", 
                             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
                    lv_label_set_text(display_instance->tab3_date_label_, date_str);
                }
                if (display_instance->tab3_weekday_label_ && timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7) {
                    lv_label_set_text(display_instance->tab3_weekday_label_, weekdays[timeinfo.tm_wday]);
                }
            }  
        }, 2000, this);  
    }
    virtual void SetupUI() override {
        DisplayLockGuard lock(this);  
        Settings settings("display", false);  
        current_theme_name_ = settings.GetString("theme", "dark");  
        if (current_theme_name_ == "dark" || current_theme_name_ == "DARK") {
            current_theme = DARK_THEME;  
        } else if (current_theme_name_ == "light" || current_theme_name_ == "LIGHT") {
            current_theme = LIGHT_THEME;  
        }  
        ESP_LOGI(TAG, "SetupUI --------------------------------------");  
        lv_obj_t * screen = lv_screen_active();  
        lv_obj_set_style_bg_color(screen, current_theme.background, 0);  
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);  
        tabview_ = lv_tabview_create(lv_scr_act());  
        lv_obj_set_size(tabview_, lv_pct(100), lv_pct(100));  
        lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);  
        lv_tabview_set_tab_bar_size(tabview_, 0);  
        lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview_);  
        lv_obj_add_flag(tab_btns, LV_OBJ_FLAG_HIDDEN);  
        lv_obj_t * content = lv_tabview_get_content(tabview_);  
        lv_obj_set_scroll_snap_x(content, LV_SCROLL_SNAP_CENTER);  
        tab1 = lv_tabview_add_tab(tabview_, "Tab1");  
        tab2 = lv_tabview_add_tab(tabview_, "Tab2");  
        tab3 = lv_tabview_add_tab(tabview_, "Tab3");  
        lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(tab1, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(tab2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(tab3, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(tab3, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_color(tab3, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(tab3, LV_OPA_COVER, 0);
        tab3_time_label_ = lv_label_create(tab3);
        lv_obj_set_style_text_font(tab3_time_label_, &time40, 0);  
        lv_obj_set_style_text_color(tab3_time_label_, lv_color_white(), 0);
        lv_obj_align(tab3_time_label_, LV_ALIGN_CENTER, 0, -30);  
        lv_label_set_text(tab3_time_label_, "00:00");
        tab3_date_label_ = lv_label_create(tab3);
        lv_obj_set_style_text_font(tab3_date_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_date_label_, lv_color_white(), 0);
        lv_obj_align(tab3_date_label_, LV_ALIGN_CENTER, 0, 15);  
        lv_label_set_text(tab3_date_label_, "2024-01-01");
        tab3_weekday_label_ = lv_label_create(tab3);
        lv_obj_set_style_text_font(tab3_weekday_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_weekday_label_, lv_color_white(), 0);
        lv_obj_align(tab3_weekday_label_, LV_ALIGN_CENTER, 0, 40);  
        lv_label_set_text(tab3_weekday_label_, "星期一");
        tab3_mode_label_ = lv_label_create(tab3);
        lv_obj_set_style_text_font(tab3_mode_label_, fonts_.text_font, 0);
        lv_obj_set_style_text_color(tab3_mode_label_, lv_color_make(100, 100, 100), 0);  
        lv_obj_align(tab3_mode_label_, LV_ALIGN_BOTTOM_MID, 0, -20);  
        lv_label_set_text(tab3_mode_label_, "超级省电模式");
        lv_obj_add_event_cb(tab1, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_foreground(display->GetCanvas());
            }
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);  
        lv_obj_add_event_cb(tab2, [](lv_event_t *e) {
            CustomLcdDisplay *display = (CustomLcdDisplay *)lv_event_get_user_data(e);
            if (!display) return;
            lv_obj_move_foreground(display->tab2);
            if (display->GetCanvas() != nullptr) {
                lv_obj_move_background(display->GetCanvas());
            }
            if (display->idle_timer_ != nullptr) {
                lv_timer_del(display->idle_timer_);
                display->idle_timer_ = nullptr;
            }
        }, LV_EVENT_CLICKED, this);
        SetupTab1();  
        SetupTab2();  
        center_notification_bg_ = lv_obj_create(screen);
        lv_obj_set_size(center_notification_bg_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_pos(center_notification_bg_, 0, 0);
        lv_obj_set_style_bg_color(center_notification_bg_, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(center_notification_bg_, LV_OPA_50, 0);
        lv_obj_set_style_border_width(center_notification_bg_, 0, 0);
        lv_obj_set_scrollbar_mode(center_notification_bg_, LV_SCROLLBAR_MODE_OFF);
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
        center_notification_label_ = lv_label_create(center_notification_popup_);
        lv_label_set_long_mode(center_notification_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(center_notification_label_, LV_HOR_RES * 0.85 - 40);
        lv_obj_set_style_text_align(center_notification_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(center_notification_label_, lv_color_black(), 0);
        lv_label_set_text(center_notification_label_, "");
        lv_obj_center(center_notification_label_);
        lv_obj_add_flag(center_notification_bg_, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "中央通知弹窗已创建");
    }
    virtual void SetTheme(const std::string& theme_name) override {
        DisplayLockGuard lock(this);  
        current_theme = DARK_THEME;  
        if (theme_name == "dark" || theme_name == "DARK") {
            current_theme = DARK_THEME;  
        } else if (theme_name == "light" || theme_name == "LIGHT") {
            current_theme = LIGHT_THEME;  
        } else {
            ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
            return;
        }
        lv_obj_t* screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, current_theme.background, 0);  
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);  
        lv_obj_set_style_text_color(screen, current_theme.text, 0);      
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, current_theme.background, 0);  
            lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);  
            lv_obj_set_style_border_color(container_, current_theme.border, 0);  
        }
        if (status_bar_ != nullptr) {
            lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);  
            lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  
            lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);      
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
                lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);  
            }
            if (battery_percentage_label_ != nullptr) {
                lv_obj_set_style_text_color(battery_percentage_label_, lv_color_white(), 0);  
            }
        }
        if (content_ != nullptr) {
            lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);  
            lv_obj_set_style_border_color(content_, current_theme.border, 0);       
            if (chat_message_label_ != nullptr) {
                lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);  
            }
        }
        if (low_battery_popup_ != nullptr) {
            lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);  
        }
        current_theme_name_ = theme_name;  
        Settings settings("display", true);  
        settings.SetString("theme", theme_name);  
    }
    void ShowDownloadProgress(bool show, int progress = 0, const char* message = nullptr) {
        if (!show) {
            UpdateDownloadProgressUI(false, 0, nullptr);
            return;
        }
        UpdateDownloadProgressUI(true, progress, message);
    }
public:
    lv_obj_t* download_progress_container_ = nullptr;
    lv_obj_t* download_progress_label_ = nullptr; 
    lv_obj_t* message_label_ = nullptr;          
    lv_obj_t* download_progress_arc_ = nullptr;  
    lv_obj_t* preload_progress_container_ = nullptr;
    lv_obj_t* preload_progress_label_ = nullptr;
    lv_obj_t* preload_message_label_ = nullptr;
    lv_obj_t* preload_progress_arc_ = nullptr;
    lv_obj_t* preload_percentage_label_ = nullptr;
    bool user_interaction_disabled_ = false;
    void UpdatePreloadProgressUI(bool show, int current, int total, const char* message) {
        DisplayLockGuard lock(this);
        if (preload_progress_container_ == nullptr && show) {
            CreatePreloadProgressUI();
            DisableUserInteraction(); 
        }
        if (preload_progress_container_ == nullptr) {
            return;
        }
        if (show) {
            if (preload_progress_arc_ && total > 0) {
                int progress_value = (current * 100) / total;
                if (progress_value > 100) progress_value = 100;
                if (progress_value < 0) progress_value = 0;
                lv_arc_set_value(preload_progress_arc_, progress_value);
                lv_obj_set_style_arc_color(preload_progress_arc_, lv_color_hex(0x007AFF), LV_PART_INDICATOR);
            }
            if (preload_message_label_ != nullptr) {
                lv_label_set_text(preload_message_label_, "设备正在预热中...");
            }
            lv_obj_clear_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(preload_progress_container_);
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            ESP_LOGI(TAG, "预加载完成，隐藏新版预加载UI容器");
            if (preload_progress_container_) {
                lv_obj_add_flag(preload_progress_container_, LV_OBJ_FLAG_HIDDEN);
                ESP_LOGI(TAG, "新版预加载UI容器已隐藏");
            } else {
                ESP_LOGI(TAG, "新版预加载UI容器为空，无需隐藏");
            }
            ESP_LOGI(TAG, "预加载完成，准备重新启用用户交互");
            EnableUserInteraction();
        }
    }
private:
    void CreateDownloadProgressUI() {
        download_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(download_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(download_progress_container_);
        lv_obj_set_style_bg_color(download_progress_container_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(download_progress_container_, LV_OPA_COVER, 0);  
        lv_obj_set_style_border_width(download_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(download_progress_container_, 0, 0);
        lv_obj_t* progress_arc = lv_arc_create(download_progress_container_);
        lv_obj_set_size(progress_arc, 120, 120);
        lv_arc_set_rotation(progress_arc, 270); 
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        lv_obj_align(progress_arc, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN); 
        lv_obj_set_style_arc_width(progress_arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x00D4FF), LV_PART_INDICATOR); 
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        download_progress_arc_ = progress_arc;
        download_progress_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(download_progress_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(download_progress_label_, lv_color_black(), 0);  
        lv_obj_set_style_text_align(download_progress_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(download_progress_label_, "0%");
        lv_obj_align_to(download_progress_label_, progress_arc, LV_ALIGN_CENTER, 0, 0);
        message_label_ = lv_label_create(download_progress_container_);
        lv_obj_set_style_text_font(message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(message_label_, lv_color_black(), 0);  
        lv_obj_set_style_text_align(message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(message_label_, lv_pct(80));
        lv_label_set_long_mode(message_label_, LV_LABEL_LONG_WRAP);
        lv_label_set_text(message_label_, Lang::Strings::PREPARING_DOWNLOAD_RESOURCES);
        lv_obj_align_to(message_label_, progress_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
        lv_obj_move_foreground(download_progress_container_);
    }
    void CreatePreloadProgressUI() {
        preload_progress_container_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(preload_progress_container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_center(preload_progress_container_);
        lv_obj_set_style_bg_opa(preload_progress_container_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preload_progress_container_, 0, 0);
        lv_obj_set_style_pad_all(preload_progress_container_, 0, 0);
        lv_obj_set_flex_flow(preload_progress_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(preload_progress_container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(preload_progress_container_, 20, 0);
        lv_obj_t* progress_arc = lv_arc_create(preload_progress_container_);
        lv_obj_set_size(progress_arc, 80, 80);
        lv_arc_set_rotation(progress_arc, 270); 
        lv_arc_set_bg_angles(progress_arc, 0, 360);
        lv_arc_set_value(progress_arc, 0);
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x3A3A3C), LV_PART_MAIN); 
        lv_obj_set_style_arc_width(progress_arc, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(progress_arc, lv_color_hex(0x007AFF), LV_PART_INDICATOR); 
        lv_obj_set_style_bg_opa(progress_arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(progress_arc, 0, LV_PART_KNOB);
        lv_obj_remove_flag(progress_arc, LV_OBJ_FLAG_CLICKABLE);
        preload_progress_arc_ = progress_arc;
        preload_message_label_ = lv_label_create(preload_progress_container_);
        lv_obj_set_style_text_font(preload_message_label_, &font_puhui_20_4, 0);
        lv_obj_set_style_text_color(preload_message_label_, lv_color_black(), 0);
        lv_obj_set_style_text_align(preload_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(preload_message_label_, "设备正在预热中...");
        preload_progress_label_ = nullptr;
        preload_percentage_label_ = nullptr;
        lv_obj_move_foreground(preload_progress_container_);
    }
    void DisableUserInteraction() {
        user_interaction_disabled_ = true;
        ESP_LOGI(TAG, "用户交互已禁用");
        ESP_LOGI(TAG, "调用 SetIdle(false) 禁用空闲定时器");
        SetIdle(false);
    }
    void EnableUserInteraction() {
        user_interaction_disabled_ = false;
        ESP_LOGI(TAG, "用户交互已启用");
        auto& wifi_station = WifiStation::GetInstance();
        auto& app = Application::GetInstance();
        if (wifi_station.IsConnected() && app.GetDeviceState() == kDeviceStateIdle) {
            ESP_LOGI(TAG, "设备预热完成，播放联网成功提示音");
            app.PlaySound(Lang::Sounds::P3_SUCCESS);
        }
        ESP_LOGI(TAG, "调用 SetIdle(true) 重新启用空闲定时器");
        SetIdle(true);
    }
    void UpdateDownloadProgressUI(bool show, int progress, const char* message) {
        DisplayLockGuard lock(this);
        if (!lock.IsLocked()) {
            ESP_LOGW(TAG, "无法获取显示锁以更新下载进度UI，跳过本次更新");
            return;
        }
        if (download_progress_container_ == nullptr && show) {
            CreateDownloadProgressUI();
        }
        if (download_progress_container_ == nullptr) {
            return;
        }
        if (show) {
            if (progress < 0) progress = 0;
            if (progress > 100) progress = 100;
            if (download_progress_arc_) {
                lv_arc_set_value(download_progress_arc_, progress);
                if (progress < 30) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00D4FF), LV_PART_INDICATOR);
                } else if (progress < 70) {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FFB3), LV_PART_INDICATOR);
                } else {
                    lv_obj_set_style_arc_color(download_progress_arc_, lv_color_hex(0x00FF7F), LV_PART_INDICATOR);
                }
            }
            if (download_progress_label_) {
                char percent_text[8];
                snprintf(percent_text, sizeof(percent_text), "%d%%", progress);
                lv_label_set_text(download_progress_label_, percent_text);
            }
            if (message && message_label_ != nullptr) {
                lv_label_set_text(message_label_, message);
            }
            lv_obj_clear_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(download_progress_container_);
            SetIdle(false);
            if (tabview_) {
                uint32_t active_tab = lv_tabview_get_tab_act(tabview_);
                if (active_tab == 1) {
                    lv_tabview_set_act(tabview_, 0, LV_ANIM_OFF);
                }
            }
        } else {
            lv_obj_add_flag(download_progress_container_, LV_OBJ_FLAG_HIDDEN);
            SetIdle(true);
        }
    }
};
class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;  
    CustomLcdDisplay* display_;              
    Button boot_btn;                         
    esp_lcd_panel_io_handle_t io_handle = nullptr;  
    esp_lcd_panel_handle_t panel = nullptr;        
    TaskHandle_t image_task_handle_ = nullptr;
    PowerManager* power_manager_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    bool is_light_sleeping_ = false;
    bool is_in_super_power_save_ = false;
    bool is_alarm_pre_wake_active_ = false;
    lv_timer_t* alarm_monitor_timer_ = nullptr;
    MqttMusicHandler* mqtt_music_handler_ = nullptr;
    bool is_screen_rotated_ = false;
    esp_timer_handle_t rotation_check_timer_ = nullptr;
    bool press_to_talk_mode_ = false;  
    static const char* API_URL;
    static const char* VERSION_URL;
    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,                    
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,    
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,    
            .clk_source = I2C_CLK_SRC_DEFAULT,        
            .glitch_ignore_cnt = 7,                   
            .intr_priority = 0,                       
            .trans_queue_depth = 0,                   
            .flags = {
                .enable_internal_pullup = 1,          
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));  
    }
    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN, 
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));    
    }
    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Init GC9A01 display");
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, NULL, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));  
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;    
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;           
        panel_config.bits_per_pixel = 16;                       
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel));  
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));  
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));  
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));  
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));            
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));  
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true)); 
        display_ = new CustomLcdDisplay(io_handle, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }
    void RotateScreen(bool rotate_90_degrees) {
        if (panel == nullptr) {
            ESP_LOGE(TAG, "LCD panel未初始化，无法旋转屏幕");
            return;
        }
        if (image_task_handle_ != nullptr) {
            vTaskSuspend(image_task_handle_);
            ESP_LOGI(TAG, "已暂停图片轮播任务");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        if (display_) {
            DisplayLockGuard lock(display_);
            if (rotate_90_degrees) {
                ESP_LOGI(TAG, "旋转屏幕90度");
                ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, false));
                lv_display_t* lv_disp = display_->GetLvDisplay();
                if (lv_disp) {
                    lv_display_set_rotation(lv_disp, LV_DISPLAY_ROTATION_270);
                    ESP_LOGI(TAG, "LVGL显示旋转已更新为270度");
                }
            } else {
                ESP_LOGI(TAG, "恢复屏幕到正常角度");
                ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
                lv_display_t* lv_disp = display_->GetLvDisplay();
                if (lv_disp) {
                    lv_display_set_rotation(lv_disp, LV_DISPLAY_ROTATION_0);
                    ESP_LOGI(TAG, "LVGL显示旋转已恢复为0度");
                }
            }
            is_screen_rotated_ = rotate_90_degrees;
            if (rotate_90_degrees) {
                if (rotation_check_timer_ == nullptr) {
                    esp_timer_create_args_t timer_args = {
                        .callback = [](void* arg) {
                            CustomBoard* self = static_cast<CustomBoard*>(arg);
                            if (self->power_manager_ && self->is_screen_rotated_) {
                                if (!self->power_manager_->IsUsbConnected()) {
                                    ESP_LOGI(TAG, "USB已断开，自动恢复屏幕");
                                    self->RotateScreen(false);
                                    if (self->display_) {
                                        self->display_->ShowCenterNotification(Lang::Strings::CHARGING_DOCK_DISCONNECTED, 3000);
                                    }
                                }
                            }
                        },
                        .arg = this,
                        .dispatch_method = ESP_TIMER_TASK,
                        .name = "rotation_check",
                        .skip_unhandled_events = true
                    };
                    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &rotation_check_timer_));
                    ESP_ERROR_CHECK(esp_timer_start_periodic(rotation_check_timer_, 2000000));  
                    ESP_LOGI(TAG, "已启动旋转状态检查定时器");
                }
            } else {
                if (rotation_check_timer_ != nullptr) {
                    esp_timer_stop(rotation_check_timer_);
                    esp_timer_delete(rotation_check_timer_);
                    rotation_check_timer_ = nullptr;
                    ESP_LOGI(TAG, "已停止旋转状态检查定时器");
                }
            }
        }
        if (image_task_handle_ != nullptr) {
            vTaskResume(image_task_handle_);
            ESP_LOGI(TAG, "已恢复图片轮播任务");
        }
    }
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   
    }
    void SetupMusicPlayerCallbacks() {
        if (!display_ || !display_->music_player_ui_) {
            ESP_LOGW(TAG, "音乐播放器UI未初始化，跳过回调设置");
            return;
        }
        display_->music_player_ui_->SetBeforeShowCallback([this]() {
            ESP_LOGI(TAG, "🎵 音乐播放器显示前：隐藏背景UI，暂停图片任务");
            this->SuspendImageTask();
            if (display_->subtitle_container_) {
                DisplayLockGuard lock(display_);
                if (lock.IsLocked()) {
                    lv_obj_add_flag(display_->subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                } else {
                    ESP_LOGW(TAG, "无法获取显示锁以隐藏字幕容器");
                }
            }
        });
        display_->music_player_ui_->SetAfterHideCallback([this]() {
            ESP_LOGI(TAG, "🎵 音乐播放器隐藏后：恢复背景UI，恢复图片任务");
            this->ResumeImageTask();
            if (display_->subtitle_container_ && display_->IsSubtitleEnabled()) {
                DisplayLockGuard lock(display_);
                if (lock.IsLocked()) {
                    lv_obj_clear_flag(display_->subtitle_container_, LV_OBJ_FLAG_HIDDEN);
                } else {
                    ESP_LOGW(TAG, "无法获取显示锁以恢复字幕容器");
                }
            }
        });
        ESP_LOGI(TAG, "音乐播放器UI优化回调已注册");
    }
    void InitializeButtons() {
        boot_btn.OnClick([this]() {
            if (is_in_super_power_save_) {
                ESP_LOGI(TAG, "从超级省电模式唤醒设备");
                is_in_super_power_save_ = false;
                power_save_timer_->SetEnabled(true);
                power_save_timer_->WakeUp();
                ESP_LOGI(TAG, "省电定时器已重新启用");
                esp_pm_config_t pm_config = {
                    .max_freq_mhz = 160,     
                    .min_freq_mhz = 40,      
                    .light_sleep_enable = false,  
                };
                esp_pm_configure(&pm_config);
                ESP_LOGI(TAG, "CPU频率已恢复到160MHz");
                if (power_manager_) {
                    power_manager_->StartTimer();
                }
                auto& app_timer = Application::GetInstance();
                app_timer.StartClockTimer();
                auto backlight = GetBacklight();
                if (backlight) {
                    backlight->RestoreBrightness();
                    ESP_LOGI(TAG, "屏幕亮度已恢复");
                }
                auto& app_restore = Application::GetInstance();
                app_restore.ResumeAudioProcessing();
                ESP_LOGI(TAG, "音频处理系统已恢复");
                auto& wifi_station = WifiStation::GetInstance();
                ESP_LOGI(TAG, "正在重新初始化WiFi...");
                wifi_station.Start();
                int wifi_wait_count = 0;
                const int max_wifi_wait = 150; 
                ESP_LOGI(TAG, "等待WiFi完全初始化和连接...");
                while (!IsWifiFullyConnected() && wifi_wait_count < max_wifi_wait) {
                    vTaskDelay(pdMS_TO_TICKS(100));  
                    wifi_wait_count++;
                    if (wifi_wait_count % 20 == 0) {
                        ESP_LOGI(TAG, "等待WiFi连接... (%d/%d 秒)", wifi_wait_count/10, max_wifi_wait/10);
                    }
                }
                if (IsWifiFullyConnected()) {
                    ESP_LOGI(TAG, "WiFi完全连接成功，禁用WiFi省电模式");
                    SetPowerSaveMode(false);
                    ESP_LOGI(TAG, "WiFi省电模式已禁用");
                    auto& app_mqtt = Application::GetInstance();
                    ESP_LOGI(TAG, "WiFi已完全连接，重新启动MQTT通知服务...");
                    app_mqtt.StartMqttNotifier();
                } else {
                    ESP_LOGW(TAG, "WiFi连接超时或驱动未完全初始化，跳过省电模式设置和MQTT连接");
                }
                ResumeImageTask();
                ESP_LOGI(TAG, "图片轮播任务已恢复");
                if (display_) {
                    CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
                    if (customDisplay->tabview_) {
                        DisplayLockGuard lock(display_);
                        ESP_LOGI(TAG, "从超级省电模式唤醒：切换回主页面（tab1）");
                        lv_tabview_set_act(customDisplay->tabview_, 0, LV_ANIM_OFF);  
                    }
                }
                ESP_LOGI(TAG, "从超级省电模式完全恢复到正常状态");
                return; 
            }
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
                ESP_LOGI(TAG, "用户交互，唤醒省电定时器");
            }
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略按钮点击");
                return;
            }
            // 按住对话模式下，单击无效
            if (press_to_talk_mode_) {
                ESP_LOGI(TAG, "按住对话模式下，单击事件被忽略");
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();  
            }
            app.ToggleChatState();  
        });
        boot_btn.OnLongPress([this]() {
            ESP_LOGI(TAG, "检测到长按boot按键");
            // 按住对话模式下，长按用于说话，禁用屏幕旋转
            if (press_to_talk_mode_) {
                ESP_LOGI(TAG, "按住对话模式下，长按用于收音，屏幕旋转功能已禁用");
                return;
            }
            if (is_in_super_power_save_) {
                ESP_LOGW(TAG, "设备处于超级省电模式，忽略屏幕旋转操作");
                return;
            }
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略屏幕旋转操作");
                return;
            }
            bool usb_connected = power_manager_ ? power_manager_->IsUsbConnected() : false;
            ESP_LOGI(TAG, "长按按键触发 - USB连接状态检查: %s", usb_connected ? "已连接" : "未连接");
            if (!usb_connected) {
                ESP_LOGW(TAG, "USB未插入，屏幕旋转功能仅在连接充电底座时可用");
                if (display_) {
                    display_->ShowCenterNotification(Lang::Strings::CONNECT_CHARGING_DOCK, 3000);
                }
                return;
            }
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
            }
            bool new_rotation_state = !is_screen_rotated_;
            RotateScreen(new_rotation_state);
            if (display_) {
                display_->ShowCenterNotification(Lang::Strings::SCREEN_ROTATED, 3000);
            }
            ESP_LOGI(TAG, "屏幕旋转状态: %s", new_rotation_state ? "已旋转90度" : "正常");
        });
        
        // 注册3次点击事件 - 切换按住对话模式
        boot_btn.OnMultipleClick(3, [this]() {
            ESP_LOGI(TAG, "检测到3次点击，切换对话模式");
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
            }
            if (is_in_super_power_save_) {
                ESP_LOGW(TAG, "设备处于超级省电模式，忽略模式切换");
                return;
            }
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略模式切换");
                return;
            }
            
            // 切换模式
            press_to_talk_mode_ = !press_to_talk_mode_;
            ESP_LOGI(TAG, "对话模式已切换为: %s", press_to_talk_mode_ ? "按住对话" : "点击对话");
            
            // 显示通知
            if (display_) {
                const char* message = press_to_talk_mode_ ? "按住对话模式" : "连续对话模式";
                display_->ShowCenterNotification(message, 2000);
            }
        });
        
        // 注册按下事件 - 按住对话模式下开始收音
        boot_btn.OnPressDown([this]() {
            if (power_save_timer_) {
                power_save_timer_->WakeUp();
            }
            if (!press_to_talk_mode_) {
                return;
            }
            if (is_in_super_power_save_) {
                ESP_LOGW(TAG, "设备处于超级省电模式，忽略按键");
                return;
            }
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                ESP_LOGW(TAG, "用户交互已禁用，忽略按键");
                return;
            }
            
            ESP_LOGI(TAG, "按住对话模式：开始收音（不发送唤醒消息）");
            Application::GetInstance().StartListening(true);  // 传入true跳过唤醒消息
        });
        
        // 注册松开事件 - 按住对话模式下停止收音
        boot_btn.OnPressUp([this]() {
            if (!press_to_talk_mode_) {
                return;
            }
            if (is_in_super_power_save_) {
                return;
            }
            if (display_ && static_cast<CustomLcdDisplay*>(display_)->user_interaction_disabled_) {
                return;
            }
            
            ESP_LOGI(TAG, "按住对话模式：停止收音");
            Application::GetInstance().StopListening();
        });
    }
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));         
        thing_manager.AddThing(iot::CreateThing("Screen"));          
        thing_manager.AddThing(iot::CreateThing("RotateDisplay"));   
        thing_manager.AddThing(iot::CreateThing("ImageDisplay"));    
        thing_manager.AddThing(iot::CreateThing("SubtitleControl")); 
        thing_manager.AddThing(new iot::MusicPlayerThing());
#if CONFIG_USE_ALARM
        thing_manager.AddThing(iot::CreateThing("AlarmIot"));
#endif
    }
    void InitializeImageResources() {
        auto& image_manager = ImageResourceManager::GetInstance();
#ifdef DEBUG_CLEAR_CORRUPTED_FILES
        ESP_LOGI(TAG, "调试模式：清理所有图片文件");
        image_manager.ClearAllImageFiles();
#endif
        esp_err_t result = image_manager.Initialize();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "图片资源管理器初始化失败");
        }
        image_manager.PreloadRemainingImagesSilent(0);
    }
    void InitializePowerManager() {
        power_manager_ = new PowerManager(CHARGING_STATUS_PIN, USB_DETECT_PIN);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            ESP_LOGI(TAG, "充电状态变化: %s", is_charging ? "充电中" : "未充电");
        });
        power_manager_->OnLowBatteryStatusChanged([this](bool is_low_battery) {
            ESP_LOGI(TAG, "低电量状态变化: %s", is_low_battery ? "低电量" : "正常电量");
            if (is_low_battery && display_) {
                display_->ShowNotification("电量不足，请及时充电", 5000);
            }
        });
        ESP_LOGI(TAG, "电源管理器初始化完成");
    }
    bool CanEnterPowerSaveMode() {
        auto& app = Application::GetInstance();
        if (!app.CanEnterSleepMode()) {
            return false;
        }
        DeviceState currentState = app.GetDeviceState();
        if (currentState == kDeviceStateActivating || currentState == kDeviceStateUpgrading) {
            ESP_LOGD(TAG, "设备处于激活/升级状态，不进入节能模式");
            return false;
        }
        int battery_level;
        bool charging, discharging;
        if (GetBatteryLevel(battery_level, charging, discharging)) {
            if (charging) {
                ESP_LOGD(TAG, "设备正在充电，不进入节能模式");
                return false;
            }
            if (battery_level >= 95) {
                ESP_LOGD(TAG, "设备电量很高(>=95)，很可能插着电源，不进入节能模式");
                return false;
            }
        }
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->download_progress_container_ && 
                !lv_obj_has_flag(customDisplay->download_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                ESP_LOGD(TAG, "正在下载图片，不进入节能模式");
                return false;
            }
            if (customDisplay->preload_progress_container_ && 
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                ESP_LOGD(TAG, "正在预加载图片，不进入节能模式");
                return false;
            }
            if (customDisplay->user_interaction_disabled_) {
                ESP_LOGD(TAG, "用户交互被禁用，系统忙碌，不进入节能模式");
                return false;
            }
        }
#if CONFIG_USE_ALARM
        if (app.alarm_m_ != nullptr) {
            time_t now = time(NULL);
            auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
            if (next_alarm.has_value()) {
                int seconds_to_alarm = (int)(next_alarm->time - now);
                if (seconds_to_alarm > 0 && seconds_to_alarm <= 60) {
                    ESP_LOGD(TAG, "闹钟 '%s' 将在 %d 秒内响起，不进入超级省电模式", 
                             next_alarm->name.c_str(), seconds_to_alarm);
                    return false;
                }
                ESP_LOGI(TAG, "有活动闹钟 '%s'，但距离响起还有 %d 秒，仍可进入超级省电模式（将保留闹钟功能）", 
                         next_alarm->name.c_str(), seconds_to_alarm);
            }
        }
#endif
        ESP_LOGD(TAG, "系统空闲，允许进入节能模式");
        return true;
    }
    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(160, 60, 180);
        power_save_timer_->OnEnterSleepMode([this]() {
            if (!CanEnterPowerSaveMode()) {
                ESP_LOGI(TAG, "系统忙碌，取消进入浅睡眠模式");
                power_save_timer_->WakeUp();  
                return;
            }
            ESP_LOGI(TAG, "60秒后进入浅睡眠模式");
            EnterLightSleepMode();
        });
        power_save_timer_->OnExitSleepMode([this]() {
            ESP_LOGI(TAG, "退出浅睡眠模式");
            ExitLightSleepMode();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            if (!CanEnterPowerSaveMode()) {
                ESP_LOGI(TAG, "系统忙碌，取消进入超级省电模式");
                power_save_timer_->WakeUp();  
                return;
            }
            ESP_LOGI(TAG, "180秒后进入超级省电模式");
            EnterDeepSleepMode();
        });
        power_save_timer_->SetEnabled(true);
        ESP_LOGI(TAG, "3级省电定时器初始化完成 - 60秒浅睡眠, 180秒超级省电");
    }
    void InitializeMqttMusicHandler() {
        if (!mqtt_music_handler_) {
            try {
                auto& app = Application::GetInstance();
                const auto& device_config = app.GetDeviceConfig();
                mqtt_music_handler_ = new MqttMusicHandler();
                mqtt_music_handler_->SetBrokerHost(device_config.mqtt_host);
                mqtt_music_handler_->SetBrokerPort(device_config.mqtt_port);
                mqtt_music_handler_->SetUsername(device_config.mqtt_username);
                mqtt_music_handler_->SetPassword(device_config.mqtt_password);
                mqtt_music_handler_->SetClientId(device_config.device_id);
                mqtt_music_handler_->SetMusicCommandCallback([this](const char* command, const char* params) {
                    if (display_) {
                        HandleMusicCommand(command, params);
                    }
                });
                if (mqtt_music_handler_->Connect()) {
                    ESP_LOGI(TAG, "MQTT音乐控制器初始化成功");
                } else {
                    ESP_LOGE(TAG, "MQTT音乐控制器连接失败");
                }
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "MQTT音乐控制器初始化异常: %s", e.what());
                if (mqtt_music_handler_) {
                    delete mqtt_music_handler_;
                    mqtt_music_handler_ = nullptr;
                }
            }
        }
    }
    void HandleMusicCommand(const char* command, const char* params) {
        if (!display_) return;
        ESP_LOGI(TAG, "收到音乐控制命令: %s, 参数: %s", command, params ? params : "无");
        if (strcmp(command, "show") == 0) {
            const char* title = "未知歌曲";
            const char* artist = "未知艺术家";
            const char* album_cover = nullptr;
            uint32_t duration_ms = 30000;
            if (params) {
                cJSON* json = cJSON_Parse(params);
                if (json) {
                    cJSON* title_item = cJSON_GetObjectItem(json, "title");
                    if (title_item && cJSON_IsString(title_item)) {
                        title = title_item->valuestring;
                    }
                    cJSON* artist_item = cJSON_GetObjectItem(json, "artist");
                    if (artist_item && cJSON_IsString(artist_item)) {
                        artist = artist_item->valuestring;
                    }
                    cJSON* cover_item = cJSON_GetObjectItem(json, "album_cover");
                    if (cover_item && cJSON_IsString(cover_item)) {
                        album_cover = cover_item->valuestring;
                    }
                    cJSON* duration_item = cJSON_GetObjectItem(json, "duration_ms");
                    if (duration_item && cJSON_IsNumber(duration_item)) {
                        duration_ms = (uint32_t)duration_item->valuedouble;
                    }
                    cJSON_Delete(json);
                }
            }
            display_->ShowMusicPlayer(album_cover, title, artist, duration_ms);
        } else if (strcmp(command, "hide") == 0) {
            display_->HideMusicPlayer();
        } else if (strcmp(command, "spectrum") == 0 && params) {
            cJSON* json = cJSON_Parse(params);
            if (json) {
                cJSON* spectrum_array = cJSON_GetObjectItem(json, "spectrum");
                if (spectrum_array && cJSON_IsArray(spectrum_array)) {
                    int array_size = cJSON_GetArraySize(spectrum_array);
                    if (array_size > 0 && array_size <= 32) {
                        float spectrum_data[32] = {0};
                        for (int i = 0; i < array_size; i++) {
                            cJSON* item = cJSON_GetArrayItem(spectrum_array, i);
                            if (item && cJSON_IsNumber(item)) {
                                spectrum_data[i] = (float)item->valuedouble;
                            }
                        }
                        display_->UpdateMusicSpectrum(spectrum_data, array_size);
                    }
                }
                cJSON_Delete(json);
            }
        }
    }
    void AdjustAlarmCheckFrequency() {
#if CONFIG_USE_ALARM
        if (!alarm_monitor_timer_) return;
        auto& app = Application::GetInstance();
        if (app.alarm_m_ == nullptr) return;
        time_t now = time(NULL);
        auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
        uint32_t new_period_ms = 2000; 
        if (!next_alarm.has_value()) {
            new_period_ms = 60000; 
            ESP_LOGD(TAG, "无活动闹钟，检测频率降至60秒");
        } else {
            time_t alarm_time = next_alarm->time;
            int seconds_to_alarm = (int)(alarm_time - now);
            if (seconds_to_alarm <= 0) {
                new_period_ms = 2000;
            } else if (seconds_to_alarm > 7200) {
                new_period_ms = 300000;  
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：5分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 3600) {
                new_period_ms = 120000;  
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：2分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 1800) {
                new_period_ms = 60000;   
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：1分钟", seconds_to_alarm);
            } else if (seconds_to_alarm > 600) {
                new_period_ms = 30000;   
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：30秒", seconds_to_alarm);
            } else if (seconds_to_alarm > 300) {
                new_period_ms = 10000;   
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：10秒", seconds_to_alarm);
            } else {
                new_period_ms = 5000;    
                ESP_LOGD(TAG, "闹钟将在%d秒后响起，检测频率：5秒", seconds_to_alarm);
            }
        }
        lv_timer_set_period(alarm_monitor_timer_, new_period_ms);
        ESP_LOGI(TAG, "闹钟检测频率已调整为：%lu ms", (unsigned long)new_period_ms);
#endif
    }
    void InitializeAlarmMonitor() {
#if CONFIG_USE_ALARM
        ESP_LOGI(TAG, "初始化智能动态闹钟监听器");
        alarm_monitor_timer_ = lv_timer_create([](lv_timer_t *t) {
            CustomBoard* board = static_cast<CustomBoard*>(lv_timer_get_user_data(t));
            if (!board) return;
            auto& app = Application::GetInstance();
            if (app.alarm_m_ == nullptr) return;
            time_t now = time(NULL);
            if (app.alarm_m_->IsRing()) {
                ESP_LOGI(TAG, "检测到闹钟触发");
                if (board->IsInSuperPowerSaveMode()) {
                    ESP_LOGI(TAG, "闹钟触发：从超级省电模式唤醒设备");
                    board->WakeFromSuperPowerSaveMode();
                }
                board->is_alarm_pre_wake_active_ = false;
                app.alarm_m_->ClearRing();
                board->AdjustAlarmCheckFrequency();
                return;
            }
            if (board->IsInSuperPowerSaveMode() && !board->is_alarm_pre_wake_active_) {
                auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
                if (next_alarm.has_value()) {
                    time_t alarm_time = next_alarm->time;
                    int seconds_to_alarm = (int)(alarm_time - now);
                    if (seconds_to_alarm > 0 && seconds_to_alarm <= 60) {
                        ESP_LOGI(TAG, "闹钟 '%s' 将在 %d 秒后触发，提前唤醒设备", 
                                 next_alarm->name.c_str(), seconds_to_alarm);
                        board->is_alarm_pre_wake_active_ = true;
                        board->WakeFromSuperPowerSaveMode();
                        auto display = board->GetDisplay();
                        if (display) {
                            char message[128];
                            snprintf(message, sizeof(message), 
                                    "闹钟 '%s' 即将响起\n设备提前唤醒准备中", 
                                    next_alarm->name.c_str());
                            display->SetChatMessage("system", message);
                        }
                    }
                }
            }
            if (!board->IsInSuperPowerSaveMode() && board->is_alarm_pre_wake_active_) {
                auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
                if (!next_alarm.has_value()) {
                    board->is_alarm_pre_wake_active_ = false;
                } else {
                    time_t alarm_time = next_alarm->time;
                    int seconds_to_alarm = (int)(alarm_time - now);
                    if (seconds_to_alarm <= 0 || seconds_to_alarm > 120) {
                        board->is_alarm_pre_wake_active_ = false;
                    }
                }
            }
            board->AdjustAlarmCheckFrequency();
        }, 2000, this);  
        AdjustAlarmCheckFrequency();
        ESP_LOGI(TAG, "智能动态闹钟监听器初始化完成");
#else
        ESP_LOGI(TAG, "闹钟功能未启用，跳过闹钟监听器初始化");
#endif
    }
    void EnterLightSleepMode() {
        ESP_LOGI(TAG, "进入浅睡眠模式 - 适度降低功耗");
        is_light_sleeping_ = true;
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            customDisplay->SetLightSleeping(true);
        }
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->SetBrightness(10);  
            ESP_LOGI(TAG, "屏幕亮度已降至10");
        }
        ESP_LOGI(TAG, "保持图片任务运行以确保时钟正常显示");
        SetPowerSaveMode(true);
        ESP_LOGI(TAG, "WiFi省电模式已启用");
        ESP_LOGI(TAG, "浅睡眠模式激活完成 - 时钟继续正常运行");
    }
    void ExitLightSleepMode() {
        ESP_LOGI(TAG, "退出浅睡眠模式 - 恢复正常功能");
        is_light_sleeping_ = false;
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            customDisplay->SetLightSleeping(false);
        }
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->RestoreBrightness();  
            ESP_LOGI(TAG, "屏幕亮度已恢复");
        }
        ESP_LOGI(TAG, "图片任务保持运行状态");
        SetPowerSaveMode(false);
        ESP_LOGI(TAG, "WiFi省电模式已禁用");
        ESP_LOGI(TAG, "浅睡眠模式退出完成");
    }
    void PauseUiTimers() {
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        if (!customDisplay) return;
        if (customDisplay->idle_timer_) {
            lv_timer_pause(customDisplay->idle_timer_);
            ESP_LOGI(TAG, "idle_timer已暂停");
        }
        if (customDisplay->sleep_timer_) {
            lv_timer_pause(customDisplay->sleep_timer_);
            ESP_LOGI(TAG, "sleep_timer已暂停");
        }
    }
    void ResumeUiTimers() {
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        if (!customDisplay) return;
        if (customDisplay->idle_timer_) {
            lv_timer_resume(customDisplay->idle_timer_);
            ESP_LOGI(TAG, "idle_timer已恢复");
        }
        if (customDisplay->sleep_timer_) {
            lv_timer_resume(customDisplay->sleep_timer_);
            ESP_LOGI(TAG, "sleep_timer已恢复");
        }
    }
    void EnterDeepSleepMode() {
        ESP_LOGI(TAG, "进入超级省电模式 - 检查闹钟状态");
        auto& app = Application::GetInstance();
        bool has_active_alarm = false;
#if CONFIG_USE_ALARM
        if (app.alarm_m_ != nullptr) {
            time_t now = time(NULL);
            auto next_alarm = app.alarm_m_->GetProximateAlarm(now);
            if (next_alarm.has_value()) {
                has_active_alarm = true;
                ESP_LOGI(TAG, "检测到活动闹钟 '%s'，将保留闹钟功能", next_alarm->name.c_str());
            }
        }
#endif
        if (power_save_timer_) {
            power_save_timer_->SetEnabled(false);
            ESP_LOGI(TAG, "省电定时器已停止，防止重复进入超级省电模式");
        }
        is_in_super_power_save_ = true;
        if (display_) {
            if (has_active_alarm) {
                display_->SetChatMessage("system", "进入超级省电模式\n闹钟功能保持活跃\n按键唤醒设备");
            } else {
                display_->SetChatMessage("system", "进入超级省电模式\n按键唤醒设备");
            }
            vTaskDelay(pdMS_TO_TICKS(3000));  
        }
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->tabview_) {
                DisplayLockGuard lock(display_);
                ESP_LOGI(TAG, "超级省电模式：切换到超级省电模式页面（tab3，纯黑背景）");
                lv_tabview_set_act(customDisplay->tabview_, 1, LV_ANIM_OFF);  
            }
        }
        SuspendImageTask();
        ESP_LOGI(TAG, "图片轮播任务已停止");
        if (rotation_check_timer_ != nullptr) {
            esp_timer_stop(rotation_check_timer_);
            esp_timer_delete(rotation_check_timer_);
            rotation_check_timer_ = nullptr;
            ESP_LOGI(TAG, "屏幕旋转检查定时器已停止并删除");
        }
        PauseUiTimers();
        if (!has_active_alarm) {
            app.PauseAudioProcessing();
            auto codec = GetAudioCodec();
            if (codec) {
                codec->EnableInput(false);
                codec->EnableOutput(false);
            }
            ESP_LOGI(TAG, "无活动闹钟，完全关闭音频系统");
        } else {
            auto codec = GetAudioCodec();
            if (codec) {
                codec->EnableInput(false);  
            }
            ESP_LOGI(TAG, "有活动闹钟，保留音频输出功能");
        }
        if (app.protocol_) {
            ESP_LOGI(TAG, "正在关闭MQTT协议连接...");
            app.protocol_->CloseAudioChannel();
            ESP_LOGI(TAG, "MQTT协议连接已关闭");
        }
        if (mqtt_music_handler_) {
            ESP_LOGI(TAG, "正在断开音乐播放器MQTT连接...");
            mqtt_music_handler_->Disconnect();
            ESP_LOGI(TAG, "音乐播放器MQTT连接已断开");
        }
        ESP_LOGI(TAG, "正在停止MQTT通知服务...");
        app.StopMqttNotifier();
        ESP_LOGI(TAG, "MQTT通知服务已停止");
        auto& wifi_station = WifiStation::GetInstance();
        if (wifi_station.IsConnected()) {
            wifi_station.Stop();
            ESP_LOGI(TAG, "WiFi已断开（闹钟触发时将重新连接）");
        } else {
            ESP_LOGI(TAG, "WiFi已经处于断开状态");
        }
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->SetBrightness(1);  
            ESP_LOGI(TAG, "屏幕亮度设置为1%%");
        }
        if (power_manager_) {
            power_manager_->StopTimer();
        }
        auto& app_timer = Application::GetInstance();
        app_timer.StopClockTimer();
        if (!has_active_alarm) {
            if (alarm_monitor_timer_) {
                lv_timer_pause(alarm_monitor_timer_);
                ESP_LOGI(TAG, "无活动闹钟，已暂停alarm_monitor_timer_");
            }
        }
        ESP_LOGI(TAG, "正在延长LVGL刷新间隔以降低功耗...");
        lv_timer_t* timer = lv_timer_get_next(NULL);
        int timer_count = 0;
        while (timer != NULL) {
            if (timer != alarm_monitor_timer_) {
                lv_timer_set_period(timer, 500);  
                ESP_LOGD(TAG, "LVGL定时器 %d: 设置为 500 ms", timer_count);
            }
            timer = lv_timer_get_next(timer);
            timer_count++;
        }
        ESP_LOGI(TAG, "已调整 %d 个LVGL定时器的刷新间隔", timer_count);
        esp_pm_config_t pm_config;
        if (!has_active_alarm) {
            pm_config.max_freq_mhz = 40;
            pm_config.min_freq_mhz = 40;
            pm_config.light_sleep_enable = true;  
            esp_pm_configure(&pm_config);
            ESP_LOGI(TAG, "CPU频率降至40MHz，轻睡眠已启用（无活动闹钟，最大化省电）");
        } else {
            pm_config.max_freq_mhz = 40;
            pm_config.min_freq_mhz = 40;
            pm_config.light_sleep_enable = false;  
            esp_pm_configure(&pm_config);
            ESP_LOGI(TAG, "CPU频率降至40MHz，轻睡眠已禁用（有活动闹钟，保证可靠性）");
        }
        ESP_LOGI(TAG, "超级省电模式激活完成 - 闹钟功能%s", 
                 has_active_alarm ? "保持活跃" : "已关闭");
    }
    void SuspendImageTask() {
        if (image_task_handle_ != nullptr) {
            vTaskSuspend(image_task_handle_);
            ESP_LOGI(TAG, "图片轮播任务已暂停");
        }
    }
    void ResumeImageTask() {
        if (image_task_handle_ != nullptr) {
            vTaskResume(image_task_handle_);
            ESP_LOGI(TAG, "图片轮播任务已恢复");
        }
    }
    void CheckImageResources() {
        ESP_LOGI(TAG, "图片资源检查任务开始执行");
        ESP_LOGI(TAG, "当前任务可用堆栈: %u 字节", uxTaskGetStackHighWaterMark(NULL));
        ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        auto& image_manager = ImageResourceManager::GetInstance();
        auto& wifi = WifiStation::GetInstance();
        int wifi_wait_count = 0;
        while (!wifi.IsConnected() && wifi_wait_count < 30) { 
            ESP_LOGI(TAG, "等待WiFi连接以检查图片资源... (%d/30)", wifi_wait_count + 1);
            vTaskDelay(pdMS_TO_TICKS(2000));  
            wifi_wait_count++;
        }
        if (!wifi.IsConnected()) {
            ESP_LOGE(TAG, "WiFi连接超时，图片资源检查任务退出");
            return;
        }
        ESP_LOGI(TAG, "WiFi已连接，立即开始资源检查...");
        ESP_LOGI(TAG, "取消并等待预加载完成...");
        image_manager.CancelPreload();
        image_manager.WaitForPreloadToFinish(1000);
        ESP_LOGI(TAG, "预加载处理完成");
        ESP_LOGI(TAG, "开始调用CheckAndUpdateAllResources");
        ESP_LOGI(TAG, "API_URL: %s", API_URL);
        ESP_LOGI(TAG, "VERSION_URL: %s", VERSION_URL);
        ESP_LOGI(TAG, "调用前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        esp_err_t all_resources_result = image_manager.CheckAndUpdateAllResources(API_URL, VERSION_URL);
        ESP_LOGI(TAG, "CheckAndUpdateAllResources调用完成，结果: %s (%d)", 
                esp_err_to_name(all_resources_result), all_resources_result);
        ESP_LOGI(TAG, "调用后可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
        bool has_updates = false;
        bool has_errors = false;
        if (all_resources_result == ESP_OK) {
            ESP_LOGI(TAG, "图片资源更新完成（一次API请求完成所有下载）");
            has_updates = true;
        } else if (all_resources_result == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "所有图片资源已是最新版本，无需更新");
        } else {
            ESP_LOGE(TAG, "图片资源检查/下载失败");
            has_errors = true;
        }
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "logo图片已设置");
        } else {
            ESP_LOGW(TAG, "未能获取logo图片，将使用默认显示");
        }
        ESP_LOGI(TAG, "开始检查时钟壁纸资源");
        bool wallpaper_updated = false;
        CustomLcdDisplay* custom_display = static_cast<CustomLcdDisplay*>(display_);
        if (custom_display) {
            if (custom_display->CheckAndDownloadWallpapers()) {
                ESP_LOGI(TAG, "壁纸资源已更新，重新加载壁纸");
                custom_display->LoadBackgroundImages();
                wallpaper_updated = true;  
            } else {
                ESP_LOGI(TAG, "壁纸资源无需更新");
            }
        }
        if (!has_updates && !wallpaper_updated && !has_errors) {
            auto& app = Application::GetInstance();
            DeviceState current_state = app.GetDeviceState();
            if (current_state == kDeviceStateStarting || current_state == kDeviceStateIdle) {
                ESP_LOGI(TAG, "所有资源检查完成，设备就绪，播放开机成功提示音（当前状态: %d）", current_state);
                ESP_LOGI(TAG, "调用SetDeviceState(kDeviceStateIdle)以启用空闲定时器");
                app.SetDeviceState(kDeviceStateIdle);
                app.PlaySound(Lang::Sounds::P3_SUCCESS);
            }
        }
        if ((has_updates || wallpaper_updated) && !has_errors) {
            if (has_updates && wallpaper_updated) {
                ESP_LOGI(TAG, "图片资源和壁纸都有更新，2秒后重启设备...");
            } else if (has_updates) {
                ESP_LOGI(TAG, "图片资源有更新，2秒后重启设备...");
            } else {
                ESP_LOGI(TAG, "壁纸资源有更新，2秒后重启设备...");
            }
            for (int i = 2; i > 0; i--) {
                ESP_LOGI(TAG, "将在 %d 秒后重启...", i);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            esp_restart();
        } else if (has_errors) {
            ESP_LOGW(TAG, "图片资源下载存在错误，设备继续运行但可能缺少部分图片");
        } else {
            ESP_LOGI(TAG, "所有资源（图片+壁纸）都是最新版本，无需重启");
        }
    }
    void StartImageSlideshow() {
        auto& image_manager = ImageResourceManager::GetInstance();
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
        image_manager.SetDownloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                int percent = (total > 0) ? (current * 100 / total) : 0;
                customDisplay->ShowDownloadProgress(true, percent, message);
            }
        });
        image_manager.SetPreloadProgressCallback([customDisplay](int current, int total, const char* message) {
            if (customDisplay) {
                customDisplay->UpdatePreloadProgressUI(message != nullptr, current, total, message);
            }
        });
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 8192, this, 1, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
        auto& app = Application::GetInstance();
        app.SetImageResourceCallback([this]() {
            ESP_LOGI(TAG, "OTA检查完成，开始检查图片资源");
            BaseType_t task_result = xTaskCreate([](void* param) {
                CustomBoard* board = static_cast<CustomBoard*>(param);
                board->CheckImageResources();
                vTaskDelete(NULL);
            }, "img_resource_check", 8192, this, 3, NULL);  
            if (task_result != pdPASS) {
                ESP_LOGE(TAG, "图片资源检查任务创建失败，错误码: %d", task_result);
                ESP_LOGI(TAG, "当前可用内存: %" PRIu32 " 字节", esp_get_free_heap_size());
            } else {
                ESP_LOGI(TAG, "图片资源检查任务创建成功");
            }
        });
    }
    template<typename T, typename... Args>
    static T* malloc_struct(Args... args) {
        T* result = (T*)malloc(sizeof(T));
        if (result) {
            *result = {args...};
        }
        return result;
    }
    static void ImageSlideshowTask(void* arg) {
        CustomBoard* board = static_cast<CustomBoard*>(arg);
        Display* display = board->GetDisplay();
        auto& app = Application::GetInstance();
        auto& image_manager = ImageResourceManager::GetInstance();
        ESP_LOGI(TAG, "🎬 图片播放任务启动 - 配置强力音频保护机制");
        const bool ENABLE_DYNAMIC_PRIORITY = true;   
        if (ENABLE_DYNAMIC_PRIORITY) {
            vTaskPrioritySet(NULL, 2); 
            ESP_LOGI(TAG, "💡 图片任务优先级已调整，音频任务享有更高优先权");
        }
        app.SetAudioPriorityMode(false); 
        ESP_LOGI(TAG, "🎯 智能音频保护已激活，图片播放将根据音频状态智能调节");
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display);
        int imgWidth = 240;
        int imgHeight = 240;
        lv_image_dsc_t img_dsc = {
            .header = {
                .magic = LV_IMAGE_HEADER_MAGIC,
                .cf = LV_COLOR_FORMAT_RGB565,        
                .flags = 0,
                .w = (uint32_t)imgWidth,             
                .h = (uint32_t)imgHeight,            
                .stride = (uint32_t)(imgWidth * 2),  
                .reserved_2 = 0,
            },
            .data_size = (uint32_t)(imgWidth * imgHeight * 2),  
            .data = NULL,  
            .reserved = NULL
        };
        lv_obj_t* img_container = nullptr;
        lv_obj_t* img_obj = nullptr;
        {
            DisplayLockGuard lock(display);
            img_container = lv_obj_create(customDisplay->tab1);
            lv_obj_remove_style_all(img_container);
            lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
            lv_obj_center(img_container);
            lv_obj_set_style_border_width(img_container, 0, 0);
            lv_obj_set_style_bg_opa(img_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(img_container, 0, 0);
            lv_obj_move_foreground(img_container);  
            img_obj = lv_img_create(img_container);
            lv_obj_center(img_obj);
            lv_obj_move_foreground(img_obj);
        }
        vTaskDelay(pdMS_TO_TICKS(100));  
        const uint8_t* logo = image_manager.GetLogoImage();
        if (logo) {
            iot::g_static_image = logo;
            ESP_LOGI(TAG, "已从资源管理器快速获取logo图片");
        } else {
            ESP_LOGW(TAG, "暂无logo图片，等待下载...");
        }
        ESP_LOGI(TAG, "预加载表情包资源...");
        iot::LoadAllEmoticons();
        int loaded_count = 0;
        for (int i = 0; i < 6; i++) {
            if (iot::g_emoticon_images[i] != nullptr) {
                loaded_count++;
            }
        }
        ESP_LOGI(TAG, "表情包预加载完成: %d/6", loaded_count);
        if (g_image_display_mode == iot::MODE_STATIC && g_static_image) {
            DisplayLockGuard lock(display);
            img_dsc.data = g_static_image;
            lv_img_set_src(img_obj, &img_dsc);
            ESP_LOGI(TAG, "开机立即显示logo图片");
        } else if (g_image_display_mode == iot::MODE_EMOTICON) {
            iot::EmotionType current_emotion = iot::g_current_emotion;
            if (current_emotion < iot::EMOTION_UNKNOWN && iot::g_emoticon_images[current_emotion]) {
                DisplayLockGuard lock(display);
                img_dsc.data = iot::g_emoticon_images[current_emotion];
                lv_img_set_src(img_obj, &img_dsc);
                ESP_LOGI(TAG, "开机立即显示表情包：表情类型 %d", current_emotion);
            } else {
                ESP_LOGW(TAG, "表情包数据无效: emotion=%d, valid=%d, ptr=%p", 
                    current_emotion,
                    (current_emotion < iot::EMOTION_UNKNOWN),
                    iot::g_emoticon_images[current_emotion]);
            }
        } else {
            const auto& imageArray = image_manager.GetImageArray();
            if (!imageArray.empty()) {
                const uint8_t* currentImage = imageArray[0];
                if (currentImage) {
                    DisplayLockGuard lock(display);
                    img_dsc.data = currentImage;
                    lv_img_set_src(img_obj, &img_dsc);
                    ESP_LOGI(TAG, "开机立即显示存储的图片");
                } else {
                    ESP_LOGW(TAG, "图片数据为空");
                }
            } else {
                ESP_LOGW(TAG, "图片数组为空");
            }
        }
        ESP_LOGI(TAG, "优化检查：快速检查预加载状态...");
        int preload_check_count = 0;
        while (preload_check_count < 50) { 
            bool isPreloadActive = false;
            if (customDisplay && customDisplay->preload_progress_container_ &&
                !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                isPreloadActive = true;
            }
            if (!isPreloadActive) {
                break; 
            }
            ESP_LOGI(TAG, "快速检查预加载状态... (%d/50)", preload_check_count + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            preload_check_count++;
        }
        if (preload_check_count >= 50) {
            ESP_LOGW(TAG, "预加载等待优化：超时后继续启动图片轮播");
        } else {
            ESP_LOGI(TAG, "预加载状态检查完成，快速启动图片轮播");
        }
        int currentIndex = 0;
        bool directionForward = true;  
        const uint8_t* currentImage = nullptr;
        bool lastWasStaticMode = false;
        const uint8_t* lastStaticImage = nullptr;
        TickType_t lastUpdateTime = xTaskGetTickCount();  
        const TickType_t cycleInterval = pdMS_TO_TICKS(150);  
        bool isAudioPlaying = false;       
        bool wasAudioPlaying = false;      
        DeviceState previousState = app.GetDeviceState();  
        bool pendingAnimationStart = false;  
        TickType_t stateChangeTime = 0;      
        while (true) {
            const auto& imageArray = image_manager.GetImageArray();
            if (imageArray.empty()) {
                static int wait_count = 0;
                wait_count++;
                if (wait_count <= 30) {  
                    ESP_LOGW(TAG, "图片资源未加载，优化等待策略... (%d/30)", wait_count);
                    vTaskDelay(pdMS_TO_TICKS(3000));  
                    continue;
                } else {
                    ESP_LOGE(TAG, "图片资源等待超时，显示黑屏");
                    DisplayLockGuard lock(display);
                    if (img_container) {
                        lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    }
                    vTaskDelay(pdMS_TO_TICKS(5000));  
                    wait_count = 0;  
                    continue;
                }
            }
            if (currentIndex >= imageArray.size()) {
                currentIndex = 0;
            }
            DeviceState currentState = app.GetDeviceState();
            TickType_t currentTime = xTaskGetTickCount();
            auto audioLevel = app.GetAudioActivityLevel();
            bool shouldPauseCompletely = false;
            TickType_t dynamicCycleInterval = cycleInterval; 
            switch (audioLevel) {
                case Application::AUDIO_IDLE:
                    dynamicCycleInterval = cycleInterval; 
                    shouldPauseCompletely = false;
                    break;
                case Application::AUDIO_STANDBY:
                    dynamicCycleInterval = pdMS_TO_TICKS(150); 
                    shouldPauseCompletely = false;
                    break;
                case Application::AUDIO_ACTIVE:
                    dynamicCycleInterval = pdMS_TO_TICKS(200); 
                    shouldPauseCompletely = false;
                    break;
                case Application::AUDIO_CRITICAL:
                    shouldPauseCompletely = true;
                    break;
            }
            bool shouldAnimate = isAudioPlaying && g_image_display_mode == iot::MODE_ANIMATED;
            if (shouldPauseCompletely && shouldAnimate) {
                static TickType_t lastLogTime = 0;
                TickType_t now = xTaskGetTickCount();
                if (now - lastLogTime > pdMS_TO_TICKS(5000)) { 
                    ESP_LOGI(TAG, "🔒 关键音频保护激活: 级别=%d, 完全暂停图片播放", audioLevel);
                    lastLogTime = now;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            {
                static Application::AudioActivityLevel lastLoggedLevel = Application::AUDIO_IDLE;
                static TickType_t lastLogTime = 0;
                TickType_t now = xTaskGetTickCount();
                if ((audioLevel != lastLoggedLevel || now - lastLogTime > pdMS_TO_TICKS(10000)) && shouldAnimate) {
                    const char* levelNames[] = {"空闲", "待机", "活跃", "关键"};
                    ESP_LOGI(TAG, "🎬 图片播放策略: 音频级别=%d(%s), 帧间隔=%dms", 
                            audioLevel, levelNames[audioLevel], (int)(dynamicCycleInterval * portTICK_PERIOD_MS));
                    lastLoggedLevel = audioLevel;
                    lastLogTime = now;
                }
            }
            bool isClockTabActive = false;
            {
                DisplayLockGuard lock(display);
                if (lock.IsLocked() && customDisplay && customDisplay->tabview_) {
                    int active_tab = lv_tabview_get_tab_act(customDisplay->tabview_);
                    isClockTabActive = (active_tab == 1);
                } else if (!lock.IsLocked()) {
                    ESP_LOGW(TAG, "无法获取显示锁以检查标签页状态");
                }
            }
            bool isPreloadUIVisible = false;
            {
                DisplayLockGuard lock(display);
                if (lock.IsLocked() && customDisplay && customDisplay->preload_progress_container_ &&
                    !lv_obj_has_flag(customDisplay->preload_progress_container_, LV_OBJ_FLAG_HIDDEN)) {
                    isPreloadUIVisible = true;
                } else if (!lock.IsLocked()) {
                    ESP_LOGW(TAG, "无法获取显示锁以检查预加载UI状态");
                }
            }
            if (isClockTabActive || isPreloadUIVisible) {
                {
                    DisplayLockGuard lock(display);
                    if (lock.IsLocked() && img_container) {
                        lv_obj_add_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                    } else if (!lock.IsLocked()) {
                        ESP_LOGW(TAG, "无法获取显示锁以隐藏图片容器");
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            } else {
                {
                    DisplayLockGuard lock(display);
                    if (lock.IsLocked() && img_container) {
                        lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
                        lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
                    } else if (!lock.IsLocked()) {
                        ESP_LOGW(TAG, "无法获取显示锁以显示图片容器");
                    }
                }
                {
                    DisplayLockGuard lock(display);
                    if (lock.IsLocked() && img_container) {
                        lv_obj_move_to_index(img_container, 0);
                        lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                        if (img_obj) {
                            lv_obj_center(img_obj);
                            lv_obj_move_foreground(img_obj);
                        }
                    } else if (!lock.IsLocked()) {
                        ESP_LOGW(TAG, "无法获取显示锁以处理图片层级");
                    }
                }
            }
            if (currentState == kDeviceStateSpeaking && previousState != kDeviceStateSpeaking) {
                if (g_image_display_mode == iot::MODE_ANIMATED) {
                    pendingAnimationStart = true;
                    stateChangeTime = currentTime;
                    directionForward = true;  
                    ESP_LOGI(TAG, "检测到音频状态改变，准备启动动画");
                } else {
                    ESP_LOGI(TAG, "检测到音频状态改变，当前为非动画模式，不启动动画");
                }
            }
            if (currentState != kDeviceStateSpeaking && isAudioPlaying) {
                isAudioPlaying = false;
                ESP_LOGI(TAG, "退出说话状态，停止动画");
            }
            if (pendingAnimationStart && g_image_display_mode == iot::MODE_ANIMATED && (currentTime - stateChangeTime >= pdMS_TO_TICKS(1200))) {
                currentIndex = 1;  
                directionForward = true;  
                if (currentIndex < imageArray.size()) {
                    int actual_image_index = currentIndex + 1;  
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画启动：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画启动：图片 %d 紧急加载失败，使用第一张图片", actual_image_index);
                            currentIndex = 0;  
                        }
                    } else {
                        ESP_LOGI(TAG, "动画启动：图片 %d 已预加载，开始流畅播放", actual_image_index);
                    }
                    currentImage = imageArray[currentIndex];
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        if (lock.IsLocked()) {
                            lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                            if (img_obj) {
                                img_dsc.data = currentImage;  
                                lv_img_set_src(img_obj, &img_dsc);
                            }
                        } else {
                            ESP_LOGW(TAG, "无法获取显示锁以启动动画");
                        }
                    }
                    if (currentImage) {
                        ESP_LOGI(TAG, "开始播放动画，与音频同步");
                    }
                    lastUpdateTime = currentTime;
                    isAudioPlaying = true;         
                    pendingAnimationStart = false; 
                }
            }
            if (shouldAnimate && !pendingAnimationStart && !shouldPauseCompletely && (currentTime - lastUpdateTime >= dynamicCycleInterval)) {
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
                if (currentIndex >= 0 && currentIndex < imageArray.size()) {
                    int actual_image_index = currentIndex + 1;  
                    if (!image_manager.IsImageLoaded(actual_image_index)) {
                        ESP_LOGW(TAG, "动画播放：图片 %d 未预加载，正在紧急加载...", actual_image_index);
                        if (!image_manager.LoadImageOnDemand(actual_image_index)) {
                            ESP_LOGE(TAG, "动画播放：图片 %d 紧急加载失败，跳过此帧", actual_image_index);
                            lastUpdateTime = currentTime;
                            continue;  
                        }
                    }
                    currentImage = imageArray[currentIndex];
                    if (currentImage) {
                        DisplayLockGuard lock(display);
                        if (lock.IsLocked()) {
                            lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                            if (img_obj) {
                                img_dsc.data = currentImage;  
                                lv_img_set_src(img_obj, &img_dsc);
                            }
                        } else {
                            ESP_LOGW(TAG, "无法获取显示锁以播放动画帧");
                        }
                    }
                }
                lastUpdateTime = currentTime;
            }
            else if ((!isAudioPlaying && wasAudioPlaying) || 
                     (g_image_display_mode == iot::MODE_STATIC) || 
                     (g_image_display_mode == iot::MODE_EMOTICON) ||
                     (!isAudioPlaying && currentIndex != 0)) {
                const uint8_t* staticImage = nullptr;
                bool isStaticMode = false;
                bool isEmoticonMode = false;
                if (g_image_display_mode == iot::MODE_STATIC && iot::g_static_image) {
                    staticImage = iot::g_static_image;
                    isStaticMode = true;
                } else if (g_image_display_mode == iot::MODE_EMOTICON) {
                    iot::EmotionType current_emotion = iot::g_current_emotion;
                    if (current_emotion < iot::EMOTION_UNKNOWN && iot::g_emoticon_images[current_emotion]) {
                        staticImage = iot::g_emoticon_images[current_emotion];
                        isEmoticonMode = true;
                    } else {
                        static TickType_t lastDiagTime = 0;
                        TickType_t now = xTaskGetTickCount();
                        if (now - lastDiagTime > pdMS_TO_TICKS(5000)) {
                            ESP_LOGW(TAG, "表情包无法显示: emotion=%d, valid=%d, ptr=%p", 
                                current_emotion, 
                                (current_emotion < iot::EMOTION_UNKNOWN),
                                iot::g_emoticon_images[current_emotion]);
                            lastDiagTime = now;
                        }
                    }
                } else if (!imageArray.empty()) {
                    currentIndex = 0;
                    staticImage = imageArray[currentIndex];
                    isStaticMode = false;
                }
                if (staticImage) {
                    DisplayLockGuard lock(display);
                    if (lock.IsLocked()) {
                        lv_obj_t* img_obj = lv_obj_get_child(img_container, 0);
                        if (img_obj) {
                            img_dsc.data = staticImage;  
                            lv_img_set_src(img_obj, &img_dsc);
                        }
                        if (isStaticMode != lastWasStaticMode || staticImage != lastStaticImage) {
                            const char* mode_name = isEmoticonMode ? "表情包" : (isStaticMode ? "logo" : "初始");
                            ESP_LOGI(TAG, "显示%s图片", mode_name);
                            lastWasStaticMode = isStaticMode;
                            lastStaticImage = staticImage;
                        }
                    } else {
                        ESP_LOGW(TAG, "无法获取显示锁以显示静态/表情包图片");
                    }
                    pendingAnimationStart = false;
                }
            }
            wasAudioPlaying = isAudioPlaying;
            previousState = currentState;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelete(NULL);
    }
public:
    void SafeWakeUpPowerSaveTimer() {
        if (power_save_timer_) {
            power_save_timer_->WakeUp();
            ESP_LOGI(TAG, "安全唤醒省电定时器");
        }
    }
    bool IsLightSleeping() const {
        return is_light_sleeping_;
    }
    bool IsInSuperPowerSaveMode() const {
        return is_in_super_power_save_;
    }
    bool IsWifiFullyConnected() {
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGD(TAG, "WiFi驱动未初始化");
            return false;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "WiFi状态检查失败: %s", esp_err_to_name(err));
            return false;
        }
        auto& wifi_station = WifiStation::GetInstance();
        bool is_connected = wifi_station.IsConnected();
        std::string ip = wifi_station.GetIpAddress();
        bool has_valid_ip = !ip.empty() && ip != "0.0.0.0";
        ESP_LOGD(TAG, "WiFi状态检查: 驱动已初始化=%s, 连接状态=%s, IP地址=%s", 
                 err == ESP_OK ? "是" : "否",
                 is_connected ? "已连接" : "未连接", 
                 ip.c_str());
        return is_connected && has_valid_ip;
    }
    void WakeFromSuperPowerSaveMode() {
        if (!is_in_super_power_save_) {
            return; 
        }
        ESP_LOGI(TAG, "从超级省电模式唤醒");
        is_in_super_power_save_ = false;
        ESP_LOGI(TAG, "正在恢复LVGL刷新间隔...");
        lv_timer_t* timer = lv_timer_get_next(NULL);
        int timer_count = 0;
        while (timer != NULL) {
            if (timer != alarm_monitor_timer_) {
                lv_timer_set_period(timer, 5);
                ESP_LOGD(TAG, "LVGL定时器 %d: 恢复为 5 ms", timer_count);
            }
            timer = lv_timer_get_next(timer);
            timer_count++;
        }
        ESP_LOGI(TAG, "已恢复 %d 个LVGL定时器的刷新间隔", timer_count);
        ResumeUiTimers();
        if (alarm_monitor_timer_) {
            lv_timer_resume(alarm_monitor_timer_);
            ESP_LOGI(TAG, "alarm_monitor_timer_已恢复");
            AdjustAlarmCheckFrequency();
        }
        esp_pm_config_t pm_config = {
            .max_freq_mhz = 160,
            .min_freq_mhz = 40,
            .light_sleep_enable = false,
        };
        esp_pm_configure(&pm_config);
        ESP_LOGI(TAG, "CPU频率已恢复到160MHz");
        if (power_manager_) {
            power_manager_->StartTimer();
        }
        auto& app_timer = Application::GetInstance();
        app_timer.StartClockTimer();
        auto backlight = GetBacklight();
        if (backlight) {
            backlight->RestoreBrightness();
            ESP_LOGI(TAG, "屏幕亮度已恢复");
        }
        auto& app = Application::GetInstance();
        app.ResumeAudioProcessing();
        auto codec = GetAudioCodec();
        if (codec) {
            codec->EnableInput(true);
            codec->EnableOutput(true);
            ESP_LOGI(TAG, "音频系统已恢复");
        }
        auto& wifi_station = WifiStation::GetInstance();
        ESP_LOGI(TAG, "正在重新初始化WiFi...");
        wifi_station.Start();
        int wifi_wait_count = 0;
        const int max_wifi_wait = 150; 
        ESP_LOGI(TAG, "等待WiFi完全初始化和连接...");
        while (!IsWifiFullyConnected() && wifi_wait_count < max_wifi_wait) {
            vTaskDelay(pdMS_TO_TICKS(100));  
            wifi_wait_count++;
            if (wifi_wait_count % 20 == 0) {
                ESP_LOGI(TAG, "等待WiFi连接... (%d/%d 秒)", wifi_wait_count/10, max_wifi_wait/10);
            }
        }
        if (IsWifiFullyConnected()) {
            ESP_LOGI(TAG, "WiFi完全连接成功，重新初始化MQTT连接...");
            ESP_LOGI(TAG, "重新启动MQTT通知服务...");
            app.StartMqttNotifier();
            if (mqtt_music_handler_) {
                ESP_LOGI(TAG, "重新连接音乐播放器MQTT...");
                if (mqtt_music_handler_->Connect()) {
                    ESP_LOGI(TAG, "音乐播放器MQTT重连成功");
                } else {
                    ESP_LOGW(TAG, "音乐播放器MQTT重连失败");
                }
            }
        } else {
            ESP_LOGW(TAG, "WiFi连接超时或驱动未完全初始化，跳过MQTT连接");
        }
        ResumeImageTask();
        ESP_LOGI(TAG, "图片轮播任务已恢复");
        if (power_save_timer_) {
            power_save_timer_->SetEnabled(true);
            power_save_timer_->WakeUp();
            ESP_LOGI(TAG, "省电定时器已重新启用");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  
        ESP_LOGI(TAG, "禁用WiFi省电模式");
        SetPowerSaveMode(false);
        if (display_) {
            CustomLcdDisplay* customDisplay = static_cast<CustomLcdDisplay*>(display_);
            if (customDisplay->tabview_) {
                DisplayLockGuard lock(display_);
                ESP_LOGI(TAG, "从超级省电模式唤醒：切换回主页面（tab1）");
                lv_tabview_set_act(customDisplay->tabview_, 0, LV_ANIM_OFF);  
            }
        }
        ESP_LOGI(TAG, "从超级省电模式完全恢复 - 闹钟触发唤醒完成");
    }
    CustomBoard() : boot_btn(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();        
        InitializeSpi();             
        InitializeLcdDisplay();      
        SetupMusicPlayerCallbacks();
        vTaskDelay(pdMS_TO_TICKS(1500));
        GetBacklight()->RestoreBrightness();
        InitializeButtons();         
        InitializeIot();             
        InitializeImageResources();  
        InitializePowerManager();    
        InitializePowerSaveTimer();  
        InitializeAlarmMonitor();
        InitializeMqttMusicHandler();
        ShowWelcomeMessage();
        OptimizeAudioSettings();
        StartImageSlideshow();
    }
    void ShowWelcomeMessage() {
        if (!display_) return;
        auto& wifi_station = WifiStation::GetInstance();
        if (!wifi_station.IsConnected()) {
            display_->SetChatMessage("system", Lang::Strings::WELCOME_MESSAGE_CONNECTING);
            display_->ShowNotification(Lang::Strings::PLEASE_CONFIGURE_NETWORK, 0);
        } else {
            display_->SetChatMessage("system", Lang::Strings::WELCOME_MESSAGE_INITIALIZING);
        }
    }
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);  
        return &audio_codec;
    }
    void OptimizeAudioSettings() {
        auto codec = GetAudioCodec();
        if (codec) {
            Settings settings("audio", false);
            int gain_int = settings.GetInt("input_gain", 48);  
            float custom_gain = static_cast<float>(gain_int);
            codec->SetInputGain(custom_gain);
            ESP_LOGI(TAG, "音频设置已优化：输入增益 %.1fdB", custom_gain);
        }
    }
    virtual Display* GetDisplay() override {
        return display_;  
    }
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);  
        return &backlight;
    }
    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (!power_manager_) {
            return false;  
        }
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        level = power_manager_->GetBatteryLevel();
        ESP_LOGD(TAG, "电池状态 - 电量: %d%%, 充电: %s, 放电: %s", 
                level, charging ? "是" : "否", discharging ? "是" : "否");
        return true;
    }
    virtual const char* GetNetworkStateIcon() override {
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            return FONT_AWESOME_WIFI_OFF;
        }
        auto& wifi_station = WifiStation::GetInstance();
        if (!wifi_station.IsConnected()) {
            return FONT_AWESOME_WIFI_OFF;  
        }
        return WifiBoard::GetNetworkStateIcon();
    }
    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled && power_save_timer_) {
            power_save_timer_->WakeUp();  
        }
        wifi_mode_t mode;
        esp_err_t err = esp_wifi_get_mode(&mode);
        if (err == ESP_ERR_WIFI_NOT_INIT) {
            ESP_LOGW(TAG, "WiFi驱动未初始化，跳过省电模式设置 (enabled=%s)", enabled ? "true" : "false");
            return;
        }
        auto& app = Application::GetInstance();
        DeviceState currentState = app.GetDeviceState();
        if (currentState == kDeviceStateIdle || currentState == kDeviceStateListening || 
            currentState == kDeviceStateConnecting || currentState == kDeviceStateSpeaking) {
            WifiBoard::SetPowerSaveMode(enabled);
        } else {
            ESP_LOGW(TAG, "设备未完全启动(状态:%d)，跳过WiFi省电模式设置", (int)currentState);
        }
    }
    ~CustomBoard() {
        if (image_task_handle_ != nullptr) {
            vTaskDelete(image_task_handle_);  
            image_task_handle_ = nullptr;
        }
        if (power_save_timer_ != nullptr) {
            delete power_save_timer_;
            power_save_timer_ = nullptr;
        }
        if (power_manager_ != nullptr) {
            delete power_manager_;
            power_manager_ = nullptr;
        }
        if (mqtt_music_handler_ != nullptr) {
            delete mqtt_music_handler_;
            mqtt_music_handler_ = nullptr;
        }
    }
};
#ifdef CONFIG_IMAGE_API_URL
const char* CustomBoard::API_URL = CONFIG_IMAGE_API_URL;
#else
const char* CustomBoard::API_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif
#ifdef CONFIG_IMAGE_VERSION_URL
const char* CustomBoard::VERSION_URL = CONFIG_IMAGE_VERSION_URL;
#else
const char* CustomBoard::VERSION_URL = "https://xiaoqiao-v2api.xmduzhong.com/app-api/xiaoqiao/system/skin";
#endif
void ResetPowerSaveTimer() {
    auto& board = Board::GetInstance();
    auto& custom_board = static_cast<CustomBoard&>(board);
    custom_board.SafeWakeUpPowerSaveTimer();
}
DECLARE_BOARD(CustomBoard);
