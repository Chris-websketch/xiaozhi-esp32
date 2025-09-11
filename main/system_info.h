#ifndef _SYSTEM_INFO_H_
#define _SYSTEM_INFO_H_

#include <string>

#include <esp_err.h>
#include <freertos/FreeRTOS.h>

struct GeoLocationInfo {
    std::string country_code;    // 国家代码，如"CN"、"US"
    std::string country_name;    // 国家名称，如"China"、"United States"
    std::string ip_address;      // 公网IP地址
    int timezone_offset;         // 时区偏移量（相对于UTC的小时数）
    bool is_valid;              // 数据是否有效
    
    GeoLocationInfo() : country_code(""), country_name(""), ip_address(""), timezone_offset(8), is_valid(false) {} // 默认北京时间UTC+8
};

class SystemInfo {
public:
    static size_t GetFlashSize();
    static size_t GetMinimumFreeHeapSize();
    static size_t GetFreeHeapSize();
    static std::string GetMacAddress();
    static std::string GetChipModelName();
    static esp_err_t PrintRealTimeStats(TickType_t xTicksToWait);
    static std::string GetClientId();
    static GeoLocationInfo GetCountryInfo();
    static struct tm ConvertFromBeijingTime(const struct tm& beijing_time, int target_timezone_offset);
    static int GetTimezoneOffset(const std::string& country_code);
};

#endif // _SYSTEM_INFO_H_
