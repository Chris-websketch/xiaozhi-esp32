#include "application.h"
#include "board.h"
#include "display.h"
#include "system_info.h"
#include "ml307_ssl_transport.h"
#include "audio_codec.h"
#include "mqtt_protocol.h"
#include "websocket_protocol.h"
#include "font_awesome_symbols.h"
#include "iot/thing_manager.h"
#include "assets/lang_config.h"
#include "notifications/mqtt_notifier.h"
#include "memory/memory_manager.h"
#include "config/resource_config.h"
#include "settings.h"
#include "boards/moon/iot_image_display.h"

#include <cstring>
#include <cmath>
#include <algorithm>
#include <esp_log.h>
#include <cJSON.h>
#include <driver/gpio.h>
#include <arpa/inet.h>
#include <wifi_station.h>

#define TAG "Application"

using ImageResource::MemoryManager;
using ImageResource::ImageBufferPool;
using ImageResource::ConfigManager;
using ImageResource::ResourceConfig;

/**
 * @brief 从emotion字符串映射到表情类型枚举
 * @param emotion_str emotion字符串（如"happy", "sad"等）
 * @return 对应的EmotionType枚举值
 */
static iot::EmotionType ParseEmotionString(const char* emotion_str) {
    if (emotion_str == nullptr) {
        return iot::EMOTION_CALM;
    }
    
    // 转换为小写进行比较（简单处理）
    std::string emotion_lower = emotion_str;
    std::transform(emotion_lower.begin(), emotion_lower.end(), emotion_lower.begin(), ::tolower);
    
    if (emotion_lower == "happy" || emotion_lower == "happiness" || emotion_lower == "joy") {
        return iot::EMOTION_HAPPY;
    } else if (emotion_lower == "sad" || emotion_lower == "sadness" || emotion_lower == "sorrow") {
        return iot::EMOTION_SAD;
    } else if (emotion_lower == "angry" || emotion_lower == "anger" || emotion_lower == "rage") {
        return iot::EMOTION_ANGRY;
    } else if (emotion_lower == "surprised" || emotion_lower == "surprise" || emotion_lower == "amazed") {
        return iot::EMOTION_SURPRISED;
    } else if (emotion_lower == "calm" || emotion_lower == "neutral" || emotion_lower == "normal") {
        return iot::EMOTION_CALM;
    } else if (emotion_lower == "shy" || emotion_lower == "bashful" || emotion_lower == "embarrassed") {
        return iot::EMOTION_SHY;
    }
    
    // 未识别，返回平静
    return iot::EMOTION_CALM;
}

/**
 * @brief 从文本中解析emoji并映射到表情类型
 * @param text AI响应文本
 * @return 对应的EmotionType枚举值
 */
static iot::EmotionType ParseEmojiFromText(const char* text) {
    if (text == nullptr || text[0] == '\0') {
        return iot::EMOTION_UNKNOWN;  // 空文本，不改变当前表情
    }
    
    // 在整个文本中搜索UTF-8 emoji序列（4字节编码）
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(text);
    size_t len = strlen(text);
    
    // 遍历文本，查找emoji
    for (size_t i = 0; i + 3 < len; i++) {
        // 所有目标emoji都以 0xF0 0x9F 开头
        if (bytes[i] != 0xF0 || bytes[i+1] != 0x9F) {
            continue;
        }
        
        // 检查第三和第四字节以识别具体emoji
        if (bytes[i+2] == 0x98) {
            switch (bytes[i+3]) {
                case 0x84: return iot::EMOTION_HAPPY;      // 😄 U+1F604
                case 0x86: return iot::EMOTION_HAPPY;      // 😆 U+1F606 大笑
                case 0x81: return iot::EMOTION_HAPPY;      // 😁 U+1F601 露齿笑
                case 0x8A: return iot::EMOTION_HAPPY;      // 😊 U+1F60A 微笑
                case 0x82: return iot::EMOTION_HAPPY;      // 😂 U+1F602 笑哭
                case 0xA2: return iot::EMOTION_SAD;        // 😢 U+1F622 哭泣
                case 0xAD: return iot::EMOTION_SAD;        // 😭 U+1F62D 大哭
                case 0x94: return iot::EMOTION_SAD;        // 😔 U+1F614 沉思
                case 0xA0: return iot::EMOTION_ANGRY;      // 😠 U+1F620 生气
                case 0xA1: return iot::EMOTION_ANGRY;      // 😡 U+1F621 愤怒
                case 0xA4: return iot::EMOTION_ANGRY;      // 😤 U+1F624 得意
                case 0xB2: return iot::EMOTION_SURPRISED;  // 😲 U+1F632 惊讶
                case 0xAE: return iot::EMOTION_SURPRISED;  // 😮 U+1F62E 张嘴
                case 0xB3: return iot::EMOTION_SHY;        // 😳 U+1F633 脸红/害羞
                case 0x90: return iot::EMOTION_CALM;       // 😐 U+1F610 平静
                case 0x91: return iot::EMOTION_CALM;       // 😑 U+1F611 无语
                case 0x92: return iot::EMOTION_CALM;       // 😒 U+1F612 不悦
                default: break;
            }
        }
    }
    
    // 未找到emoji，返回EMOTION_UNKNOWN表示"不改变当前表情"
    return iot::EMOTION_UNKNOWN;
}

static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"
};

Application::Application() {
    event_group_ = xEventGroupCreate();
    background_task_ = new BackgroundTask(4096 * 8);

#if CONFIG_USE_ALARM
    // 初始化闹钟预处理标志
    alarm_pre_processing_active_ = false;
    
    // 初始化闹钟前奏音频相关标志
    alarm_prelude_playing_ = false;
    alarm_prelude_start_time_ = 0;
    pending_alarm_name_.clear();
#endif

    esp_timer_create_args_t clock_timer_args = {
        .callback = [](void* arg) {
            Application* app = (Application*)arg;
            app->OnClockTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };
    esp_timer_create(&clock_timer_args, &clock_timer_handle_);
}

Application::~Application() {
    if (clock_timer_handle_ != nullptr) {
        esp_timer_stop(clock_timer_handle_);
        esp_timer_delete(clock_timer_handle_);
    }
    if (background_task_ != nullptr) {
        delete background_task_;
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion() {
    // 获取设备地理位置信息
    ESP_LOGI(TAG, "获取设备地理位置信息...");
    GeoLocationInfo location = SystemInfo::GetCountryInfo();
    if (location.is_valid) {
        ESP_LOGI(TAG, "设备地理位置: IP=%s, 国家=%s", 
                 location.ip_address.c_str(), location.country_code.c_str());
        if (!location.country_name.empty()) {
            ESP_LOGI(TAG, "国家名称: %s", location.country_name.c_str());
        }
    } else {
        ESP_LOGW(TAG, "无法获取地理位置信息");
    }
    
    // 获取配置管理器实例
    const auto& config = ConfigManager::GetInstance().get_config();
    int retry_count = 0;

    while (true) {
        auto display = Board::GetInstance().GetDisplay();
        if (!ota_.CheckVersion()) {
            retry_count++;
            if (retry_count >= static_cast<int>(config.network.retry_count)) {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                // 即使OTA检查失败，也标记为完成，让图片资源检查可以继续
                ota_check_completed_ = true;
                if (image_resource_callback_) {
                    Schedule(image_resource_callback_);
                }
                return;
            }
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%lu)", 60, retry_count, config.network.retry_count);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        retry_count = 0;

        if (ota_.HasNewVersion()) {
            // 根据是语言更新还是版本更新，显示不同的提示
            const char* upgrade_status = ota_.IsLanguageUpdate() ? 
                Lang::Strings::LANGUAGE_SWITCHING : Lang::Strings::UPGRADING;
            
            Alert(Lang::Strings::OTA_UPGRADE, upgrade_status, "happy", Lang::Sounds::P3_UPGRADE);
            //等待提示音播放完毕后再开始升级
            vTaskDelay(pdMS_TO_TICKS(3000));

            // 使用主要任务进行升级，无法取消。
            Schedule([this, display]() {
                SetDeviceState(kDeviceStateUpgrading);
                
                display->SetIcon(FONT_AWESOME_DOWNLOAD);
                // 根据更新类型显示不同的消息
                std::string message;
                if (ota_.IsLanguageUpdate()) {
                    message = Lang::Strings::LANGUAGE_SWITCHING;
                } else {
                    message = std::string(Lang::Strings::NEW_VERSION) + ota_.GetFirmwareVersion();
                }
                display->SetChatMessage("system", message.c_str());

                auto& board = Board::GetInstance();
                board.SetPowerSaveMode(false);
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
                // 预先关闭音频输出，避免升级过程有音频操作
                auto codec = board.GetAudioCodec();
                codec->EnableInput(false);
                codec->EnableOutput(false);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    audio_decode_queue_.clear();
                }
                background_task_->WaitForCompletion();
                delete background_task_;
                background_task_ = nullptr;
                vTaskDelay(pdMS_TO_TICKS(1000));

                ota_.StartUpgrade([display](int progress, size_t speed) {
                    char buffer[64];
                    snprintf(buffer, sizeof(buffer), "%d%% %zuKB/s", progress, speed / 1024);
                    display->SetChatMessage("system", buffer);
                });

                // If upgrade success, the device will reboot and never reach here
                display->SetStatus(Lang::Strings::UPGRADE_FAILED);
                ESP_LOGI(TAG, "Firmware upgrade failed...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                Reboot();
            });

            return;
        }

        // No new version, mark the current version as valid
        ota_.MarkCurrentVersionValid();
        std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        display->ShowNotification(message.c_str());
    
        if (ota_.HasActivationCode()) {
            // Activation code is valid
            SetDeviceState(kDeviceStateActivating);
            ShowActivationCode();

            // Check again in 60 seconds or until the device is idle
            for (int i = 0; i < 60; ++i) {
                if (device_state_ == kDeviceStateIdle) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            continue;
        }

        // OTA检查完成，保持Starting状态，等待图片资源检查完成后再切换到Idle
        // 注意：不能在这里设置为Idle，否则后续SetDeviceState(kDeviceStateIdle)会因状态相同而跳过
        display->SetChatMessage("system", "");
        ResetDecoder();
        // 开机成功提示音已移至资源检查完成后播放（仅在无需更新时播放）
        
        // OTA检查完成后，状态栏显示"登录服务器中"（等待资源检查完成）
        display->SetStatus(Lang::Strings::LOGGING_IN_SERVER);
        
        // OTA检查完成，标记为完成状态
        ESP_LOGI(TAG, "OTA check completed, triggering image resource check");
        ota_check_completed_ = true;
        // OTA 更新可能刷新了 MQTT 配置，尝试通知组件重连
        if (notifier_) {
            notifier_->ReconnectIfSettingsChanged();
        }
        if (image_resource_callback_) {
            Schedule(image_resource_callback_);
        }
        
        // Exit the loop if upgrade or idle
        break;
    }
}

void Application::ShowActivationCode() {
    auto& message = ota_.GetActivationMessage();
    auto& code = ota_.GetActivationCode();

    struct digit_sound {
        char digit;
        const std::string_view& sound;
    };
    static const std::array<digit_sound, 10> digit_sounds{{
        digit_sound{'0', Lang::Sounds::P3_0},
        digit_sound{'1', Lang::Sounds::P3_1}, 
        digit_sound{'2', Lang::Sounds::P3_2},
        digit_sound{'3', Lang::Sounds::P3_3},
        digit_sound{'4', Lang::Sounds::P3_4},
        digit_sound{'5', Lang::Sounds::P3_5},
        digit_sound{'6', Lang::Sounds::P3_6},
        digit_sound{'7', Lang::Sounds::P3_7},
        digit_sound{'8', Lang::Sounds::P3_8},
        digit_sound{'9', Lang::Sounds::P3_9}
    }};

    // 组合消息和激活码一起显示
    std::string full_message = message;
    if (!code.empty()) {
        full_message += "\n" + code;
    }

    // This sentence uses 9KB of SRAM, so we need to wait for it to finish
    Alert(Lang::Strings::ACTIVATION, full_message.c_str(), "happy", Lang::Sounds::P3_ACTIVATION);
    vTaskDelay(pdMS_TO_TICKS(1000));
    background_task_->WaitForCompletion();

    for (const auto& digit : code) {
        auto it = std::find_if(digit_sounds.begin(), digit_sounds.end(),
            [digit](const digit_sound& ds) { return ds.digit == digit; });
        if (it != digit_sounds.end()) {
            PlaySound(it->sound);
        }
    }
}

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        ResetDecoder();
        PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus(Lang::Strings::STANDBY);
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::PlaySound(const std::string_view& sound) {
    // The assets are encoded at 16000Hz, 60ms frame duration
    SetDecodeSampleRate(16000, 60);
    const char* data = sound.data();
    size_t size = sound.size();
    
    // 添加详细的调试信息
    ESP_LOGI(TAG, "PlaySound: 开始播放音频 - 数据大小:%zu字节, 数据指针:%p", size, data);
    
    if (size == 0) {
        ESP_LOGE(TAG, "PlaySound: 音频数据为空，无法播放");
        return;
    }
    
    if (data == nullptr) {
        ESP_LOGE(TAG, "PlaySound: 音频数据指针为空，无法播放");
        return;
    }
    
    // 确保音频输出启用
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec && !codec->output_enabled()) {
        ESP_LOGI(TAG, "PlaySound: 音频输出未启用，正在启用...");
        codec->EnableOutput(true);
    }
    
    int packet_count = 0;
    for (const char* p = data; p < data + size; ) {
        auto p3 = (BinaryProtocol3*)p;
        p += sizeof(BinaryProtocol3);

        auto payload_size = ntohs(p3->payload_size);
        std::vector<uint8_t> opus;
        opus.resize(payload_size);
        memcpy(opus.data(), p3->payload, payload_size);
        p += payload_size;
        packet_count++;

        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(opus));
    }
    
    ESP_LOGI(TAG, "PlaySound: 音频处理完成 - 总共%d个数据包已添加到播放队列", packet_count);
}

void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);
            // Reset timeout invalidation flag when attempting new connection
            protocol_invalidated_by_timeout_ = false;
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            // 添加按键唤醒消息，让服务器知道这是一次新对话开始
            ESP_LOGI(TAG, "按键唤醒，发送唤醒消息给服务器");
            last_button_wake_time_ = std::chrono::steady_clock::now();  // 记录按键唤醒时间
            protocol_->SendWakeWordDetected("button");

            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {
        // 快速停止：先切到 idle，并在后台优雅关闭通道，避免前台卡顿且减少后续断开噪声
        Schedule([this]() {
            StopListeningFast(true);
        });
    }
}

