#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

class PowerSaveTimer {
public:
    PowerSaveTimer(int cpu_max_freq, int seconds_to_clock = -1, int seconds_to_dim = -1, int seconds_to_shutdown = -1);
    ~PowerSaveTimer();

    void SetEnabled(bool enabled);
    void OnEnterClockMode(std::function<void()> callback);
    void OnExitClockMode(std::function<void()> callback);
    void OnEnterDimMode(std::function<void()> callback);
    void OnExitDimMode(std::function<void()> callback);
    void OnEnterSleepMode(std::function<void()> callback);
    void OnExitSleepMode(std::function<void()> callback);
    void OnShutdownRequest(std::function<void()> callback);
    void WakeUp();

private:
    void PowerSaveCheck();

    esp_timer_handle_t power_save_timer_ = nullptr;
    bool enabled_ = false;
    bool in_clock_mode_ = false;
    bool in_dim_mode_ = false;
    bool in_sleep_mode_ = false;
    int ticks_ = 0;
    int cpu_max_freq_;
    int seconds_to_clock_;
    int seconds_to_dim_;
    int seconds_to_shutdown_;

    std::function<void()> on_enter_clock_mode_;
    std::function<void()> on_exit_clock_mode_;
    std::function<void()> on_enter_dim_mode_;
    std::function<void()> on_exit_dim_mode_;
    std::function<void()> on_enter_sleep_mode_;
    std::function<void()> on_exit_sleep_mode_;
    std::function<void()> on_shutdown_request_;
};
