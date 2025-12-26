#include "wifi_board.h"

#include "display.h"
#include "application.h"
#include "system_info.h"
#include "font_awesome_symbols.h"
#include "settings.h"
#include "assets/lang_config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http.h>
#include <esp_mqtt.h>
#include <esp_udp.h>
#include <tcp_transport.h>
#include <tls_transport.h>
#include <web_socket.h>
#include <esp_log.h>
#include <esp_system.h>

#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>

#if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
#include "afsk_demod.h"
#include "board.h"
#endif

#if CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING && CONFIG_BT_BLE_BLUFI_ENABLE
#include "blufi.h"
#ifndef BOARD_H
#include "board.h"
#endif
#endif

static const char *TAG = "WifiBoard";

WifiBoard::WifiBoard() {
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap is set to 1, reset to 0");
        settings.SetInt("force_ap", 0);
    }
    use_acoustic_mode_ = settings.GetInt("use_acoustic") == 1;
    if (use_acoustic_mode_) {
        ESP_LOGI(TAG, "use_acoustic is set to 1, reset to 0");
        settings.SetInt("use_acoustic", 0);
    }
}

std::string WifiBoard::GetBoardType() {
    return "wifi";
}

void WifiBoard::EnterWifiConfigMode() {
    auto& application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);

#if CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING && CONFIG_BT_BLE_BLUFI_ENABLE && CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
    // 同时支持蓝牙配网和声波配网，根据use_acoustic_mode_标志选择
    if (use_acoustic_mode_) {
        // 使用声波配网模式
        ESP_LOGI(TAG, "启动声波配网功能");
        
        std::string hint = "请播放声波配网音频进行配置";
        application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
        
        // 显示声波配网二维码
        auto display = Board::GetInstance().GetDisplay();
        if (display && display->width() > 0) {
            display->ShowQRCode("https://dzyupan.xmduzhong.com/#s/EBF-D13G");
            ESP_LOGI(TAG, "声波配网二维码已显示");
        }
        
        // 创建boot按钮实例，用于监听双击事件切换配网模式
        config_button_ = new Button(GPIO_NUM_0);
        config_button_->OnDoubleClick([]() {
            ESP_LOGI(TAG, "双击BOOT按钮，切换到蓝牙配网模式");
            Settings settings("wifi", true);
            settings.SetInt("use_acoustic", 0);  // 下次使用蓝牙配网
            settings.SetInt("force_ap", 1);      // 强制进入配网模式
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        });
        
        // 启动声波配网任务
        auto codec = Board::GetInstance().GetAudioCodec();
        int input_channels = 1;
        if (codec) {
            input_channels = codec->input_channels();
        }
        
        ESP_LOGI(TAG, "启动声波配网任务，音频输入通道数: %d", input_channels);
        
        xTaskCreate([](void* param) {
            auto* params = static_cast<std::tuple<Application*, Display*, size_t>*>(param);
            auto* app = std::get<0>(*params);
            auto* display = std::get<1>(*params);
            auto channels = std::get<2>(*params);
            
            ESP_LOGI("AcousticWiFi", "声波配网任务启动");
            audio_wifi_config::ReceiveWifiCredentialsFromAudio(app, display, channels);
            
            delete params;
            vTaskDelete(NULL);
        }, "acoustic_wifi_config", 8192, 
           new std::tuple<Application*, Display*, size_t>(
               &application, display, input_channels), 
           5, NULL);
    } else {
        // 使用蓝牙配网 (BluFi)
        ESP_LOGI(TAG, "启动BluFi蓝牙配网功能");
        
        std::string device_name = "Voxia-" + SystemInfo::GetMacAddress().substr(9, 5);
        std::string hint = "请先打开手机蓝牙，扫描屏幕二维码，进入小程序后台配网页面扫描设备: " + device_name + " 进行配网";
        application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
        
        // 显示蓝牙配网二维码
        auto display = Board::GetInstance().GetDisplay();
        if (display && display->width() > 0) {
            display->ShowQRCode("https://xq-download.xmduzhong.com/");
            ESP_LOGI(TAG, "蓝牙配网二维码已显示");
        }
        
        // 创建boot按钮实例，用于监听双击事件切换配网模式
        config_button_ = new Button(GPIO_NUM_0);
        config_button_->OnDoubleClick([]() {
            ESP_LOGI(TAG, "双击BOOT按钮，切换到声波配网模式");
            Settings settings("wifi", true);
            settings.SetInt("use_acoustic", 1);  // 下次使用声波配网
            settings.SetInt("force_ap", 1);      // 强制进入配网模式
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        });
        
        auto &blufi = Blufi::GetInstance();
        blufi.init();
    }