void Application::StartListening(bool skip_wake_message) {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }
    
    if (device_state_ == kDeviceStateIdle) {
        Schedule([this, skip_wake_message]() {
            bool was_channel_closed = !protocol_->IsAudioChannelOpened();
            if (was_channel_closed) {
                SetDeviceState(kDeviceStateConnecting);
                // Reset timeout invalidation flag when attempting new connection
                protocol_invalidated_by_timeout_ = false;
                if (!protocol_->OpenAudioChannel()) {
                    return;
                }
                
                // 如果是首次打开音频通道且未要求跳过，发送按键唤醒消息
                if (!skip_wake_message) {
                    ESP_LOGI(TAG, "按住说话首次连接，发送唤醒消息给服务器");
                    last_button_wake_time_ = std::chrono::steady_clock::now();  // 记录按键唤醒时间
                    protocol_->SendWakeWordDetected("button");
                } else {
                    ESP_LOGI(TAG, "按住对话模式，跳过唤醒消息");
                }
            }

            SetListeningMode(kListeningModeManualStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
            SetListeningMode(kListeningModeManualStop);
        });
    }
}

void Application::StopListening() {
    Schedule([this]() {
        if (device_state_ == kDeviceStateListening) {
            protocol_->SendStopListening();
            SetDeviceState(kDeviceStateIdle);
        }
    });
}

// 立即结束监听，先切 UI 再异步关闭连接，避免优雅关闭的同步等待带来的卡顿
void Application::StopListeningFast(bool close_channel_after) {
    // 仅在 Listening 状态下执行快速停止
    if (device_state_ == kDeviceStateListening) {
        // 先通知服务端停止监听，但不等待
        protocol_->SendStopListening();
        // 立即切换到 Idle，保证 UI 立刻反馈
        SetDeviceState(kDeviceStateIdle);

        if (close_channel_after) {
            // 可选：后台关闭通道，避免阻塞主线程
            background_task_->Schedule([this]() {
                if (protocol_) {
                    protocol_->CloseAudioChannel();
                }
            });
        }
    }
}

void Application::Start() {
    auto& board = Board::GetInstance();
    SetDeviceState(kDeviceStateStarting);
    
    // 🔍 启动时内存状态报告
    ESP_LOGI(TAG, "=== 🚀 应用启动 - 初始内存状态 ===");
    MemoryManager::GetInstance().log_memory_status();
    ImageBufferPool::GetInstance().log_pool_status();

    /* Setup the display */
    auto display = board.GetDisplay();

    /* Setup the audio codec */
    auto codec = board.GetAudioCodec();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
    opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
    
    // 音频系统初始化完成，现在可以安全启动Display定时器
    ESP_LOGI(TAG, "音频系统初始化完成，启动Display定时器");
    display->StartUpdateTimer();
    // 性能优化：强制使用最快编码速度
    ESP_LOGI(TAG, "Performance optimization: setting opus encoder complexity to 0 (fastest)");
    opus_encoder_->SetComplexity(0);
    
    // 启用实时模式相关优化
    realtime_chat_enabled_ = false;
    ESP_LOGI(TAG, "Using auto stop mode instead of realtime mode");

    if (codec->input_sample_rate() != 16000) {
        input_resampler_.Configure(codec->input_sample_rate(), 16000);
        reference_resampler_.Configure(codec->input_sample_rate(), 16000);
    }
    codec->Start();

    // 性能优化：音频任务固定绑定到Core 1，使用最高优先级
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 9, &audio_loop_task_handle_, 1);  // 优先级9，绑定Core 1

    /* Start the main loop */
    xTaskCreatePinnedToCore([](void* arg) {
        Application* app = (Application*)arg;
        app->MainLoop();
        vTaskDelete(NULL);
    }, "main_loop", 4096 * 2, this, 4, &main_loop_task_handle_, 0);

    /* Wait for the network to be ready */
    board.StartNetwork();

    // Initialize the protocol
    display->SetStatus(Lang::Strings::LOADING_PROTOCOL);
#ifdef CONFIG_CONNECTION_TYPE_WEBSOCKET
    ESP_LOGI(TAG, "🔧 使用WebSocket协议");
    protocol_ = std::make_unique<WebsocketProtocol>();
#else
    ESP_LOGI(TAG, "🔧 使用MQTT+UDP协议");
    protocol_ = std::make_unique<MqttProtocol>();
