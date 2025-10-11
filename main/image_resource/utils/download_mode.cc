#include "download_mode.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "board.h"
#include "application.h"

#define TAG "DownloadMode"

namespace ImageResource {

DownloadMode::~DownloadMode() {
    if (is_active_) {
        Exit();
    }
}

void DownloadMode::Enter() {
    if (is_active_) {
        ESP_LOGW(TAG, "已处于下载模式");
        return;
    }
    
    ESP_LOGI(TAG, "进入专用下载模式，优化系统资源...");
    
    auto& board = Board::GetInstance();
    auto& app = Application::GetInstance();
    
    // 立即显示下载UI，告知用户正在准备
    auto display = board.GetDisplay();
    if (display) {
        display->SetDownloadProgress(0, "正在准备下载...");
        ESP_LOGI(TAG, "已显示下载准备UI");
    }
    
    // 检查设备状态
    DeviceState current_state = app.GetDeviceState();
    if (current_state == kDeviceStateSpeaking || current_state == kDeviceStateListening) {
        ESP_LOGW(TAG, "设备正在进行音频操作，等待完成...");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    
    // 1. 禁用省电模式
    board.SetPowerSaveMode(false);
    ESP_LOGI(TAG, "已禁用省电模式");
    
    // 2. 暂停音频处理
    app.PauseAudioProcessing();
    
    // 3. 暂停音频编解码器
    auto codec = board.GetAudioCodec();
    if (codec) {
        codec->EnableInput(false);
        codec->EnableOutput(false);
        ESP_LOGI(TAG, "已暂停音频输入输出");
    }
    
    // 4. 提高任务优先级
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 2);
    ESP_LOGI(TAG, "已提高下载任务优先级");
    
    // 5. 更新UI显示为就绪状态
    if (display) {
        display->SetDownloadProgress(0, "准备就绪，即将开始下载");
    }
    
    // 6. 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(300));
    
    is_active_ = true;
    ESP_LOGI(TAG, "下载模式设置完成");
}

void DownloadMode::Exit() {
    if (!is_active_) {
        return;
    }
    
    ESP_LOGI(TAG, "退出下载模式，恢复正常运行状态...");
    
    auto& board = Board::GetInstance();
    
    // 1. 恢复任务优先级
    vTaskPrioritySet(NULL, 4);
    ESP_LOGI(TAG, "已恢复正常任务优先级");
    
    // 2. 重新启用省电模式
    board.SetPowerSaveMode(true);
    ESP_LOGI(TAG, "已重新启用省电模式");
    
    // 3. 恢复音频编解码器
    auto codec = board.GetAudioCodec();
    if (codec) {
        codec->EnableInput(true);
        codec->EnableOutput(true);
        ESP_LOGI(TAG, "已恢复音频输入输出");
    }
    
    // 4. 恢复音频处理
    auto& app = Application::GetInstance();
    app.ResumeAudioProcessing();
    
    // 5. 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(800));
    
    is_active_ = false;
    ESP_LOGI(TAG, "系统已恢复正常运行状态");
}

} // namespace ImageResource