#elif CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING && CONFIG_BT_BLE_BLUFI_ENABLE
    // 仅蓝牙配网可用
    ESP_LOGI(TAG, "启动BluFi蓝牙配网功能");
    
    std::string device_name = "Voxia-" + SystemInfo::GetMacAddress().substr(9, 5);
    std::string hint = "请先打开手机蓝牙，扫描屏幕二维码，进入小程序后台配网页面扫描设备: " + device_name + " 进行配网";
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
    
    // 显示蓝牙配网二维码
    auto display = Board::GetInstance().GetDisplay();
    if (display && display->width() > 0) {
        display->ShowQRCode("https://xq-download.xmduzhong.com/");
        ESP_LOGI(TAG, "蓝牙配网二维码已显示");
    }
    
    auto &blufi = Blufi::GetInstance();
    blufi.init();
#else
    // 使用传统AP配网
    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetLanguage(Lang::CODE);
    wifi_ap.SetSsidPrefix("独众AI伴侣");
    wifi_ap.Start();

    // 显示 WiFi 配置 AP 的 SSID 和 Web 服务器 URL
    std::string hint = Lang::Strings::CONNECT_TO_HOTSPOT;
    hint += wifi_ap.GetSsid();
    hint += Lang::Strings::ACCESS_VIA_BROWSER;
    hint += wifi_ap.GetWebServerUrl();
    hint += "\n";
    
#if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
    // 添加声波配网提示信息
    hint += "或播放声波配网音频进行快速配置\n";
#endif
    
    // 播报配置 WiFi 的提示
    application.Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "", Lang::Sounds::P3_WIFICONFIG);
    
    // 显示WiFi配网二维码
    auto display = Board::GetInstance().GetDisplay();
    if (display && display->width() > 0) {
        // 生成WiFi二维码内容（WiFi标准格式，手机扫码可自动连接AP）
        std::string qr_data = "WIFI:T:nopass;S:";
        qr_data += wifi_ap.GetSsid();
        qr_data += ";P:;;";
        
        // 显示二维码（自适应大小和位置）
        display->ShowQRCode(qr_data.c_str());
        
        ESP_LOGI(TAG, "WiFi QR Code displayed: %s", qr_data.c_str());
    }
    
#if CONFIG_USE_ACOUSTIC_WIFI_PROVISIONING
    // 启动声波配网任务作为传统配网的补充
    auto codec = Board::GetInstance().GetAudioCodec();
    int input_channels = 1;
    if (codec) {
        input_channels = codec->input_channels();
    }
    
    ESP_LOGI(TAG, "启动声波配网功能作为配网补充选项，音频输入通道数: %d", input_channels);
    
    // 创建声波配网任务
    xTaskCreate([](void* param) {
        auto* params = static_cast<std::tuple<Application*, Display*, size_t>*>(param);
        auto* app = std::get<0>(*params);
        auto* display = std::get<1>(*params);
        auto channels = std::get<2>(*params);
        
        ESP_LOGI("AcousticWiFi", "声波配网任务启动");
        audio_wifi_config::ReceiveWifiCredentialsFromAudio(app, display, channels);
        
        delete params;
        vTaskDelete(NULL);
    }, "acoustic_wifi_config", 8192, 
       new std::tuple<Application*, Display*, size_t>(
           &application, display, input_channels), 
       5, NULL);