#endif
    protocol_->OnNetworkError([this](const std::string& message) {
        SetDeviceState(kDeviceStateIdle);
        Alert(Lang::Strings::ERROR, message.c_str(), "sad", Lang::Sounds::P3_EXCLAMATION);
    });
    protocol_->OnIncomingAudio([this](std::vector<uint8_t>&& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.emplace_back(std::move(data));
    });
    protocol_->OnAudioChannelOpened([this, codec, &board]() {
        board.SetPowerSaveMode(false);
        
        // 调试信息：显示服务器音频参数
        ESP_LOGI(TAG, "🔗 音频通道已打开 - 服务器参数: [采样率:%d, 帧长度:%dms], 客户端发送帧长度:%dms", 
                 protocol_->server_sample_rate(), protocol_->server_frame_duration(), OPUS_FRAME_DURATION_MS);
        
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        auto& thing_manager = iot::ThingManager::GetInstance();
        protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        std::string states;
        thing_manager.GetStatesJson(states, false);
        if (!states.empty() && states != "[]") {
            protocol_->SendIotStates(states);
        }
        
        // WebSocket握手成功后，自动进入listening状态并发送listen start消息
        ESP_LOGI(TAG, "WebSocket握手成功，自动开始新对话");
        SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
    });
    protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
    protocol_->OnIncomingJson([this, display](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    // 优化：减少背景任务等待时间，加快状态切换速度
                    // background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            // 优化：立即切换到listening状态，减少音频数据丢失
                            ESP_LOGI(TAG, "TTS结束，快速切换到listening状态");
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    
                    // 如果处于表情包模式，提取emoji并更新当前表情
                    // 注意：只有当文本中真的包含emoji时才更新，否则保持LLM设置的emotion
                    if (iot::g_image_display_mode == iot::MODE_EMOTICON) {
                        iot::EmotionType emotion = ParseEmojiFromText(text->valuestring);
                        
                        // 只有找到了真正的emoji（不是EMOTION_UNKNOWN），才更新当前表情
                        if (emotion != iot::EMOTION_UNKNOWN) {
                            iot::g_current_emotion = emotion;
                            ESP_LOGI(TAG, "表情包模式：从文本中检测到emoji，情绪类型 %d", emotion);
                        }
                        // 如果文本中没有emoji，保持之前LLM设置的emotion不变
                    }
                    
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                    
                    // 尝试检测多个意图，支持同时调节
                    auto intent_results = local_intent_detector_.DetectMultipleIntents(message.c_str());
                    if (!intent_results.empty()) {
                        ESP_LOGI(TAG, "本地检测到 %zu 个意图，跳过云端LLM处理", intent_results.size());
                        
                        // 依次执行所有检测到的意图
                        for (const auto& result : intent_results) {
                            ESP_LOGI(TAG, "执行意图: %s.%s (置信度: %.2f)", 
                                     result.device_name.c_str(), result.action.c_str(), result.confidence);
                            ExecuteLocalIntent(result);
                        }
                    } else {
                        ESP_LOGD(TAG, "本地意图检测未匹配，继续云端LLM处理");
                        // 本地检测失败，继续发送给云端LLM（保持原有逻辑）
                    }
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                // 如果处于表情包模式，解析emotion字符串并更新当前表情
                if (iot::g_image_display_mode == iot::MODE_EMOTICON) {
                    iot::EmotionType emotion_type = ParseEmotionString(emotion->valuestring);
                    iot::g_current_emotion = emotion_type;
                    ESP_LOGI(TAG, "表情包模式：LLM返回emotion=\"%s\", 映射到情绪类型 %d", 
                             emotion->valuestring, emotion_type);
                }
                
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    // 只有当设备不在说话状态时，才更新表情（旧方法，已禁用）
                    if (device_state_ != kDeviceStateSpeaking) {
                        display->SetEmotion(emotion_str.c_str());
                    }
                });
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
                auto& thing_manager = iot::ThingManager::GetInstance();
                for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                    auto command = cJSON_GetArrayItem(commands, i);
                    thing_manager.Invoke(command);
                }
            }
        }
    });
    protocol_->Start();

    // 启动 MQTT 通知组件（如果配置可用），仅用于服务端主动推送消息
    notifier_ = std::make_unique<MqttNotifier>();
    notifier_->OnMessage([this](const cJSON* root){
        OnMqttNotification(root);
    });
    notifier_->Start();

    // Check for new firmware version or get the MQTT broker address
    xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->CheckNewVersion();
        vTaskDelete(NULL);
    }, "check_new_version", 4096 * 2, this, 2, nullptr);

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_.Initialize(codec, realtime_chat_enabled_);
    audio_processor_.OnOutput([this](std::vector<int16_t>&& data) {
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
    });
    audio_processor_.OnVadStateChange([this](bool speaking) {
        if (device_state_ == kDeviceStateListening) {
            Schedule([this, speaking]() {
                if (speaking) {
                    voice_detected_ = true;
                } else {
                    voice_detected_ = false;
                }
                // LED功能已移除
            });
        }
    });
#endif

    // 初始化本地意图检测器
    local_intent_detector_.Initialize();
    local_intent_detector_.OnIntentDetected([this](const intent::IntentResult& result) {
        ESP_LOGI(TAG, "本地检测到意图: %s.%s (置信度: %.2f)", 
                 result.device_name.c_str(), result.action.c_str(), result.confidence);
        
        // 在主线程中执行意图
        Schedule([this, result]() {
            ExecuteLocalIntent(result);
        });
    });

#if CONFIG_USE_WAKE_WORD_DETECT
    wake_word_detect_.Initialize(codec);
    wake_word_detect_.OnWakeWordDetected([this](const std::string& wake_word) {
        Schedule([this, &wake_word]() {
            if (device_state_ == kDeviceStateIdle) {
                SetDeviceState(kDeviceStateConnecting);
                wake_word_detect_.EncodeWakeWordData();

                // Reset timeout invalidation flag when attempting new connection
                protocol_invalidated_by_timeout_ = false;
                if (!protocol_->OpenAudioChannel()) {
                    wake_word_detect_.StartDetection();
                    return;
                }
                
                std::vector<uint8_t> opus;
                // Encode and send the wake word data to the server
                while (wake_word_detect_.GetWakeWordOpus(opus)) {
                    protocol_->SendAudio(opus);
                }
                // Set the chat state to wake word detected
                protocol_->SendWakeWordDetected(wake_word);
                ESP_LOGI(TAG, "Wake word detected: %s", wake_word.c_str());
                SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
            } else if (device_state_ == kDeviceStateSpeaking) {
                AbortSpeaking(kAbortReasonWakeWordDetected);
            } else if (device_state_ == kDeviceStateActivating) {
                SetDeviceState(kDeviceStateIdle);
            }
        });
    });
    wake_word_detect_.StartDetection();
#endif

    // 初始化完成，设置设备状态为starting，等待图片资源检查完成后才切换到idle
    // 这样SetDeviceState(kDeviceStateIdle)才能正常执行状态切换逻辑（显示"待命"、启用空闲定时器）
    device_state_ = kDeviceStateStarting;
    esp_timer_start_periodic(clock_timer_handle_, 1000000);

#if 0
    while (true) {
        SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
#endif

#if CONFIG_USE_ALARM
    while(!ota_.HasServerTime()){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    alarm_m_ = new AlarmManager();
    // alarm_m_->SetAlarm(10, "alarm1");
#endif
}

void Application::OnMqttNotification(const cJSON* root) {
    auto type = cJSON_GetObjectItem(root, "type");
    if (!type || !cJSON_IsString(type)) {
        ESP_LOGW(TAG, "MQTT notify: missing type");
        return;
    }
    // 可选请求ID，用于ACK回传关联
    const cJSON* request_id_item = cJSON_GetObjectItem(root, "request_id");
    auto add_request_id_if_any = [request_id_item](cJSON* obj){
        if (!request_id_item) return;
        if (cJSON_IsString(request_id_item)) {
            cJSON_AddStringToObject(obj, "request_id", request_id_item->valuestring);
        } else if (cJSON_IsNumber(request_id_item)) {
            cJSON_AddNumberToObject(obj, "request_id", request_id_item->valuedouble);
        } else if (cJSON_IsBool(request_id_item)) {
            cJSON_AddBoolToObject(obj, "request_id", cJSON_IsTrue(request_id_item));
        }
    };
    // 系统控制：通过MQTT触发设备动作（如重启）
    if (strcmp(type->valuestring, "system") == 0) {
        auto action = cJSON_GetObjectItem(root, "action");
        if (action && cJSON_IsString(action)) {
            if (strcmp(action->valuestring, "reboot") == 0) {
                int delay_ms = 1000;
                auto delay_item = cJSON_GetObjectItem(root, "delay_ms");
                if (delay_item && cJSON_IsNumber(delay_item)) {
                    delay_ms = delay_item->valueint;
                    if (delay_ms < 0) delay_ms = 0;
                    if (delay_ms > 10000) delay_ms = 10000; // 保护：最大10秒
                }
                ESP_LOGW(TAG, "MQTT system action: reboot in %d ms", delay_ms);
                // 上报ACK（执行结果：ok），随后再调度重启
                if (notifier_) {
                    cJSON* ack = cJSON_CreateObject();
                    cJSON_AddStringToObject(ack, "type", "ack");
                    cJSON_AddStringToObject(ack, "target", "system");
                    cJSON_AddStringToObject(ack, "action", "reboot");
                    cJSON_AddStringToObject(ack, "status", "ok");
                    cJSON_AddNumberToObject(ack, "delay_ms", delay_ms);
                    add_request_id_if_any(ack);
                    notifier_->PublishAck(ack);
                    cJSON_Delete(ack);
                }
                auto display = Board::GetInstance().GetDisplay();
                if (display) {
                    display->ShowNotification(Lang::Strings::DEVICE_REBOOT_SOON, 1000);
                }
                Schedule([this, delay_ms]() {
                    vTaskDelay(pdMS_TO_TICKS(delay_ms));
                    Reboot();
                });
                return;
            }
            // 未支持的system action：返回错误ACK
            if (notifier_) {
                cJSON* ack = cJSON_CreateObject();
                cJSON_AddStringToObject(ack, "type", "ack");
                cJSON_AddStringToObject(ack, "target", "system");
                cJSON_AddStringToObject(ack, "status", "error");
                cJSON_AddStringToObject(ack, "error", "unsupported action");
                add_request_id_if_any(ack);
                notifier_->PublishAck(ack);
                cJSON_Delete(ack);
            }
        }
        return;
    }
    if (strcmp(type->valuestring, "notify") == 0) {
        auto title = cJSON_GetObjectItem(root, "title");
        auto body = cJSON_GetObjectItem(root, "body");
        std::string message;
        if (title && cJSON_IsString(title)) {
            message += title->valuestring;
        }
        if (body && cJSON_IsString(body)) {
            if (!message.empty()) message += "\n";
            message += body->valuestring;
        }
        if (!message.empty()) {
            auto display = Board::GetInstance().GetDisplay();
            display->ShowCenterNotification(message.c_str(), 10000);
            if (notifier_) {
                cJSON* ack = cJSON_CreateObject();
                cJSON_AddStringToObject(ack, "type", "ack");
                cJSON_AddStringToObject(ack, "target", "notify");
                cJSON_AddStringToObject(ack, "status", "ok");
                add_request_id_if_any(ack);
                notifier_->PublishAck(ack);
                cJSON_Delete(ack);
            }
        } else {
            if (notifier_) {
                cJSON* ack = cJSON_CreateObject();
                cJSON_AddStringToObject(ack, "type", "ack");
                cJSON_AddStringToObject(ack, "target", "notify");
                cJSON_AddStringToObject(ack, "status", "error");
                cJSON_AddStringToObject(ack, "error", "empty notification");
                add_request_id_if_any(ack);
                notifier_->PublishAck(ack);
                cJSON_Delete(ack);
            }
        }
        return;
    }
    if (strcmp(type->valuestring, "iot") == 0) {
        auto commands = cJSON_GetObjectItem(root, "commands");
        if (commands != NULL) {
            // 逐条命令在主线程同步执行，执行完成后回传ACK
            for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
                const cJSON* command = cJSON_GetArrayItem(commands, i);
                // 序列化command以跨线程传递
                char* cmd_json = cJSON_PrintUnformatted(command);
                // 从命令级读取可选request_id（优先命令级，其次顶层）
                std::string req_id_str;
                const cJSON* cmd_req = cJSON_GetObjectItem(command, "request_id");
                if (cmd_req && cJSON_IsString(cmd_req)) req_id_str = cmd_req->valuestring;
                if (req_id_str.empty() && request_id_item && cJSON_IsString(request_id_item)) req_id_str = request_id_item->valuestring;
                Schedule([this, cmd_json, req_id_str]() {
                    if (!cmd_json) return;
                    cJSON* cmd = cJSON_Parse(cmd_json);
                    cJSON_free(cmd_json);
                    if (!cmd) return;
                    std::string error;
                    bool ok = iot::ThingManager::GetInstance().InvokeSync(cmd, &error);
                    // 构建ACK
                    if (notifier_) {
                        cJSON* ack = cJSON_CreateObject();
                        cJSON_AddStringToObject(ack, "type", "ack");
                        cJSON_AddStringToObject(ack, "target", "iot");
                        cJSON_AddStringToObject(ack, "status", ok ? "ok" : "error");
                        // 附带原始命令
                        cJSON_AddItemToObject(ack, "command", cJSON_Duplicate(cmd, 1));
                        if (!ok) {
                            cJSON_AddStringToObject(ack, "error", error.c_str());
                        }
                        // 附带最新IoT状态（可选）
                        std::string states_json;
                        iot::ThingManager::GetInstance().GetStatesJson(states_json, false);
                        if (!states_json.empty() && states_json != "[]") {
                            cJSON* states = cJSON_Parse(states_json.c_str());
                            if (states) {
                                cJSON_AddItemToObject(ack, "states", states);
                            }
                        }
                        // 追加request_id（若存在）
                        if (!req_id_str.empty()) {
                            cJSON_AddStringToObject(ack, "request_id", req_id_str.c_str());
                        }
                        notifier_->PublishAck(ack);
                        cJSON_Delete(ack);
                    }
                    cJSON_Delete(cmd);
                });
            }
        }
        return;
    }
}

