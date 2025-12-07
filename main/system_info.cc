#include "system_info.h"
#include "boards/common/board.h"
#include "boards/common/wifi_board.h"

#include <algorithm>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_flash.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cJSON.h>
#include <wifi_station.h>
#include <map>


#define TAG "SystemInfo"

// 国家代码到时区偏移的映射表（相对于UTC的小时数）
// 对于跨多个时区的国家，使用首都或主要城市所在时区
static const std::map<std::string, int> COUNTRY_TIMEZONE_MAP = {
    // === 亚洲 ===
    {"CN", 8},   // 中国 UTC+8
    {"JP", 9},   // 日本 UTC+9
    {"KR", 9},   // 韩国 UTC+9
    {"IN", 5},   // 印度 UTC+5:30 (简化为5)
    {"SG", 8},   // 新加坡 UTC+8
    {"TH", 7},   // 泰国 UTC+7
    {"VN", 7},   // 越南 UTC+7
    {"MY", 8},   // 马来西亚 UTC+8
    {"ID", 7},   // 印尼西部 UTC+7
    {"PH", 8},   // 菲律宾 UTC+8
    {"BD", 6},   // 孟加拉国 UTC+6
    {"PK", 5},   // 巴基斯坦 UTC+5
    {"AF", 4},   // 阿富汗 UTC+4:30 (简化为4)
    {"IR", 3},   // 伊朗 UTC+3:30 (简化为3)
    {"IQ", 3},   // 伊拉克 UTC+3
    {"SY", 2},   // 叙利亚 UTC+2
    {"JO", 2},   // 约旦 UTC+2
    {"LB", 2},   // 黎巴嫩 UTC+2
    {"IL", 2},   // 以色列 UTC+2
    {"PS", 2},   // 巴勒斯坦 UTC+2
    {"AM", 4},   // 亚美尼亚 UTC+4
    {"AZ", 4},   // 阿塞拜疆 UTC+4
    {"GE", 4},   // 格鲁吉亚 UTC+4
    {"KZ", 6},   // 哈萨克斯坦东部 UTC+6
    {"UZ", 5},   // 乌兹别克斯坦 UTC+5
    {"TM", 5},   // 土库曼斯坦 UTC+5
    {"TJ", 5},   // 塔吉克斯坦 UTC+5
    {"KG", 6},   // 吉尔吉斯斯坦 UTC+6
    {"MN", 8},   // 蒙古 UTC+8
    {"KH", 7},   // 柬埔寨 UTC+7
    {"LA", 7},   // 老挝 UTC+7
    {"MM", 6},   // 缅甸 UTC+6:30 (简化为6)
    {"LK", 5},   // 斯里兰卡 UTC+5:30 (简化为5)
    {"MV", 5},   // 马尔代夫 UTC+5
    {"NP", 5},   // 尼泊尔 UTC+5:45 (简化为5)
    {"BT", 6},   // 不丹 UTC+6
    {"BN", 8},   // 文莱 UTC+8
    {"TL", 9},   // 东帝汶 UTC+9
    {"MO", 8},   // 澳门 UTC+8
    {"HK", 8},   // 香港 UTC+8
    {"TW", 8},   // 台湾 UTC+8
    {"KP", 9},   // 朝鲜 UTC+9

    // === 中东 ===
    {"TR", 3},   // 土耳其 UTC+3
    {"AE", 4},   // 阿联酋 UTC+4
    {"SA", 3},   // 沙特阿拉伯 UTC+3
    {"QA", 3},   // 卡塔尔 UTC+3
    {"BH", 3},   // 巴林 UTC+3
    {"KW", 3},   // 科威特 UTC+3
    {"OM", 4},   // 阿曼 UTC+4
    {"YE", 3},   // 也门 UTC+3

    // === 欧洲 ===
    {"GB", 0},   // 英国 UTC+0
    {"IE", 0},   // 爱尔兰 UTC+0
    {"IS", 0},   // 冰岛 UTC+0
    {"PT", 0},   // 葡萄牙 UTC+0
    {"ES", 1},   // 西班牙 UTC+1
    {"FR", 1},   // 法国 UTC+1
    {"DE", 1},   // 德国 UTC+1
    {"IT", 1},   // 意大利 UTC+1
    {"CH", 1},   // 瑞士 UTC+1
    {"AT", 1},   // 奥地利 UTC+1
    {"BE", 1},   // 比利时 UTC+1
    {"NL", 1},   // 荷兰 UTC+1
    {"LU", 1},   // 卢森堡 UTC+1
    {"DK", 1},   // 丹麦 UTC+1
    {"NO", 1},   // 挪威 UTC+1
    {"SE", 1},   // 瑞典 UTC+1
    {"FI", 2},   // 芬兰 UTC+2
    {"PL", 1},   // 波兰 UTC+1
    {"CZ", 1},   // 捷克 UTC+1
    {"SK", 1},   // 斯洛伐克 UTC+1
    {"HU", 1},   // 匈牙利 UTC+1
    {"SI", 1},   // 斯洛文尼亚 UTC+1
    {"HR", 1},   // 克罗地亚 UTC+1
    {"BA", 1},   // 波黑 UTC+1
    {"RS", 1},   // 塞尔维亚 UTC+1
    {"ME", 1},   // 黑山 UTC+1
    {"MK", 1},   // 北马其顿 UTC+1
    {"AL", 1},   // 阿尔巴尼亚 UTC+1
    {"XK", 1},   // 科索沃 UTC+1
    {"GR", 2},   // 希腊 UTC+2
    {"BG", 2},   // 保加利亚 UTC+2
    {"RO", 2},   // 罗马尼亚 UTC+2
    {"MD", 2},   // 摩尔多瓦 UTC+2
    {"UA", 2},   // 乌克兰 UTC+2
    {"BY", 3},   // 白俄罗斯 UTC+3
    {"LT", 2},   // 立陶宛 UTC+2
    {"LV", 2},   // 拉脱维亚 UTC+2
    {"EE", 2},   // 爱沙尼亚 UTC+2
    {"CY", 2},   // 塞浦路斯 UTC+2
    {"MT", 1},   // 马耳他 UTC+1
    {"RU", 3},   // 俄罗斯莫斯科 UTC+3
    {"MC", 1},   // 摩纳哥 UTC+1
    {"AD", 1},   // 安道尔 UTC+1
    {"SM", 1},   // 圣马力诺 UTC+1
    {"VA", 1},   // 梵蒂冈 UTC+1
    {"LI", 1},   // 列支敦士登 UTC+1

    // === 非洲 ===
    {"EG", 2},   // 埃及 UTC+2
    {"LY", 2},   // 利比亚 UTC+2
    {"TN", 1},   // 突尼斯 UTC+1
    {"DZ", 1},   // 阿尔及利亚 UTC+1
    {"MA", 1},   // 摩洛哥 UTC+1
    {"SD", 2},   // 苏丹 UTC+2
    {"SS", 3},   // 南苏丹 UTC+3
    {"ET", 3},   // 埃塞俄比亚 UTC+3
    {"ER", 3},   // 厄立特里亚 UTC+3
    {"DJ", 3},   // 吉布提 UTC+3
    {"SO", 3},   // 索马里 UTC+3
    {"KE", 3},   // 肯尼亚 UTC+3
    {"UG", 3},   // 乌干达 UTC+3
    {"TZ", 3},   // 坦桑尼亚 UTC+3
    {"RW", 2},   // 卢旺达 UTC+2
    {"BI", 2},   // 布隆迪 UTC+2
    {"MG", 3},   // 马达加斯加 UTC+3
    {"MU", 4},   // 毛里求斯 UTC+4
    {"SC", 4},   // 塞舌尔 UTC+4
    {"KM", 3},   // 科摩罗 UTC+3
    {"MZ", 2},   // 莫桑比克 UTC+2
    {"ZW", 2},   // 津巴布韦 UTC+2
    {"ZM", 2},   // 赞比亚 UTC+2
    {"MW", 2},   // 马拉维 UTC+2
    {"BW", 2},   // 博茨瓦纳 UTC+2
    {"NA", 2},   // 纳米比亚 UTC+2
    {"ZA", 2},   // 南非 UTC+2
    {"LS", 2},   // 莱索托 UTC+2
    {"SZ", 2},   // 斯威士兰 UTC+2
    {"AO", 1},   // 安哥拉 UTC+1
    {"CD", 1},   // 刚果民主共和国西部 UTC+1
    {"CG", 1},   // 刚果共和国 UTC+1
    {"CF", 1},   // 中非 UTC+1
    {"CM", 1},   // 喀麦隆 UTC+1
    {"TD", 1},   // 乍得 UTC+1
    {"NE", 1},   // 尼日尔 UTC+1
    {"NG", 1},   // 尼日利亚 UTC+1
    {"BJ", 1},   // 贝宁 UTC+1
    {"TG", 0},   // 多哥 UTC+0
    {"GH", 0},   // 加纳 UTC+0
    {"CI", 0},   // 科特迪瓦 UTC+0
    {"LR", 0},   // 利比里亚 UTC+0
    {"SL", 0},   // 塞拉利昂 UTC+0
    {"GN", 0},   // 几内亚 UTC+0
    {"GW", 0},   // 几内亚比绍 UTC+0
    {"SN", 0},   // 塞内加尔 UTC+0
    {"GM", 0},   // 冈比亚 UTC+0
    {"ML", 0},   // 马里 UTC+0
    {"BF", 0},   // 布基纳法索 UTC+0
    {"MR", 0},   // 毛里塔尼亚 UTC+0
    {"CV", -1},  // 佛得角 UTC-1
    {"GA", 1},   // 加蓬 UTC+1
    {"GQ", 1},   // 赤道几内亚 UTC+1
    {"ST", 0},   // 圣多美和普林西比 UTC+0

    // === 北美洲 ===
    {"US", -5},  // 美国东部时间 UTC-5
    {"CA", -5},  // 加拿大东部 UTC-5
    {"MX", -6},  // 墨西哥 UTC-6
    {"GT", -6},  // 危地马拉 UTC-6
    {"BZ", -6},  // 伯利兹 UTC-6
    {"SV", -6},  // 萨尔瓦多 UTC-6
    {"HN", -6},  // 洪都拉斯 UTC-6
    {"NI", -6},  // 尼加拉瓜 UTC-6
    {"CR", -6},  // 哥斯达黎加 UTC-6
    {"PA", -5},  // 巴拿马 UTC-5
    {"CU", -5},  // 古巴 UTC-5
    {"JM", -5},  // 牙买加 UTC-5
    {"HT", -5},  // 海地 UTC-5
    {"DO", -4},  // 多米尼加 UTC-4
    {"PR", -4},  // 波多黎各 UTC-4
    {"BS", -5},  // 巴哈马 UTC-5
    {"BB", -4},  // 巴巴多斯 UTC-4
    {"TT", -4},  // 特立尼达和多巴哥 UTC-4
    {"GD", -4},  // 格林纳达 UTC-4
    {"VC", -4},  // 圣文森特和格林纳丁斯 UTC-4
    {"LC", -4},  // 圣卢西亚 UTC-4
    {"DM", -4},  // 多米尼克 UTC-4
    {"AG", -4},  // 安提瓜和巴布达 UTC-4
    {"KN", -4},  // 圣基茨和尼维斯 UTC-4

    // === 南美洲 ===
    {"BR", -3},  // 巴西 UTC-3
    {"AR", -3},  // 阿根廷 UTC-3
    {"CL", -4},  // 智利 UTC-4
    {"PE", -5},  // 秘鲁 UTC-5
    {"EC", -5},  // 厄瓜多尔 UTC-5
    {"CO", -5},  // 哥伦比亚 UTC-5
    {"VE", -4},  // 委内瑞拉 UTC-4
    {"GY", -4},  // 圭亚那 UTC-4
    {"SR", -3},  // 苏里南 UTC-3
    {"BO", -4},  // 玻利维亚 UTC-4
    {"PY", -3},  // 巴拉圭 UTC-3
    {"UY", -3},  // 乌拉圭 UTC-3
    {"FK", -3},  // 福克兰群岛 UTC-3
    {"GS", -2},  // 南乔治亚岛 UTC-2

    // === 大洋洲 ===
    {"AU", 10},  // 澳大利亚东部 UTC+10
    {"NZ", 12},  // 新西兰 UTC+12
    {"FJ", 12},  // 斐济 UTC+12
    {"PG", 10},  // 巴布亚新几内亚 UTC+10
    {"SB", 11},  // 所罗门群岛 UTC+11
    {"VU", 11},  // 瓦努阿图 UTC+11
    {"NC", 11},  // 新喀里多尼亚 UTC+11
    {"PF", -10}, // 法属波利尼西亚 UTC-10
    {"WS", 13},  // 萨摩亚 UTC+13
    {"TO", 13},  // 汤加 UTC+13
    {"TV", 12},  // 图瓦卢 UTC+12
    {"KI", 12},  // 基里巴斯东部 UTC+12
    {"NR", 12},  // 瑙鲁 UTC+12
    {"MH", 12},  // 马绍尔群岛 UTC+12
    {"FM", 10},  // 密克罗尼西亚 UTC+10
    {"PW", 9},   // 帕劳 UTC+9
    {"GU", 10},  // 关岛 UTC+10
    {"MP", 10},  // 北马里亚纳群岛 UTC+10
    {"AS", -11}, // 美属萨摩亚 UTC-11
    {"CK", -10}, // 库克群岛 UTC-10
    {"NU", -11}, // 纽埃 UTC-11
    {"TK", 13},  // 托克劳 UTC+13
    {"PN", -8},  // 皮特凯恩群岛 UTC-8

    // === 南极洲 ===
    {"AQ", 0},   // 南极洲 UTC+0 (默认)
};

