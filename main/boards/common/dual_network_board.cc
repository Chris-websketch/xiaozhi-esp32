#include "dual_network_board.h"
#include "application.h"
#include "display.h"
#include "assets/lang_config.h"
#include "settings.h"
#include <esp_log.h>

static const char *TAG = "DualNetworkBoard";

DualNetworkBoard::DualNetworkBoard(gpio_num_t ml307_tx_pin, gpio_num_t ml307_rx_pin, gpio_num_t ml307_dtr_pin, int32_t default_net_type) 
    : Board(), 
      ml307_tx_pin_(ml307_tx_pin), 
      ml307_rx_pin_(ml307_rx_pin), 
      ml307_dtr_pin_(ml307_dtr_pin) {
    
    // 从Settings加载网络类型
    network_type_ = LoadNetworkTypeFromSettings(default_net_type);
    
    // 只初始化当前网络类型对应的板卡
    InitializeCurrentBoard();
}

NetworkType DualNetworkBoard::LoadNetworkTypeFromSettings(int32_t default_net_type) {
    Settings settings("network", true);
    int network_type = settings.GetInt("type", default_net_type); // 默认使用ML307 (1)
    return network_type == 1 ? NetworkType::ML307 : NetworkType::WIFI;
}

void DualNetworkBoard::SaveNetworkTypeToSettings(NetworkType type) {
    Settings settings("network", true);
    int network_type = (type == NetworkType::ML307) ? 1 : 0;
    settings.SetInt("type", network_type);
}

void DualNetworkBoard::InitializeCurrentBoard() {
    if (network_type_ == NetworkType::ML307) {
        ESP_LOGI(TAG, "Initialize ML307 board");
        current_board_ = std::make_unique<Ml307Board>(ml307_tx_pin_, ml307_rx_pin_, 4096);
    } else {
        ESP_LOGI(TAG, "Initialize WiFi board");
        current_board_ = std::make_unique<WifiBoard>();
    }
}

void DualNetworkBoard::SwitchNetworkType() {
    auto display = Board::GetInstance().GetDisplay();
    auto& app = Application::GetInstance();
    
    ESP_LOGI(TAG, "准备切换网络类型，当前类型: %d", static_cast<int>(network_type_));
    
    // 1. 先关闭所有网络连接
    ESP_LOGI(TAG, "关闭当前网络连接...");
    
    // 关闭MQTT通知服务（心跳、通知）
    ESP_LOGI(TAG, "停止MQTT通知服务");
    app.StopMqttNotifier();
    
    // 关闭WebSocket音频通道（如果打开）
    auto& protocol = app.GetProtocol();
    if (protocol.IsAudioChannelOpened()) {
        ESP_LOGI(TAG, "关闭音频通道");
        protocol.CloseAudioChannel();
    }
    
    // 等待所有连接完全关闭
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 2. 退出idle模式，显示通知
    display->SetIdle(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 3. 保存新的网络类型并显示通知
    if (network_type_ == NetworkType::WIFI) {    
        SaveNetworkTypeToSettings(NetworkType::ML307);
        ESP_LOGI(TAG, "切换到 4G 网络，显示通知");
        display->ShowNotification(Lang::Strings::SWITCH_TO_4G_NETWORK, 3000);
    } else {
        SaveNetworkTypeToSettings(NetworkType::WIFI);
        ESP_LOGI(TAG, "切换到 Wi-Fi 网络，显示通知");
        display->ShowNotification(Lang::Strings::SWITCH_TO_WIFI_NETWORK, 3000);
    }
    
    // 4. 等待通知显示后重启
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "网络切换完成，即将重启");
    app.Reboot();
}

 
std::string DualNetworkBoard::GetBoardType() {
    return current_board_->GetBoardType();
}

void DualNetworkBoard::StartNetwork() {
    auto display = Board::GetInstance().GetDisplay();
    
    if (network_type_ == NetworkType::WIFI) {
        display->SetStatus(Lang::Strings::CONNECTING);
    } else {
        display->SetStatus(Lang::Strings::DETECTING_MODULE);
    }
    current_board_->StartNetwork();
}

bool DualNetworkBoard::IsNetworkReady() {
    return current_board_ ? current_board_->IsNetworkReady() : false;
}

Http* DualNetworkBoard::CreateHttp() {
    return current_board_ ? current_board_->CreateHttp() : nullptr;
}

WebSocket* DualNetworkBoard::CreateWebSocket() {
    return current_board_ ? current_board_->CreateWebSocket() : nullptr;
}

Mqtt* DualNetworkBoard::CreateMqtt() {
    return current_board_ ? current_board_->CreateMqtt() : nullptr;
}

Udp* DualNetworkBoard::CreateUdp() {
    return current_board_ ? current_board_->CreateUdp() : nullptr;
}

const char* DualNetworkBoard::GetNetworkStateIcon() {
    return current_board_->GetNetworkStateIcon();
}

void DualNetworkBoard::SetPowerSaveMode(bool enabled) {
    current_board_->SetPowerSaveMode(enabled);
}

std::string DualNetworkBoard::GetBoardJson() {
    return current_board_ ? current_board_->GetJson() : std::string();
}