void Application::OnClockTimer() {
    clock_ticks_++;

    // Check for protocol timeout every second by monitoring channel state changes
    // Only check when device is in active states, avoid checking when already idle
    static bool was_channel_opened_last_check = false;
    if (protocol_ && device_state_ != kDeviceStateIdle) {
        bool is_channel_opened = protocol_->IsAudioChannelOpened();
        
        // If channel was opened before but now it's closed due to timeout (and not by user action)
        if (was_channel_opened_last_check && !is_channel_opened && 
            (device_state_ == kDeviceStateConnecting || device_state_ == kDeviceStateListening || device_state_ == kDeviceStateSpeaking) &&
            !timeout_handling_active_) {
            ESP_LOGW(TAG, "Protocol timeout detected (channel state changed), scheduling timeout handling");
            Schedule([this]() {
                HandleProtocolTimeout();
            });
        }
        
        was_channel_opened_last_check = is_channel_opened;
    } else if (device_state_ == kDeviceStateIdle) {
        // Reset the check when device becomes idle to prevent false positives on next connection
        was_channel_opened_last_check = false;
    }

    // 详细内存监控每30秒
    if (clock_ticks_ % 30 == 0) {
        // 原始简单监控（保留兼容性）
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

        // 获取当前内存状态和缓冲区池状态
        auto current_memory_status = MemoryManager::GetInstance().get_memory_status();
        auto& pool = ImageBufferPool::GetInstance();
        float current_pool_utilization = pool.get_pool_utilization_percent();
        bool current_pool_pressure = pool.is_pool_under_pressure();
        
        // 检查内存状态是否发生变化
        bool memory_status_changed = (current_memory_status != last_memory_status_);
        
        // 检查缓冲区池状态是否发生显著变化（使用率变化超过10%或压力状态改变）
        bool pool_status_changed = (std::abs(current_pool_utilization - last_pool_utilization_) > 10.0f) ||
                                  (current_pool_pressure != last_pool_pressure_state_);
        
        // 仅在状态发生变化时打印详细日志
        if (memory_status_changed) {
            ESP_LOGI(TAG, "内存状态发生变化: %s -> %s", 
                    (last_memory_status_ == ImageResource::MemoryStatus::GOOD ? "正常" : 
                     last_memory_status_ == ImageResource::MemoryStatus::WARNING ? "警告" : "危险"),
                    (current_memory_status == ImageResource::MemoryStatus::GOOD ? "正常" : 
                     current_memory_status == ImageResource::MemoryStatus::WARNING ? "警告" : "危险"));
            MemoryManager::GetInstance().log_memory_status();
            last_memory_status_ = current_memory_status;
        }
        
        if (pool_status_changed) {
            ESP_LOGI(TAG, "缓冲区池状态发生变化: 使用率 %.1f%% -> %.1f%%, 压力状态 %s -> %s",
                    last_pool_utilization_, current_pool_utilization,
                    last_pool_pressure_state_ ? "高压力" : "正常",
                    current_pool_pressure ? "高压力" : "正常");
            pool.log_pool_status();
            last_pool_utilization_ = current_pool_utilization;
            last_pool_pressure_state_ = current_pool_pressure;
        }
        
        // 多级内存状态检测（仅在状态变化时输出警告）
        if (memory_status_changed) {
            if (current_memory_status == ImageResource::MemoryStatus::CRITICAL) {
                ESP_LOGW(TAG, "🆘 内存处于危险状态，建议释放资源！");
            } else if (current_memory_status == ImageResource::MemoryStatus::WARNING) {
                ESP_LOGW(TAG, "⚠️  内存接近警告水平，请注意内存使用");
            }
        }

        // If we have synchronized server time, set the status to clock "HH:MM" if the device is idle
        // 调试：显示状态栏更新条件
        static int status_debug_counter = 0;
        status_debug_counter++;
        if (status_debug_counter % 100 == 0) { // 每100次循环打印一次
            const char* state_names[] = {"Unknown", "Starting", "WifiConfiguring", "Idle", "Connecting", "Listening", "Speaking", "Upgrading", "Activating", "FatalError"};
            const char* state_name = ((int)device_state_ < 10) ? state_names[(int)device_state_] : "InvalidState";
            ESP_LOGI(TAG, "Status bar update check: HasServerTime=%s, DeviceState=%s(%d)", 
                     ota_.HasServerTime() ? "true" : "false", state_name, (int)device_state_);
        }
        
        if (ota_.HasServerTime()) {
            if (device_state_ == kDeviceStateIdle) {
                Schedule([this]() {
                    // Set status to clock "HH:MM" with timezone conversion
                    time_t now = time(NULL);
                    struct tm timeinfo;
                    localtime_r(&now, &timeinfo);
                    
                    // 获取地理位置信息并进行时区转换（静态缓存避免重复调用）
                    static GeoLocationInfo status_location_cache;
                    static bool status_location_initialized = false;
                    
                    // 调试：显示当前状态
                    static int debug_counter = 0;
                    debug_counter++;
                    if (debug_counter % 30 == 0) { // 每30次调用打印一次状态
                        ESP_LOGI(TAG, "Status bar debug: initialized=%s, valid=%s, WiFi=%s", 
                                 status_location_initialized ? "true" : "false",
                                 status_location_cache.is_valid ? "true" : "false", 
                                 WifiStation::GetInstance().IsConnected() ? "true" : "false");
                    }
                    
                    // 只在WiFi连接成功且未初始化时获取地理位置
                    if (!status_location_initialized && WifiStation::GetInstance().IsConnected()) {
                        ESP_LOGI(TAG, "Status bar: Getting geolocation for timezone conversion...");
                        status_location_cache = SystemInfo::GetCountryInfo();
                        if (status_location_cache.is_valid) {
                            status_location_initialized = true;
                            ESP_LOGI(TAG, "Status bar timezone initialized for country %s (UTC%+d)", 
                                     status_location_cache.country_code.c_str(), status_location_cache.timezone_offset);
                        } else {
                            ESP_LOGW(TAG, "Status bar: Failed to get valid geolocation info");
                        }
                    }
                    
                    // 如果获取到有效的地理位置信息，进行时区转换
                    if (status_location_cache.is_valid && status_location_cache.timezone_offset != 8) {
                        struct tm original_time = timeinfo;
                        timeinfo = SystemInfo::ConvertFromBeijingTime(timeinfo, status_location_cache.timezone_offset);
                        ESP_LOGI(TAG, "Status bar time converted: %02d:%02d -> %02d:%02d (UTC%+d)", 
                                 original_time.tm_hour, original_time.tm_min,
                                 timeinfo.tm_hour, timeinfo.tm_min, 
                                 status_location_cache.timezone_offset);
                    }
                    
                    char time_str[64];
                    strftime(time_str, sizeof(time_str), "%H:%M  ", &timeinfo);
                    Board::GetInstance().GetDisplay()->SetStatus(time_str);
                });
            }
        }
    }
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & SCHEDULE_EVENT) {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

// The Audio Loop is used to input and output audio data
void Application::AudioLoop() {
    auto codec = Board::GetInstance().GetAudioCodec();
    while (true) {
        OnAudioInput();
        if (codec->output_enabled()) {
            OnAudioOutput();
        }
#if CONFIG_USE_ALARM
        if(alarm_m_ != nullptr){
            // 检查是否有闹钟即将在5秒内触发，根据设备状态进行相应的预处理
            if(!alarm_pre_processing_active_ &&
               (device_state_ == kDeviceStateListening || device_state_ == kDeviceStateSpeaking || device_state_ == kDeviceStateIdle)){
                time_t now = time(NULL);
                auto next_alarm = alarm_m_->GetProximateAlarm(now);
                if(next_alarm.has_value()){
                    int seconds_to_alarm = (int)(next_alarm->time - now);
                    if(seconds_to_alarm > 0 && seconds_to_alarm <= 5){
                        const char* state_name = (device_state_ == kDeviceStateListening) ? "聆听" : 
                                                (device_state_ == kDeviceStateSpeaking) ? "说话" : "空闲";
                        ESP_LOGI(TAG, "闹钟 '%s' 将在 %d 秒内触发，当前设备状态：%s",
                                 next_alarm->name.c_str(), seconds_to_alarm, state_name);

                        // 设置预处理标志，避免重复处理
                        alarm_pre_processing_active_ = true;
                        
                        // 如果还没有播放闹钟前奏音频，根据当前状态采用不同的预处理策略
                        if (!alarm_prelude_playing_) {
                            alarm_prelude_playing_ = true;
                            alarm_prelude_start_time_ = time(NULL);
                            pending_alarm_name_ = next_alarm->name;

                            // 根据当前状态采用不同的预处理策略
                            if(device_state_ == kDeviceStateSpeaking){
                                ESP_LOGI(TAG, "开始闹钟预处理：先中断TTS播放，再播放前奏音频");

                                // 说话状态：先中断TTS播放，然后播放前奏音频
                                Schedule([this]() {
                                    AbortSpeaking(kAbortReasonNone);
                                    ESP_LOGI(TAG, "TTS已中断，播放闹钟前奏音频");
                                    
                                    // 确保音频输出已启用
                                    auto codec = Board::GetInstance().GetAudioCodec();
                                    if (codec && !codec->output_enabled()) {
                                        ESP_LOGI(TAG, "音频输出已禁用，为播放闹钟前奏音频重新启用");
                                        codec->EnableOutput(true);
                                    }
                                    
                                    // 播放闹钟前奏音频（使用专用闹钟音频）
                                    PlaySound(Lang::Sounds::P3_ALARMCLOCK3S);
                                });

                            } else if(device_state_ == kDeviceStateListening) {
                                ESP_LOGI(TAG, "开始闹钟预处理：先停止音频录制，再播放前奏音频");

                                // 聆听状态：先丢弃待处理的音频数据，关闭音频通道，切换到待命状态，再播放前奏音频
                                Schedule([this]() {
                                    // 先丢弃所有待处理的音频数据
                                    DiscardPendingAudioForAlarm();

                                    // 然后关闭音频通道
                                    if(protocol_->IsAudioChannelOpened()){
                                        protocol_->CloseAudioChannel();
                                    }

                                    // 切换到待命状态
                                    SetDeviceState(kDeviceStateIdle);
                                    ESP_LOGI(TAG, "为即将触发的闹钟准备：音频数据已丢弃，设备已切换到待命状态，播放闹钟前奏音频");
                                    
                                    // 确保音频输出已启用
                                    auto codec = Board::GetInstance().GetAudioCodec();
                                    if (codec && !codec->output_enabled()) {
                                        ESP_LOGI(TAG, "音频输出已禁用，为播放闹钟前奏音频重新启用");
                                        codec->EnableOutput(true);
                                    }
                                    
                                    // 播放闹钟前奏音频（使用专用闹钟音频）
                                    PlaySound(Lang::Sounds::P3_ALARMCLOCK3S);
                                });
                                
                            } else if(device_state_ == kDeviceStateIdle) {
                                ESP_LOGI(TAG, "开始闹钟预处理：设备空闲，直接播放前奏音频");

                                // 空闲状态：直接播放前奏音频
                                Schedule([this]() {
                                    ESP_LOGI(TAG, "设备空闲状态，播放闹钟前奏音频");
                                    
                                    // 确保音频输出已启用（处理浅睡眠状态下音频输出被禁用的情况）
                                    auto codec = Board::GetInstance().GetAudioCodec();
                                    if (codec && !codec->output_enabled()) {
                                        ESP_LOGI(TAG, "音频输出已禁用，为播放闹钟前奏音频重新启用");
                                        codec->EnableOutput(true);
                                    }
                                    
                                    // 更新最后输出时间，防止音频输出被自动禁用
                                    last_output_time_ = std::chrono::steady_clock::now();
                                    
                                    // 播放闹钟前奏音频（使用专用闹钟音频，如果不可用则使用备用音频）
                                    if (Lang::Sounds::P3_ALARMCLOCK3S.size() > 0) {
                                        ESP_LOGI(TAG, "播放专用闹钟前奏音频 P3_ALARMCLOCK3S (大小:%zu字节)", Lang::Sounds::P3_ALARMCLOCK3S.size());
                                        PlaySound(Lang::Sounds::P3_ALARMCLOCK3S);
                                    } else {
                                        ESP_LOGW(TAG, "专用闹钟音频文件为空或不可用，使用备用音频 P3_EXCLAMATION");
                                        PlaySound(Lang::Sounds::P3_EXCLAMATION);
                                    }
                                });
                            }
                        }
                    }
                }
            }

            // 闹钟来了
            if(alarm_m_->IsRing()){
                // 重置预处理标志
                alarm_pre_processing_active_ = false;
                
                // 重置闹钟前奏音频相关标志
                if (alarm_prelude_playing_) {
                    ESP_LOGI(TAG, "闹钟 '%s' 触发，闹钟前奏音频播放完成", pending_alarm_name_.c_str());
                    alarm_prelude_playing_ = false;
                    alarm_prelude_start_time_ = 0;
                    pending_alarm_name_.clear();
                }

                if(device_state_ != kDeviceStateListening){
                    if (device_state_ == kDeviceStateActivating) {
                        Reboot();
                        return;
                    }
                    if (!protocol_->IsAudioChannelOpened()) {
                        SetDeviceState(kDeviceStateConnecting);
                        // Reset timeout invalidation flag when attempting new connection
                        protocol_invalidated_by_timeout_ = false;
                        if (!protocol_->OpenAudioChannel()) {
                            SetDeviceState(kDeviceStateIdle);
                            return;
                        }
                    }
                    // protocol_->SendStartListening(kListeningModeManualStop);
                    SetDeviceState(kDeviceStateListening);
                    ESP_LOGI(TAG, "Alarm ring, begging status %d", device_state_);
                }
                // 检查闹钟消息是否为空，避免发送空payload给服务端
                std::string alarm_message = alarm_m_->get_now_alarm_name();
                if (!alarm_message.empty()) {
                    protocol_->SendText(alarm_message);
                } else {
                    ESP_LOGW(TAG, "闹钟消息为空，跳过发送以避免服务端日志洪水");
                }
                alarm_m_->ClearRing();
            }
        }
#endif
    }
}

void Application::OnAudioOutput() {
    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        if (device_state_ == kDeviceStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            
#if CONFIG_USE_ALARM
            // 如果正在播放闹钟前奏音频，不要禁用音频输出
            if (alarm_prelude_playing_) {
                ESP_LOGD(TAG, "正在播放闹钟前奏音频，保持音频输出启用");
                return;
            }
#endif
            
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();
        return;
    }

    auto opus = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();

    background_task_->Schedule([this, codec, opus = std::move(opus)]() mutable {
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!opus_decoder_->Decode(std::move(opus), pcm)) {
            ESP_LOGE(TAG, "🔥 Opus解码失败 - 数据包大小:%zu bytes, 解码器配置: [采样率:%d, 帧长度:%dms]", 
                     opus.size(), opus_decoder_->sample_rate(), opus_decoder_->duration_ms());
            return;
        }
        // Resample if the sample rate is different
        if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
            int target_size = output_resampler_.GetOutputSamples(pcm.size());
            std::vector<int16_t> resampled(target_size);
            output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
            pcm = std::move(resampled);
        }
        codec->OutputData(pcm);
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

void Application::OnAudioInput() {
    std::vector<int16_t> data;

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        ReadAudio(data, 16000, wake_word_detect_.GetFeedSize());
        wake_word_detect_.Feed(data);
        return;
    }
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        ReadAudio(data, 16000, audio_processor_.GetFeedSize());
        audio_processor_.Feed(data);
        return;
    }
#else
    if (device_state_ == kDeviceStateListening) {
        ReadAudio(data, 16000, 30 * 16000 / 1000);
        background_task_->Schedule([this, data = std::move(data)]() mutable {
            opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
                Schedule([this, opus = std::move(opus)]() {
                    protocol_->SendAudio(opus);
                });
            });
        });
        return;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(30));
}

