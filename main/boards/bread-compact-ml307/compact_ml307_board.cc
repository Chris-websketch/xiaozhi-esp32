#include "ml307_board.h"
#include "system_reset.h"
#include "audio_device.h"

#include <esp_log.h>

#define TAG "CompactMl307Board"

class CompactMl307Board : public Ml307Board {
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactMl307Board");
        // Check if the reset button is pressed
        SystemReset::GetInstance().CheckButtons();

        Ml307Board::Initialize();
    }

    virtual AudioDevice* GetAudioDevice() override {
        static AudioDevice audio_device;
        return &audio_device;
    }
};

DECLARE_BOARD(CompactMl307Board);