size_t SystemInfo::GetFlashSize() {
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get flash size");
        return 0;
    }
    return (size_t)flash_size;
}

size_t SystemInfo::GetMinimumFreeHeapSize() {
    return esp_get_minimum_free_heap_size();
}

size_t SystemInfo::GetFreeHeapSize() {
    return esp_get_free_heap_size();
}

std::string SystemInfo::GetMacAddress() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}

std::string SystemInfo::GetClientId() {
    // 尝试从NVS读取存储的Client-Id
    std::string client_id;
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("websocket", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = 0;
        err = nvs_get_str(nvs_handle, "client_id", NULL, &required_size);
        if (err == ESP_OK && required_size > 0) {
            char* client_id_buf = (char*)malloc(required_size);
            if (client_id_buf != NULL) {
                err = nvs_get_str(nvs_handle, "client_id", client_id_buf, &required_size);
                if (err == ESP_OK) {
                    client_id = client_id_buf;
                    ESP_LOGI(TAG, "Client-Id loaded from NVS: %s", client_id.c_str());
                }
                free(client_id_buf);
            }
        }
        
        // 只有在NVS中没有Client-Id时，才尝试从配置中获取并存储
        if (client_id.empty()) {
            ESP_LOGI(TAG, "No Client-Id found in NVS, checking configuration...");
#ifdef CONFIG_WEBSOCKET_CLIENT_ID
            std::string config_client_id = CONFIG_WEBSOCKET_CLIENT_ID;
            if (!config_client_id.empty()) {
                ESP_LOGI(TAG, "Found Client-Id in configuration: %s", config_client_id.c_str());
                err = nvs_set_str(nvs_handle, "client_id", config_client_id.c_str());
                if (err == ESP_OK) {
                    err = nvs_commit(nvs_handle);
                    if (err == ESP_OK) {
                        client_id = config_client_id;
                        ESP_LOGI(TAG, "Client-Id stored to NVS from configuration: %s", client_id.c_str());
                    } else {
                        ESP_LOGE(TAG, "Failed to commit client_id to NVS: %s", esp_err_to_name(err));
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to set client_id in NVS: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGW(TAG, "CONFIG_WEBSOCKET_CLIENT_ID is empty");
            }
#else
            ESP_LOGW(TAG, "CONFIG_WEBSOCKET_CLIENT_ID not defined in this firmware");
#endif
        }
        
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS for client_id: %s", esp_err_to_name(err));
    }
    
    if (client_id.empty()) {
        ESP_LOGW(TAG, "No Client-Id available, will use Board UUID as fallback");
    }
    
    return client_id;
}

std::string SystemInfo::GetChipModelName() {
    return std::string(CONFIG_IDF_TARGET);
}

esp_err_t SystemInfo::PrintRealTimeStats(TickType_t xTicksToWait) {
    #define ARRAY_SIZE_OFFSET 5
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
    esp_err_t ret;
    uint32_t total_elapsed_time;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = (TaskStatus_t*)malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Percentage\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
            printf("| %-16s | %8lu | %4lu%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

GeoLocationInfo SystemInfo::GetCountryInfo() {
    // 全局缓存地理位置信息，避免重复API调用
    static GeoLocationInfo cached_result;
    static bool cache_initialized = false;
    static uint32_t last_request_time = 0;
    static uint32_t last_failure_time = 0;
    static int failure_count = 0;
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 如果已有有效缓存且在5分钟内，直接返回缓存
    if (cache_initialized && cached_result.is_valid && 
        (current_time - last_request_time) < 300000) { // 5分钟缓存
        ESP_LOGD(TAG, "Returning cached geolocation info for country %s", 
                 cached_result.country_code.c_str());
        return cached_result;
    }
    
    // 如果最近有失败记录，短期内跳过请求（避免反复尝试阻塞）
    // 失败次数越多，等待时间越长：30秒 * failure_count，最长5分钟
    uint32_t skip_duration = std::min(30000u * failure_count, 300000u);
    if (failure_count > 0 && (current_time - last_failure_time) < skip_duration) {
        ESP_LOGD(TAG, "Skipping geolocation request due to recent failure (retry in %lu ms)", 
                 skip_duration - (current_time - last_failure_time));
        return cached_result; // 返回空结果或上次缓存
    }
    
    // 如果正在请求中（防止并发请求），返回缓存的结果
    static bool requesting = false;
    if (requesting) {
        ESP_LOGD(TAG, "Geolocation request in progress, returning cached result");
        return cached_result;
    }
    
    requesting = true;
    GeoLocationInfo result;
    
    // 检查WiFi连接状态
    if (!WifiStation::GetInstance().IsConnected()) {
        ESP_LOGW(TAG, "WiFi not connected, cannot get geolocation info");
        requesting = false;
        return result;
    }
    
    ESP_LOGI(TAG, "Requesting geolocation info from ipinfo.io...");
    
    // 创建HTTP客户端
    auto http = Board::GetInstance().CreateHttp();
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client for geolocation, skipping");
        failure_count++;
        last_failure_time = current_time;
        requesting = false;
        return result;
    }
    
    // 设置请求头
    auto app_desc = esp_app_get_description();
    http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    http->SetHeader("Accept", "application/json");
    http->SetHeader("Connection", "close");
    
    // 发送请求到ipinfo.io
    const char* ipinfo_url = "https://ipinfo.io/json";
    if (!http->Open("GET", ipinfo_url)) {
        ESP_LOGW(TAG, "Failed to open connection to ipinfo.io, skipping IP check");
        delete http;
        failure_count++;
        last_failure_time = current_time;
        requesting = false;
        return result;
    }
    
    // 获取响应
    std::string response = http->GetBody();
    http->Close();
    delete http;
    
    if (response.empty()) {
        ESP_LOGW(TAG, "Empty response from ipinfo.io, skipping IP check");
        failure_count++;
        last_failure_time = current_time;
        requesting = false;
        return result;
    }
    
    ESP_LOGI(TAG, "Geolocation response: %s", response.c_str());
    
    // 解析JSON响应
    cJSON* root = cJSON_Parse(response.c_str());
    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse geolocation JSON response, skipping IP check");
        failure_count++;
        last_failure_time = current_time;
        requesting = false;
        return result;
    }
    
    // 提取IP地址
    cJSON* ip = cJSON_GetObjectItem(root, "ip");
    if (ip && cJSON_IsString(ip)) {
        result.ip_address = ip->valuestring;
    }
    
    // 提取国家代码
    cJSON* country = cJSON_GetObjectItem(root, "country");
    if (country && cJSON_IsString(country)) {
        result.country_code = country->valuestring;
    }
    
    // 可选：提取国家名称（ipinfo.io的免费API可能不包含此字段）
    cJSON* country_name = cJSON_GetObjectItem(root, "country_name");
    if (country_name && cJSON_IsString(country_name)) {
        result.country_name = country_name->valuestring;
    }
    
    // 清理JSON对象
    cJSON_Delete(root);
    
    // 验证结果
    if (!result.ip_address.empty() && !result.country_code.empty()) {
        result.is_valid = true;
        // 获取并填充时区偏移信息
        result.timezone_offset = GetTimezoneOffset(result.country_code);
        ESP_LOGI(TAG, "Geolocation info: IP=%s, Country=%s, Timezone=UTC%+d", 
                 result.ip_address.c_str(), result.country_code.c_str(), result.timezone_offset);
        
        // 更新缓存
        cached_result = result;
        cache_initialized = true;
        last_request_time = current_time;
        // 成功后重置失败计数
        failure_count = 0;
    } else {
        ESP_LOGW(TAG, "Incomplete geolocation data received, skipping IP check");
        failure_count++;
        last_failure_time = current_time;
    }
    
    requesting = false;
    return result;
}

int SystemInfo::GetTimezoneOffset(const std::string& country_code) {
    auto it = COUNTRY_TIMEZONE_MAP.find(country_code);
    if (it != COUNTRY_TIMEZONE_MAP.end()) {
        ESP_LOGI(TAG, "Found timezone offset for country %s: UTC%+d", 
                 country_code.c_str(), it->second);
        return it->second;
    }
    
    ESP_LOGW(TAG, "Unknown country code %s, using default Beijing timezone UTC+8", 
             country_code.c_str());
    return 8; // 默认北京时间UTC+8
}

struct tm SystemInfo::ConvertFromBeijingTime(const struct tm& beijing_time, int target_timezone_offset) {
    struct tm result = beijing_time;
    
    // 计算时区差异（目标时区 - 北京时区UTC+8）
    int timezone_diff = target_timezone_offset - 8;
    
    ESP_LOGD(TAG, "Converting time from Beijing (UTC+8) to UTC%+d, difference: %+d hours", 
             target_timezone_offset, timezone_diff);
    
    // 如果时区相同，直接返回
    if (timezone_diff == 0) {
        return result;
    }
    
    // 转换为time_t进行计算
    time_t beijing_timestamp = mktime(&result);
    if (beijing_timestamp == -1) {
        ESP_LOGE(TAG, "Failed to convert beijing_time to timestamp");
        return result; // 返回原始时间
    }
    
    // 加上时区差异（秒数）
    beijing_timestamp += timezone_diff * 3600;
    
    // 转换回struct tm
    localtime_r(&beijing_timestamp, &result);
    
    ESP_LOGD(TAG, "Converted time: %04d-%02d-%02d %02d:%02d:%02d", 
             result.tm_year + 1900, result.tm_mon + 1, result.tm_mday,
             result.tm_hour, result.tm_min, result.tm_sec);
    
    return result;
}