void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec->input_sample_rate() != sample_rate) {
        data.resize(samples * codec->input_sample_rate() / sample_rate);
        if (!codec->InputData(data)) {
            return;
        }
        if (codec->input_channels() == 2) {
            auto mic_channel = std::vector<int16_t>(data.size() / 2);
            auto reference_channel = std::vector<int16_t>(data.size() / 2);
            for (size_t i = 0, j = 0; i < mic_channel.size(); ++i, j += 2) {
                mic_channel[i] = data[j];
                reference_channel[i] = data[j + 1];
            }
            auto resampled_mic = std::vector<int16_t>(input_resampler_.GetOutputSamples(mic_channel.size()));
            auto resampled_reference = std::vector<int16_t>(reference_resampler_.GetOutputSamples(reference_channel.size()));
            input_resampler_.Process(mic_channel.data(), mic_channel.size(), resampled_mic.data());
            reference_resampler_.Process(reference_channel.data(), reference_channel.size(), resampled_reference.data());
            data.resize(resampled_mic.size() + resampled_reference.size());
            for (size_t i = 0, j = 0; i < resampled_mic.size(); ++i, j += 2) {
                data[j] = resampled_mic[i];
                data[j + 1] = resampled_reference[i];
            }
        } else {
            auto resampled = std::vector<int16_t>(input_resampler_.GetOutputSamples(data.size()));
            input_resampler_.Process(data.data(), data.size(), resampled.data());
            data = std::move(resampled);
        }
    } else {
        data.resize(samples);
        if (!codec->InputData(data)) {
            return;
        }
    }
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking - 原因: %d", reason);
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
    
    // 优化：用户中断时立即切换到listening状态，提高响应速度
    if (reason == kAbortReasonNone || reason == kAbortReasonWakeWordDetected) {
        Schedule([this]() {
            if (device_state_ == kDeviceStateSpeaking) {
                ESP_LOGI(TAG, "用户中断，立即切换到listening状态");
                SetDeviceState(kDeviceStateListening);
            }
        });
    }
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }

    clock_ticks_ = 0;
    auto previous_state = device_state_;
    device_state_ = state;