#endif
#endif // CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING
    
    // Wait forever until reset after configuration
    while (true) {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void WifiBoard::StartNetwork() {
    // User can press BOOT button while starting to enter WiFi configuration mode
    if (wifi_config_mode_) {
        EnterWifiConfigMode();
        return;
    }

    // If no WiFi SSID is configured, enter WiFi configuration mode
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (ssid_list.empty()) {
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }

    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.OnScanBegin([this]() {
        auto display = Board::GetInstance().GetDisplay();
        display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
    });
    wifi_station.OnConnect([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECT_TO;
        notification += ssid;
        notification += "...";
        display->ShowNotification(notification.c_str(), 30000);
    });
    wifi_station.OnConnected([this](const std::string& ssid) {
        auto display = Board::GetInstance().GetDisplay();
        std::string notification = Lang::Strings::CONNECTED_TO;
        notification += ssid;
        display->ShowNotification(notification.c_str(), 30000);
    });
    wifi_station.Start();

    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    if (!wifi_station.WaitForConnected(60 * 1000)) {
        wifi_station.Stop();
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
}

Http* WifiBoard::CreateHttp() {
    return new EspHttp();
}

WebSocket* WifiBoard::CreateWebSocket() {
    // WebSocket URL现在从OTA动态获取，根据协议选择传输层
    // ws:// 使用TCP传输，wss:// 使用TLS传输
    auto& application = Application::GetInstance();
    auto& ota = application.GetOta();
    
    if (ota.HasWebsocketConfig()) {
        std::string url = ota.GetWebsocketUrl();
        if (url.find("wss://") == 0) {
            // 使用TLS传输
            return new WebSocket(new TlsTransport());
        } else if (url.find("ws://") == 0) {
            // 使用TCP传输
            return new WebSocket(new TcpTransport());
        }
    }
    
    // 默认使用TLS传输作为后备方案
    return new WebSocket(new TlsTransport());
}

Mqtt* WifiBoard::CreateMqtt() {
    return new EspMqtt();
}

Udp* WifiBoard::CreateUdp() {
    return new EspUdp();
}

const char* WifiBoard::GetNetworkStateIcon() {
    if (wifi_config_mode_) {
        return FONT_AWESOME_WIFI;
    }
    auto& wifi_station = WifiStation::GetInstance();
    if (!wifi_station.IsConnected()) {
        return FONT_AWESOME_WIFI_OFF;
    }
    int8_t rssi = wifi_station.GetRssi();
    if (rssi >= -60) {
        return FONT_AWESOME_WIFI;
    } else if (rssi >= -70) {
        return FONT_AWESOME_WIFI_FAIR;
    } else {
        return FONT_AWESOME_WIFI_WEAK;
    }
}

std::string WifiBoard::GetBoardJson() {
    // Set the board type for OTA
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_json = std::string("{\"type\":\"" BOARD_TYPE "\",");
    board_json += "\"name\":\"" BOARD_NAME "\",";
    if (!wifi_config_mode_) {
        board_json += "\"ssid\":\"" + wifi_station.GetSsid() + "\",";
        board_json += "\"rssi\":" + std::to_string(wifi_station.GetRssi()) + ",";
        board_json += "\"channel\":" + std::to_string(wifi_station.GetChannel()) + ",";
        board_json += "\"ip\":\"" + wifi_station.GetIpAddress() + "\",";
    }
    board_json += "\"mac\":\"" + SystemInfo::GetMacAddress() + "\"}";
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled) {
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);
}

void WifiBoard::ResetWifiConfiguration() {
    // Set a flag and reboot the device to enter the network configuration mode
    {
        Settings settings("wifi", true);
        settings.SetInt("force_ap", 1);
    }
    GetDisplay()->ShowNotification(Lang::Strings::ENTERING_WIFI_CONFIG_MODE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Reboot the device
    esp_restart();
}
