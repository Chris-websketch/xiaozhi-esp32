#include "music_player.h"
#include "ui/music_player_ui.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "MusicPlayerIoT";

namespace iot {

MusicPlayerThing::MusicPlayerThing() : Thing("MusicPlayer", "音乐播放器界面控制") {
    ESP_LOGI(TAG, "MusicPlayer IoT device created");
    
    // 注册Show方法
    std::vector<Parameter> show_params = {
        Parameter("duration_ms", "显示持续时间（毫秒）", kValueTypeNumber, false),
        Parameter("song_title", "歌曲标题", kValueTypeString, false),
        Parameter("artist_name", "艺术家名称", kValueTypeString, false)
    };
    methods_.AddMethod("Show", "显示音乐播放器界面", ParameterList(show_params), [this](const ParameterList& parameters) {
        HandleShowMethod(parameters);
    });
    
    // 注册Hide方法
    methods_.AddMethod("Hide", "隐藏音乐播放器界面", ParameterList(), [this](const ParameterList& parameters) {
        HandleHideMethod(parameters);
    });
    

}

void MusicPlayerThing::Invoke(const cJSON* command) {
    if (!command) {
        ESP_LOGE(TAG, "Invalid command");
        return;
    }

    const cJSON* method = cJSON_GetObjectItem(command, "method");
    if (!method || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing or invalid method");
        return;
    }

    const char* method_name = cJSON_GetStringValue(method);
    const cJSON* parameters = cJSON_GetObjectItem(command, "parameters");

    ESP_LOGI(TAG, "MusicPlayer Invoke: %s", method_name);

    if (strcmp(method_name, "Show") == 0) {
        HandleShowCommand(parameters);
    } else if (strcmp(method_name, "Hide") == 0) {
        HandleHideCommand(parameters);

    } else {
        ESP_LOGW(TAG, "Unknown method: %s", method_name);
    }
}

void MusicPlayerThing::HandleShowCommand(const cJSON* parameters) {
    ESP_LOGI(TAG, "HandleShowCommand called");
    
    if (!g_music_player_instance) {
        ESP_LOGE(TAG, "Music player UI not initialized");
        return;
    }

    uint32_t duration_ms = 30000;  // 默认30秒
    const char* song_title = "未知歌曲";
    const char* artist_name = "未知艺术家";

    if (parameters) {
        // 解析持续时间
        const cJSON* duration = cJSON_GetObjectItem(parameters, "duration_ms");
        if (duration && cJSON_IsNumber(duration)) {
            duration_ms = (uint32_t)cJSON_GetNumberValue(duration);
        }

        // 解析歌曲标题
        const cJSON* title = cJSON_GetObjectItem(parameters, "song_title");
        if (title && cJSON_IsString(title)) {
            song_title = cJSON_GetStringValue(title);
        }

        // 解析艺术家名称
        const cJSON* artist = cJSON_GetObjectItem(parameters, "artist_name");
        if (artist && cJSON_IsString(artist)) {
            artist_name = cJSON_GetStringValue(artist);
        }


    }

    // 设置歌曲信息并显示
    g_music_player_instance->SetSongInfo(song_title, artist_name);
    music_player_error_t ret = g_music_player_instance->Show(duration_ms);
    
    if (ret == MUSIC_PLAYER_OK) {
        ESP_LOGI(TAG, "Music player shown: %s - %s (duration: %lu ms)", song_title, artist_name, duration_ms);
    } else {
        ESP_LOGE(TAG, "Failed to show music player: %d", ret);
    }
}

void MusicPlayerThing::HandleHideCommand(const cJSON* parameters) {
    ESP_LOGI(TAG, "HandleHideCommand called");
    
    if (!g_music_player_instance) {
        ESP_LOGE(TAG, "Music player UI not initialized");
        return;
    }

    music_player_error_t ret = g_music_player_instance->Hide();
    if (ret == MUSIC_PLAYER_OK) {
        ESP_LOGI(TAG, "Music player hidden");
    } else {
        ESP_LOGE(TAG, "Failed to hide music player: %d", ret);
    }
}



void MusicPlayerThing::HandleShowMethod(const ParameterList& parameters) {
    ESP_LOGI(TAG, "HandleShowMethod called");
    
    if (!g_music_player_instance) {
        ESP_LOGE(TAG, "Music player UI not initialized");
        return;
    }

    uint32_t duration_ms = 30000;  // 默认30秒
    const char* song_title = "未知歌曲";
    const char* artist_name = "未知艺术家";

    // 解析参数 - 使用 try-catch 来处理可选参数
    try {
        duration_ms = (uint32_t)parameters["duration_ms"].number();
    } catch (const std::exception& e) {
        // 参数不存在，使用默认值
        ESP_LOGD(TAG, "duration_ms not provided, using default: %lu", duration_ms);
    }
    
    try {
        song_title = parameters["song_title"].string().c_str();
    } catch (const std::exception& e) {
        ESP_LOGD(TAG, "song_title not provided, using default");
    }
    
    try {
        artist_name = parameters["artist_name"].string().c_str();
    } catch (const std::exception& e) {
        ESP_LOGD(TAG, "artist_name not provided, using default");
    }



    // 设置歌曲信息
    g_music_player_instance->SetSongInfo(song_title, artist_name);
    
    // 在UI操作前让出CPU时间片，避免阻塞主循环
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // 显示音乐播放器
    music_player_error_t ret = g_music_player_instance->Show(duration_ms);
    
    if (ret == MUSIC_PLAYER_OK) {
        ESP_LOGI(TAG, "Music player shown: %s - %s (duration: %lu ms)", song_title, artist_name, duration_ms);
    } else {
        ESP_LOGE(TAG, "Failed to show music player: %d", ret);
    }
}

void MusicPlayerThing::HandleHideMethod(const ParameterList& parameters) {
    ESP_LOGI(TAG, "HandleHideMethod called");
    
    if (!g_music_player_instance) {
        ESP_LOGE(TAG, "Music player UI not initialized");
        return;
    }

    music_player_error_t ret = g_music_player_instance->Hide();
    if (ret == MUSIC_PLAYER_OK) {
        ESP_LOGI(TAG, "Music player hidden");
    } else {
        ESP_LOGE(TAG, "Failed to hide music player: %d", ret);
    }
}



bool MusicPlayerThing::HandleShow(const cJSON* parameters) {
        if (!g_music_player_instance) {
            ESP_LOGE(TAG, "Music player UI not initialized");
            return false;
        }

        uint32_t duration_ms = 30000;  // 默认30秒
        const char* song_title = "未知歌曲";
        const char* artist_name = "未知艺术家";

        if (parameters) {
            // 解析持续时间
            const cJSON* duration = cJSON_GetObjectItem(parameters, "duration_ms");
            if (duration && cJSON_IsNumber(duration)) {
                duration_ms = (uint32_t)cJSON_GetNumberValue(duration);
            }

            // 解析歌曲标题
            const cJSON* title = cJSON_GetObjectItem(parameters, "song_title");
            if (title && cJSON_IsString(title)) {
                song_title = cJSON_GetStringValue(title);
            }

            // 解析艺术家名称
            const cJSON* artist = cJSON_GetObjectItem(parameters, "artist_name");
            if (artist && cJSON_IsString(artist)) {
                artist_name = cJSON_GetStringValue(artist);
            }


        }

        // 设置歌曲信息
        music_player_error_t ret = g_music_player_instance->SetSongInfo(song_title, artist_name);
        if (ret != MUSIC_PLAYER_OK) {
            ESP_LOGE(TAG, "Failed to set song info: %d", ret);
            return false;
        }

        // 显示音乐播放器
        ret = g_music_player_instance->Show(duration_ms);
        if (ret != MUSIC_PLAYER_OK) {
            ESP_LOGE(TAG, "Failed to show music player: %d", ret);
            return false;
        }

        ESP_LOGI(TAG, "Music player shown: %s - %s (duration: %lu ms)", song_title, artist_name, duration_ms);
        return true;
    }

bool MusicPlayerThing::HandleHide(const cJSON* parameters) {
        if (!g_music_player_instance) {
            ESP_LOGE(TAG, "Music player UI not initialized");
            return false;
        }

        music_player_error_t ret = g_music_player_instance->Hide();
        if (ret != MUSIC_PLAYER_OK) {
            ESP_LOGE(TAG, "Failed to hide music player: %d", ret);
            return false;
        }

        ESP_LOGI(TAG, "Music player hidden");
        return true;
    }



} // namespace iot