#if CONFIG_USE_ALARM
    // 当设备状态发生变化时，重置闹钟预处理标志
    // 除非是从聆听状态切换到待命状态（这可能是闹钟预处理导致的）
    if(!(previous_state == kDeviceStateListening && state == kDeviceStateIdle)){
        alarm_pre_processing_active_ = false;
        
        // 如果不是在闹钟预处理期间，并且状态切换到非激活状态，重置闹钟前奏标志
        if (!alarm_pre_processing_active_ && state == kDeviceStateIdle && alarm_prelude_playing_) {
            ESP_LOGI(TAG, "设备状态切换到空闲，重置闹钟前奏标志");
            alarm_prelude_playing_ = false;
            alarm_prelude_start_time_ = 0;
            pending_alarm_name_.clear();
        }
    }
#endif
    
    // 添加详细的状态切换日志，特别关注listening相关的切换
    if (state == kDeviceStateListening || previous_state == kDeviceStateListening ||
        state == kDeviceStateSpeaking || previous_state == kDeviceStateSpeaking) {
        ESP_LOGI(TAG, "🔄 STATE CHANGE: %s -> %s (listening_mode: %d)", 
                 STATE_STRINGS[previous_state], STATE_STRINGS[state], listening_mode_);
    } else {
        ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    }
    
    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    // LED功能已移除
    if(state != kDeviceStateIdle) {
        display->SetIdle(false);
    }
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
        {
            ESP_LOGI(TAG, "设备进入 idle 状态，调用 display->SetIdle(true)");
            display->SetIdle(true);
            // 清理聊天内容，但保留状态栏信息（显示待命）
            display->SetStatus(Lang::Strings::STANDBY);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
#if CONFIG_USE_AUDIO_PROCESSOR
            audio_processor_.Stop();
            // 优化：只在从连接或升级状态切换到idle时才强制重置缓冲区
            if (previous_state == kDeviceStateConnecting || 
                previous_state == kDeviceStateUpgrading ||
                previous_state == kDeviceStateActivating) {
                audio_processor_.ForceResetBuffer();
            }
#endif
#if CONFIG_USE_WAKE_WORD_DETECT
            wake_word_detect_.StartDetection();
#endif
            break;
        }
        case kDeviceStateConnecting:
        {
            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            break;
        }
        case kDeviceStateListening:
        {
            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");

            // Update the IoT states before sending the start listening command
            UpdateIotStates();

            // 优化：避免在按键唤醒后立即发送start事件，防止打断detect事件的处理
            {
                auto now = std::chrono::steady_clock::now();
                auto time_since_button_wake = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_button_wake_time_).count();
                
                // 如果是按键唤醒后1000ms内且从connecting状态切换过来，延迟发送start
                if (previous_state == kDeviceStateConnecting && time_since_button_wake < 1000) {
                    ESP_LOGI(TAG, "按键唤醒场景检测到，延迟发送listen start (距离唤醒%lldms)", time_since_button_wake);
                    
                    // 延迟800ms发送start事件，给服务器时间处理detect事件
                    Schedule([this]() {
                        vTaskDelay(pdMS_TO_TICKS(800));
                        if (device_state_ == kDeviceStateListening) {
                            protocol_->SendStartListening(listening_mode_);
                            ESP_LOGI(TAG, "延迟发送listen start通知服务器开始监听");
                        } else {
                            ESP_LOGI(TAG, "设备状态已改变，取消延迟的listen start");
                        }
                    });
                } else {
                    // 正常情况下立即发送
                    protocol_->SendStartListening(listening_mode_);
                    ESP_LOGI(TAG, "进入listening状态，发送listen start通知服务器开始监听");
                }
            }

            // Make sure the audio processor is running
#if CONFIG_USE_AUDIO_PROCESSOR
            if (!audio_processor_.IsRunning()) {
#else
            if (true) {
#endif
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // 优化：减少缓冲区等待时间从50ms到10ms，减少音频数据丢失
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                // 优化：只在从非listening状态切换时才重置编码器状态
                if (previous_state != kDeviceStateListening) {
                    opus_encoder_->ResetState();
                    ESP_LOGI(TAG, "🎤 重置Opus编码器状态 (从 %s 切换)", STATE_STRINGS[previous_state]);
                } else {
                    ESP_LOGI(TAG, "🎤 保持Opus编码器状态 (从 %s 快速切换)", STATE_STRINGS[previous_state]);
                }
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Start();
                ESP_LOGI(TAG, "🎙️ 音频处理器已启动，准备接收音频数据");
#endif
            }
            break;
        }
        case kDeviceStateSpeaking:
        {
            display->SetStatus(Lang::Strings::SPEAKING);
            display->SetEmotion("speaking");

            // 优化：在非实时聊天模式下，延迟启动唤醒词检测以减少状态切换延迟
            if (listening_mode_ != kListeningModeRealtime) {
#if CONFIG_USE_AUDIO_PROCESSOR
                audio_processor_.Stop();
#endif
                // 优化：延迟启动唤醒词检测，避免在快速状态切换时造成干扰
                Schedule([this]() {
                    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms后启动唤醒词检测
#if CONFIG_USE_WAKE_WORD_DETECT
                    if (device_state_ == kDeviceStateSpeaking) { // 确保状态仍然是speaking
                        wake_word_detect_.StartDetection();
                    }
#endif
                });
            }
            ResetDecoder();
            break;
        }
        case kDeviceStateStarting:
        case kDeviceStateWifiConfiguring:
        case kDeviceStateUpgrading:
        case kDeviceStateActivating:
        case kDeviceStateFatalError:
            // These states are handled elsewhere or don't need specific actions here
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    opus_decoder_->ResetState();
    audio_decode_queue_.clear();
    last_output_time_ = std::chrono::steady_clock::now();
    
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);
}

void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    // 添加空指针检查，防止在音频系统初始化完成前访问
    if (!opus_decoder_) {
        ESP_LOGW(TAG, "opus_decoder_未初始化，跳过设置采样率");
        return;
    }
    
    // 调试信息：显示当前和目标设置
    ESP_LOGI(TAG, "SetDecodeSampleRate: 当前解码器 [采样率:%d, 帧长度:%dms] -> 目标 [采样率:%d, 帧长度:%dms]", 
             opus_decoder_->sample_rate(), opus_decoder_->duration_ms(), sample_rate, frame_duration);
    
    if (opus_decoder_->sample_rate() == sample_rate && opus_decoder_->duration_ms() == frame_duration) {
        ESP_LOGI(TAG, "解码器参数已匹配，无需重新创建");
        return;
    }

    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, 1, frame_duration);
    ESP_LOGI(TAG, "✅ 解码器已重新创建: [采样率:%d, 帧长度:%dms]", 
             opus_decoder_->sample_rate(), opus_decoder_->duration_ms());

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decoder_->sample_rate(), codec->output_sample_rate());
        output_resampler_.Configure(opus_decoder_->sample_rate(), codec->output_sample_rate());
    }
}

void Application::UpdateIotStates() {
    auto& thing_manager = iot::ThingManager::GetInstance();
    std::string states;
    if (thing_manager.GetStatesJson(states, true)) {
        protocol_->SendIotStates(states);
    }
}

void Application::Reboot() {
    ESP_LOGI(TAG, "Rebooting...");
    esp_restart();
}

void Application::WakeWordInvoke(const std::string& wake_word) {
    if (device_state_ == kDeviceStateIdle) {
        ToggleChatState();
        Schedule([this, wake_word]() {
            if (protocol_) {
                protocol_->SendWakeWordDetected(wake_word); 
            }
        }); 
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {   
        Schedule([this]() {
            if (protocol_) {
                protocol_->CloseAudioChannel();
            }
        });
    }
}

bool Application::CanEnterSleepMode() {
    if (device_state_ != kDeviceStateIdle) {
        return false;
    }

    // If protocol was invalidated by timeout, don't check channel status to avoid error logs
    if (protocol_invalidated_by_timeout_) {
        return true;
    }

    // Check if audio channel is opened (this may trigger timeout logs if protocol is in error state)
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return false;
    }

    // Now it is safe to enter sleep mode
    return true;
}

void Application::SetImageResourceCallback(std::function<void()> callback) {
    image_resource_callback_ = callback;
    // 如果OTA检查已经完成，立即执行回调
    if (ota_check_completed_) {
        Schedule(callback);
    }
}

void Application::PauseAudioProcessing() {
    ESP_LOGI(TAG, "暂停音频处理模块...");
    
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        audio_processor_.Stop();
        ESP_LOGI(TAG, "音频处理器已停止");
    }
#endif

#if CONFIG_USE_WAKE_WORD_DETECT
    if (wake_word_detect_.IsDetectionRunning()) {
        wake_word_detect_.StopDetection();
        ESP_LOGI(TAG, "唤醒词检测已停止");
    }
#endif
    
    // 清空音频队列，释放内存
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.clear();
        ESP_LOGI(TAG, "音频解码队列已清空");
    }
    
    // 等待背景任务完成，确保所有音频处理都停止
    if (background_task_) {
        background_task_->WaitForCompletion();
        ESP_LOGI(TAG, "背景音频任务已完成");
    }
}

void Application::ResumeAudioProcessing() {
    ESP_LOGI(TAG, "恢复音频处理模块...");
    
    // 根据当前设备状态决定是否重启音频处理
    if (device_state_ == kDeviceStateIdle) {
#if CONFIG_USE_WAKE_WORD_DETECT
        wake_word_detect_.StartDetection();
        ESP_LOGI(TAG, "唤醒词检测已重启");
#endif
    } else if (device_state_ == kDeviceStateListening) {
#if CONFIG_USE_AUDIO_PROCESSOR
        if (!audio_processor_.IsRunning()) {
            audio_processor_.Start();
            ESP_LOGI(TAG, "音频处理器已重启");
        }
#endif
    }
}

void Application::StopClockTimer() {
    if (clock_timer_handle_) {
        esp_timer_stop(clock_timer_handle_);
        ESP_LOGI(TAG, "时钟定时器已停止（超级省电模式）");
    }
}

void Application::StartClockTimer() {
    if (clock_timer_handle_) {
        esp_timer_start_periodic(clock_timer_handle_, 1000000);  // 每1秒
        ESP_LOGI(TAG, "时钟定时器已启动");
    }
}

bool Application::IsAudioQueueEmpty() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    return audio_decode_queue_.empty();
}

void Application::DiscardPendingAudioForAlarm() {
    ESP_LOGI(TAG, "闹钟预处理：丢弃待处理的音频数据...");

    // 1. 停止音频处理器，防止新的音频数据进入
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        audio_processor_.Stop();
        ESP_LOGI(TAG, "音频处理器已停止");
    }
#endif

    // 2. 清空背景任务队列，丢弃所有待编码的音频任务
    if (background_task_) {
        background_task_->ClearQueue();
        ESP_LOGI(TAG, "背景任务队列已清空，待编码音频已丢弃");
    }

    // 3. 重置Opus编码器状态，确保没有残留的音频数据
    if (opus_encoder_) {
        opus_encoder_->ResetState();
        ESP_LOGI(TAG, "Opus编码器状态已重置");
    }

    // 4. 清空音频解码队列（虽然这是输出队列，但为了保险起见也清空）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.clear();
        ESP_LOGI(TAG, "音频解码队列已清空");
    }

    ESP_LOGI(TAG, "闹钟预处理：所有待处理音频数据已丢弃完成");
}

// **新增：强力音频保护机制实现**

bool Application::IsAudioActivityHigh() const {
    // 多重音频活动检测 - 确保最高精度
    
    // 1. 设备状态检测
    if (device_state_ == kDeviceStateListening || 
        device_state_ == kDeviceStateConnecting ||
        device_state_ == kDeviceStateSpeaking) {
        return true;
    }
    
    // 2. 音频队列检测  
    if (!IsAudioQueueEmpty()) {
        return true;
    }
    
    // 3. 音频处理器状态检测 - 使用const_cast因为这是只读检查
#if CONFIG_USE_AUDIO_PROCESSOR
    if (const_cast<Application*>(this)->audio_processor_.IsRunning()) {
        return true;
    }
#endif
    
    // 4. 唤醒词检测器状态检测 - 使用const_cast因为这是只读检查
#if CONFIG_USE_WAKE_WORD_DETECT
    if (const_cast<Application*>(this)->wake_word_detect_.IsDetectionRunning()) {
        return true;
    }
#endif
    
    // 5. 协议音频通道检测
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        return true;
    }
    
    return false;
}

bool Application::IsAudioProcessingCritical() const {
    // 关键音频处理状态检测 - 绝对不能中断
    
    // 正在进行语音识别
    if (device_state_ == kDeviceStateListening && voice_detected_) {
        return true;
    }
    
    // 正在播放重要音频（TTS、系统提示音）
    if (device_state_ == kDeviceStateSpeaking) {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        // 如果音频队列有较多待播放数据，认为是关键状态
        return audio_decode_queue_.size() > 3;
    }
    
    // 正在建立音频连接
    if (device_state_ == kDeviceStateConnecting) {
        return true;
    }
    
    return false;
}

void Application::SetAudioPriorityMode(bool enabled) {
    if (enabled) {
        ESP_LOGI(TAG, "🔒 启用音频优先模式 - 图片播放将被严格限制");
        
        // 提升音频相关任务的优先级
        if (audio_loop_task_handle_) {
            vTaskPrioritySet(audio_loop_task_handle_, 10); // 提升到最高优先级
        }
        
        // 降低背景任务优先级，减少对音频的干扰
        if (background_task_) {
            // 假设background_task_有设置优先级的方法，这里仅作示例
            ESP_LOGI(TAG, "降低背景任务优先级以保护音频处理");
        }
        
    } else {
        ESP_LOGI(TAG, "🔓 恢复正常优先级模式");
        
        // 恢复音频任务正常优先级
        if (audio_loop_task_handle_) {
            vTaskPrioritySet(audio_loop_task_handle_, 9); // 恢复原优先级
        }
    }
}

int Application::GetAudioPerformanceScore() const {
    int score = 100; // 满分100，分数越低表示音频压力越大
    
    // 音频队列长度影响 (-10分每个队列项目)
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        score -= std::min(50, (int)audio_decode_queue_.size() * 10);
    }
    
    // 设备状态影响
    switch (device_state_) {
        case kDeviceStateListening:
            score -= 20; // 语音识别时压力较大
            if (voice_detected_) {
                score -= 15; // 检测到语音时压力更大
            }
            break;
        case kDeviceStateSpeaking:
            score -= 25; // TTS播放时压力最大
            break;
        case kDeviceStateConnecting:
            score -= 15; // 连接时有一定压力
            break;
        case kDeviceStateStarting:
        case kDeviceStateWifiConfiguring:
        case kDeviceStateUpgrading:
        case kDeviceStateActivating:
        case kDeviceStateFatalError:
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            // These states have minimal impact on audio performance
            break;
        default:
            break;
    }
    
    // 内存压力影响
    int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (free_sram < 100000) { // 小于100KB
        score -= 20;
    } else if (free_sram < 200000) { // 小于200KB  
        score -= 10;
    }
    
    // 确保分数在合理范围内
    return std::max(0, std::min(100, score));
}

// **新增：智能分级音频保护实现**

bool Application::IsRealAudioProcessing() const {
    // 检测是否有真正的音频处理活动（有数据流动）
    
    // 1. 检查音频队列是否有数据
    if (!IsAudioQueueEmpty()) {
        return true;
    }
    
    // 2. 检查是否正在播放音频（TTS）
    if (device_state_ == kDeviceStateSpeaking) {
        return true;
    }
    
    // 3. 检查是否正在进行语音识别且检测到语音
    if (device_state_ == kDeviceStateListening && voice_detected_) {
        return true;
    }
    
    // 4. 检查是否正在建立音频连接
    if (device_state_ == kDeviceStateConnecting) {
        return true;
    }
    
    return false;
}

Application::AudioActivityLevel Application::GetAudioActivityLevel() const {
    // 分级音频活动检测 - 返回具体的活动级别
    
    // 最高级别：关键音频处理 - 完全暂停图片
    if (IsAudioProcessingCritical()) {
        return AUDIO_CRITICAL;
    }
    
    // 高级别：实际音频处理 - 降低图片优先级
    if (IsRealAudioProcessing()) {
        return AUDIO_ACTIVE;
    }
    
    // 中级别：音频系统待机 - 允许低帧率播放
    // 唤醒词检测运行但没有实际音频处理
#if CONFIG_USE_WAKE_WORD_DETECT
    if (const_cast<Application*>(this)->wake_word_detect_.IsDetectionRunning() && 
        device_state_ == kDeviceStateIdle) {
        return AUDIO_STANDBY;
    }
#endif
    
    // 音频通道开启但没有实际数据传输
    if (protocol_ && protocol_->IsAudioChannelOpened() && 
        device_state_ == kDeviceStateIdle && IsAudioQueueEmpty()) {
        return AUDIO_STANDBY;
    }
    
    // 最低级别：完全空闲 - 允许正常图片播放
    return AUDIO_IDLE;
}

void Application::ExecuteLocalIntent(const intent::IntentResult& result) {
    // 构建IOT命令JSON
    cJSON* command = cJSON_CreateObject();
    if (!command) {
        ESP_LOGE(TAG, "Failed to create JSON command object");
        return;
    }
    
    // ThingManager期望的是 "name" 字段，不是 "device"
    cJSON_AddStringToObject(command, "name", result.device_name.c_str());
    cJSON_AddStringToObject(command, "method", result.action.c_str());
    
    cJSON* parameters = cJSON_CreateObject();
    if (!parameters) {
        ESP_LOGE(TAG, "Failed to create JSON parameters object");
        cJSON_Delete(command);
        return;
    }
    
    // 添加参数
    for (const auto& param : result.parameters) {
        if (param.first == "volume") {
            int volume_value = std::stoi(param.second);
            cJSON_AddNumberToObject(parameters, param.first.c_str(), volume_value);
            ESP_LOGI(TAG, "添加音量参数: %d", volume_value);
        } else if (param.first == "brightness") {
            int brightness_value = std::stoi(param.second);
            cJSON_AddNumberToObject(parameters, param.first.c_str(), brightness_value);
            ESP_LOGI(TAG, "添加亮度参数: %d", brightness_value);
        } else if (param.first == "theme_name") {
            cJSON_AddStringToObject(parameters, param.first.c_str(), param.second.c_str());
            ESP_LOGI(TAG, "添加主题参数: %s", param.second.c_str());
        } else if (param.first == "relative" && result.type == intent::IntentType::BRIGHTNESS_CONTROL) {
            // 处理相对亮度调节
            auto backlight = Board::GetInstance().GetBacklight();
            if (backlight) {
                int current_brightness = backlight->brightness();
                int new_brightness = current_brightness;
                
                if (param.second == "increase_10") {
                    new_brightness = std::min(100, current_brightness + 10);
                    ESP_LOGI(TAG, "亮度大一点: %d -> %d (+10)", current_brightness, new_brightness);
                } else if (param.second == "decrease_10") {
                    new_brightness = std::max(0, current_brightness - 10);
                    ESP_LOGI(TAG, "亮度小一点: %d -> %d (-10)", current_brightness, new_brightness);
                } else if (param.second == "increase") {
                    new_brightness = std::min(100, current_brightness + 20);
                    ESP_LOGI(TAG, "亮度调亮: %d -> %d (+20)", current_brightness, new_brightness);
                } else if (param.second == "decrease") {
                    new_brightness = std::max(0, current_brightness - 20);
                    ESP_LOGI(TAG, "亮度调暗: %d -> %d (-20)", current_brightness, new_brightness);
                }
                
                cJSON_AddNumberToObject(parameters, "brightness", new_brightness);
            }
        } else if (param.first == "relative" && result.type == intent::IntentType::VOLUME_CONTROL) {
            // 处理相对音量调节
            auto codec = Board::GetInstance().GetAudioCodec();
            if (codec) {
                int current_volume = codec->output_volume();
                int new_volume = current_volume;
                
                if (param.second == "increase_10") {
                    new_volume = std::min(100, current_volume + 10);
                    ESP_LOGI(TAG, "音量大一点: %d -> %d (+10)", current_volume, new_volume);
                } else if (param.second == "decrease_10") {
                    new_volume = std::max(0, current_volume - 10);
                    ESP_LOGI(TAG, "音量小一点: %d -> %d (-10)", current_volume, new_volume);
                } else if (param.second == "increase") {
                    new_volume = std::min(100, current_volume + 15);
                    ESP_LOGI(TAG, "音量调大: %d -> %d (+15)", current_volume, new_volume);
                } else if (param.second == "decrease") {
                    new_volume = std::max(0, current_volume - 15);
                    ESP_LOGI(TAG, "音量调小: %d -> %d (-15)", current_volume, new_volume);
                }
                
                cJSON_AddNumberToObject(parameters, "volume", new_volume);
            }
        } else {
            cJSON_AddStringToObject(parameters, param.first.c_str(), param.second.c_str());
        }
    }
    cJSON_AddItemToObject(command, "parameters", parameters);
    
    // 执行IOT命令
    ESP_LOGI(TAG, "执行本地检测的IOT命令: %s.%s", result.device_name.c_str(), result.action.c_str());
    
    auto& thing_manager = iot::ThingManager::GetInstance();
    std::string error;
    bool exec_ok = thing_manager.InvokeSync(command, &error);
    if (!exec_ok) {
        ESP_LOGE(TAG, "IOT命令执行失败: %s", error.c_str());
    } else {
        ESP_LOGI(TAG, "IOT命令执行成功");
    }
    
    // 向服务端发送本地意图执行结果通知
    if (notifier_) {
        cJSON* notification = cJSON_CreateObject();
        if (notification) {
            // 基础字段
            cJSON_AddStringToObject(notification, "type", "local_intent_result");
            cJSON_AddStringToObject(notification, "status", exec_ok ? "ok" : "error");
            cJSON_AddNumberToObject(notification, "ts", (double)time(NULL));
            
            // 意图类型转字符串
            const char* intent_type_str = "unknown";
            switch (result.type) {
                case intent::IntentType::VOLUME_CONTROL:
                    intent_type_str = "volume_control";
                    break;
                case intent::IntentType::BRIGHTNESS_CONTROL:
                    intent_type_str = "brightness_control";
                    break;
                case intent::IntentType::THEME_CONTROL:
                    intent_type_str = "theme_control";
                    break;
                case intent::IntentType::DISPLAY_MODE_CONTROL:
                    intent_type_str = "display_mode_control";
                    break;
                default:
                    break;
            }
            cJSON_AddStringToObject(notification, "intent_type", intent_type_str);
            
            // 设备和方法
            cJSON_AddStringToObject(notification, "device", result.device_name.c_str());
            cJSON_AddStringToObject(notification, "method", result.action.c_str());
            
            // 参数（复制原有参数）
            cJSON_AddItemToObject(notification, "parameters", cJSON_Duplicate(parameters, 1));
            
            // 置信度
            cJSON_AddNumberToObject(notification, "confidence", result.confidence);
            
            // 错误信息（如果有）
            if (!exec_ok && !error.empty()) {
                cJSON_AddStringToObject(notification, "error", error.c_str());
            }
            
            // 获取并附加最新IoT状态
            std::string states_json;
            thing_manager.GetStatesJson(states_json, false);
            if (!states_json.empty() && states_json != "[]") {
                cJSON* states = cJSON_Parse(states_json.c_str());
                if (states) {
                    cJSON_AddItemToObject(notification, "states", states);
                }
            }
            
            // 使用 PublishAck 发送（QoS=0，快速发送不等待确认）
            bool sent = notifier_->PublishAck(notification);
            if (sent) {
                ESP_LOGD(TAG, "本地意图执行结果已提交发送 (via ack topic, QoS=0)");
            } else {
                ESP_LOGD(TAG, "本地意图执行结果发送失败");
            }
            
            cJSON_Delete(notification);
        }
    }
    
    cJSON_Delete(command);
}

void Application::SendAlarmMessage() {
#if CONFIG_USE_ALARM
    if (alarm_m_ == nullptr) {
        ESP_LOGE(TAG, "AlarmManager is null, cannot send alarm message");
        return;
    }

    std::string alarm_message = alarm_m_->get_now_alarm_name();
    ESP_LOGI(TAG, "原始闹钟消息: %s", alarm_message.c_str());

    // 从JSON中提取闹钟名称 - 修复字符串解析问题
    size_t start = alarm_message.find("闹钟-#");
    if (start != std::string::npos) {
        start += 5; // "闹钟-#" 的字节长度（UTF-8编码）
        size_t end = alarm_message.find("\"", start);
        if (end != std::string::npos) {
            std::string alarm_name = alarm_message.substr(start, end - start);
            std::string alarm_text = "闹钟-" + alarm_name;
            protocol_->SendWakeWordDetected(alarm_text);
            ESP_LOGI(TAG, "闹钟消息已发送: %s", alarm_text.c_str());
        } else {
            ESP_LOGW(TAG, "无法解析闹钟名称结束位置，使用备用方案");
            if (!alarm_message.empty()) {
                protocol_->SendText(alarm_message);
            } else {
                ESP_LOGW(TAG, "备用方案：闹钟消息为空，跳过发送");
            }
        }
    } else {
        ESP_LOGW(TAG, "闹钟消息格式异常，使用备用方案");
        if (!alarm_message.empty()) {
            protocol_->SendText(alarm_message);
        } else {
            ESP_LOGW(TAG, "备用方案：闹钟消息为空，跳过发送");
        }
    }
#endif
}

void Application::HandleProtocolTimeout() {
    // 防止重复处理超时
    if (timeout_handling_active_) {
        return;
    }
    
    timeout_handling_active_ = true;
    ESP_LOGW(TAG, "Handling protocol timeout - current state: %s", STATE_STRINGS[device_state_]);
    
    // 检查是否在关键状态，如果是则不处理超时
    if (device_state_ == kDeviceStateUpgrading || 
        device_state_ == kDeviceStateWifiConfiguring ||
        device_state_ == kDeviceStateActivating) {
        ESP_LOGI(TAG, "Device in critical state, skipping timeout handling");
        timeout_handling_active_ = false;
        return;
    }
    
    // 主动关闭音频通道
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        ESP_LOGI(TAG, "Closing audio channel due to timeout");
        protocol_->CloseAudioChannel();
    }
    
    // 清理音频队列
    {
        std::lock_guard<std::mutex> lock(mutex_);
        audio_decode_queue_.clear();
        ESP_LOGI(TAG, "Audio decode queue cleared");
    }
    
    // 等待背景任务完成
    if (background_task_) {
        background_task_->WaitForCompletion();
    }
    
    // 重置音频编码器和解码器状态
    if (opus_encoder_) {
        opus_encoder_->ResetState();
    }
    if (opus_decoder_) {
        opus_decoder_->ResetState();
    }
    
    // 停止音频处理器
#if CONFIG_USE_AUDIO_PROCESSOR
    if (audio_processor_.IsRunning()) {
        audio_processor_.Stop();
        audio_processor_.ForceResetBuffer();
    }
#endif
    
    // 标记Protocol为失效状态，避免后续的重复检查
    protocol_invalidated_by_timeout_ = true;
    
    // 设置设备状态为idle
    SetDeviceState(kDeviceStateIdle);
    
    ESP_LOGI(TAG, "Protocol timeout handling completed, device returned to idle state");
    timeout_handling_active_ = false;
}

/**
 * @brief 停止MQTT通知服务，用于省电模式
 */
void Application::StopMqttNotifier() {
    if (notifier_) {
        ESP_LOGI(TAG, "正在停止MQTT通知服务...");
        notifier_->Stop();
        ESP_LOGI(TAG, "MQTT通知服务已停止");
    }
}

/**
 * @brief 启动MQTT通知服务，用于从省电模式恢复
 */
void Application::StartMqttNotifier() {
    if (notifier_) {
        ESP_LOGI(TAG, "正在启动MQTT通知服务...");
        notifier_->Start();
        ESP_LOGI(TAG, "MQTT通知服务已启动");
    }
}

/**
 * @brief 获取设备配置信息
 * 从NVS存储中读取MQTT配置参数
 * @return DeviceConfig 设备配置结构，包含MQTT连接参数
 */
const DeviceConfig& Application::GetDeviceConfig() const {
    if (!device_config_loaded_) {
        // 从Settings中读取MQTT配置
        Settings mqtt_settings("mqtt", false);
        
        // 读取MQTT服务器配置
        std::string endpoint = mqtt_settings.GetString("endpoint", "110.42.35.132");
        
        // 解析主机和端口
        size_t colon_pos = endpoint.find(':');
        if (colon_pos != std::string::npos) {
            device_config_.mqtt_host = endpoint.substr(0, colon_pos);
            device_config_.mqtt_port = std::stoi(endpoint.substr(colon_pos + 1));
        } else {
            device_config_.mqtt_host = endpoint;
            device_config_.mqtt_port = 1883; // 默认MQTT端口（非SSL）
        }
        
        // 读取认证信息
        device_config_.mqtt_username = mqtt_settings.GetString("username", "xiaoqiao");
        device_config_.mqtt_password = mqtt_settings.GetString("password", "dzkj0000");
        device_config_.device_id = mqtt_settings.GetString("client_id", "xiaozhi_device");
        
        device_config_loaded_ = true;
        
        ESP_LOGI(TAG, "Device config loaded: host=%s, port=%d, username=%s, client_id=%s", 
                 device_config_.mqtt_host.c_str(), 
                 device_config_.mqtt_port,
                 device_config_.mqtt_username.c_str(),
                 device_config_.device_id.c_str());
    }
    
    return device_config_;
}
