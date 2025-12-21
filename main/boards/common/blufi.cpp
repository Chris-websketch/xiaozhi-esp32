#include "blufi.h"
#include <string>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <new>

#include "application.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_bt.h"
// WifiStation is already included below
// Adapted for WifiStation instead of WifiManager

// Bluedroid specific
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#endif

// NimBLE specific
#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "console/console.h"
#include "store/config/ble_store_config.h"
#endif

extern "C" {
// Blufi Advertising & Connection
void esp_blufi_adv_start(void);

void esp_blufi_adv_stop(void);

void esp_blufi_disconnect(void);

// Internal BTC layer functions needed for error reporting
void btc_blufi_report_error(esp_blufi_error_state_t state);

// Bluedroid specific GAP event handler
#ifdef CONFIG_BT_BLUEDROID_ENABLED
void esp_blufi_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

// NimBLE specific internal functions
#ifdef CONFIG_BT_NIMBLE_ENABLED
void esp_blufi_gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int esp_blufi_gatt_svr_init(void);
void esp_blufi_gatt_svr_deinit(void);
void esp_blufi_btc_init(void);
void esp_blufi_btc_deinit(void);
void ble_store_config_init(void);
#endif
}

// mbedTLS for security
#include "mbedtls/md5.h"
#include "esp_crc.h"
#include "esp_random.h"
#include "ssid_manager.h"
#include <wifi_station.h>

// Logging Tag
static const char *BLUFI_TAG = "BLUFI_CLASS";

static wifi_mode_t GetWifiModeWithFallback() {
    auto& wifi_station = WifiStation::GetInstance();
    if (wifi_station.IsConnected()) {
        return WIFI_MODE_STA;
    }

    wifi_mode_t mode = WIFI_MODE_STA;
    esp_wifi_get_mode(&mode);
    return mode;
}


Blufi &Blufi::GetInstance() {
    static Blufi instance;
    return instance;
}

Blufi::Blufi() : m_sec(nullptr),
                 m_ble_is_connected(false),
                 m_sta_connected(false),
                 m_sta_got_ip(false),
                 m_provisioned(false),
                 m_deinited(false),
                 m_sta_ssid_len(0),
                 m_sta_is_connecting(false) {
    // Initialize member variables
    memset(&m_sta_config, 0, sizeof(m_sta_config));
    memset(&m_ap_config, 0, sizeof(m_ap_config));
    memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
    memset(m_sta_ssid, 0, sizeof(m_sta_ssid));
    memset(&m_sta_conn_info, 0, sizeof(m_sta_conn_info));
}

Blufi::~Blufi() {
    if (m_sec) {
        _security_deinit();
    }
}

esp_err_t Blufi::init() {
    esp_err_t ret;
    m_provisioned = false;
    m_deinited = false;

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = _controller_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif

    ret = _host_and_cb_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "BLUFI host and cb init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(BLUFI_TAG, "BLUFI VERSION %04x", esp_blufi_get_version());
    return ESP_OK;
}

esp_err_t Blufi::deinit() {
    if (m_deinited) {
        return ESP_OK;
    }
    m_deinited = true;
    esp_err_t ret;
    ret = _host_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "Host deinit failed: %s", esp_err_to_name(ret));
    }
#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
    ret = _controller_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "Controller deinit failed: %s", esp_err_to_name(ret));
    }
#endif
    return ret;
}


#ifdef CONFIG_BT_BLUEDROID_ENABLED
esp_err_t Blufi::_host_init() {
    esp_err_t ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s init bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ESP_LOGI(BLUFI_TAG, "BD ADDR: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit() {
    esp_err_t ret = esp_blufi_profile_deinit();
    if (ret != ESP_OK) return ret;

    ret = esp_bluedroid_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    ret = esp_bluedroid_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Blufi::_gap_register_callback() {
    esp_err_t rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if (rc) {
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }
    ret = _gap_register_callback();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s gap register failed, error code = %x", __func__, ret);
        return ret;
    }

    return ESP_OK;
}
#endif /* CONFIG_BT_BLUEDROID_ENABLED */

#ifdef CONFIG_BT_NIMBLE_ENABLED
// Stubs for NimBLE specific store functionality
void ble_store_config_init();

void Blufi::_nimble_on_reset(int reason) {
    ESP_LOGE(BLUFI_TAG, "NimBLE Resetting state; reason=%d", reason);
}

void Blufi::_nimble_on_sync() {
    // This is called when the host and controller are synced.
    // 在调用 esp_blufi_profile_init() 之前设置设备名称，确保广播使用正确的名称
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    char device_name[32];
    snprintf(device_name, sizeof(device_name), "Voxia-%02X%02X%02X", mac[3], mac[4], mac[5]);
    ble_svc_gap_device_name_set(device_name);
    ESP_LOGI(BLUFI_TAG, "BLE device name set to: %s", device_name);
    
    esp_blufi_profile_init();
}

void Blufi::_nimble_host_task(void *param) {
    ESP_LOGI(BLUFI_TAG, "BLE Host Task Started");
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
    nimble_port_freertos_deinit();
}

esp_err_t Blufi::_host_init() {
    // esp_nimble_init() is called by controller_init for NimBLE
    ble_hs_cfg.reset_cb = _nimble_on_reset;
    ble_hs_cfg.sync_cb = _nimble_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;

    // Security Manager settings (can be customized)
    ble_hs_cfg.sm_io_cap = 4; // IO capability: No Input, No Output
#ifdef CONFIG_EXAMPLE_BONDING
ble_hs_cfg.sm_bonding=1;
#endif

int rc = esp_blufi_gatt_svr_init();
assert (rc== 0);

ble_store_config_init(); // Configure the BLE storage
esp_blufi_btc_init();

esp_err_t err = esp_nimble_enable((void*)_nimble_host_task);
    if (err) {
    ESP_LOGE(BLUFI_TAG, "%s failed: %s", __func__, esp_err_to_name(err));
    return ESP_FAIL;
}
    return ESP_OK;
}

esp_err_t Blufi::_host_deinit(void) {
    esp_err_t ret = nimble_port_stop();
    if (ret == ESP_OK) {
        esp_nimble_deinit();
    }
    esp_blufi_gatt_svr_deinit();
    ret = esp_blufi_profile_deinit();
    esp_blufi_btc_deinit();
    return ret;
}

esp_err_t Blufi::_gap_register_callback(void) {
    return ESP_OK; // For NimBLE, GAP callbacks are handled differently
}

esp_err_t Blufi::_host_and_cb_init() {
    static esp_blufi_callbacks_t blufi_callbacks = {
        .event_cb = &_event_callback_trampoline,
        .negotiate_data_handler = &_negotiate_data_handler_trampoline,
        .encrypt_func = &_encrypt_func_trampoline,
        .decrypt_func = &_decrypt_func_trampoline,
        .checksum_func = &_checksum_func_trampoline,
    };

    esp_err_t ret = esp_blufi_register_callbacks(&blufi_callbacks);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s blufi register failed, error code = %x", __func__, ret);
        return ret;
    }

    // Host init must be called after registering callbacks for NimBLE
    ret = _host_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialise host failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
#endif /* CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_BT_CONTROLLER_ENABLED || !CONFIG_BT_NIMBLE_ENABLED
esp_err_t Blufi::_controller_init() {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }

#ifdef CONFIG_BT_NIMBLE_ENABLED
    // For NimBLE, host init needs to be done after controller init
    ret = esp_nimble_init();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "esp_nimble_init() failed: %s", esp_err_to_name(ret));
        return ret;
    }
#endif
    return ESP_OK;
}

esp_err_t Blufi::_controller_deinit() {
    esp_err_t ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s disable controller failed: %s", __func__, esp_err_to_name(ret));
    }
    ret = esp_bt_controller_deinit();
    if (ret) {
        ESP_LOGE(BLUFI_TAG, "%s deinit controller failed: %s", __func__, esp_err_to_name(ret));
    }
    return ret;
}
#endif // Generic controller init


static int myrand(void *rng_state, unsigned char *output, size_t len) {
    esp_fill_random(output, len);
    return 0;
}

void Blufi::_security_init() {
    m_sec = new (std::nothrow) BlufiSecurity();
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate security context");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }
    memset(m_sec, 0, sizeof(BlufiSecurity));
    
    m_sec->dhm = new (std::nothrow) mbedtls_dhm_context();
    if (m_sec->dhm == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate DHM context");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        delete m_sec;
        m_sec = nullptr;
        return;
    }
    
    m_sec->aes = new (std::nothrow) mbedtls_aes_context();
    if (m_sec->aes == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Failed to allocate AES context");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        delete m_sec->dhm;
        delete m_sec;
        m_sec = nullptr;
        return;
    }

    mbedtls_dhm_init(m_sec->dhm);
    mbedtls_aes_init(m_sec->aes);

    memset(m_sec->iv, 0x0, sizeof(m_sec->iv));
}

void Blufi::_security_deinit() {
    if (m_sec == nullptr) return;

    if (m_sec->dh_param) {
        free(m_sec->dh_param);
    }
    mbedtls_dhm_free(m_sec->dhm);
    mbedtls_aes_free(m_sec->aes);
    delete m_sec->dhm;
    delete m_sec->aes;
    delete m_sec;
    m_sec = nullptr;
}

void Blufi::_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len,
                                       bool *need_free) {
    if (m_sec == nullptr) {
        ESP_LOGE(BLUFI_TAG, "Security not initialized in DH handler");
        btc_blufi_report_error(ESP_BLUFI_INIT_SECURITY_ERROR);
        return;
    }

    uint8_t type = data[0];
    switch (type) {
        case 0x00: /* DH_PARAM_LEN */
            m_sec->dh_param_len = (data[1] << 8) | data[2];
            if (m_sec->dh_param) {
                free(m_sec->dh_param);
                m_sec->dh_param = nullptr;
            }
            m_sec->dh_param = (uint8_t *) malloc(m_sec->dh_param_len);
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH malloc failed");
                btc_blufi_report_error(ESP_BLUFI_DH_MALLOC_ERROR);
                return;
            }
            break;
        case 0x01: /* DH_PARAM_DATA */ {
            if (m_sec->dh_param == nullptr) {
                ESP_LOGE(BLUFI_TAG, "DH param not allocated");
                btc_blufi_report_error(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            uint8_t *param = m_sec->dh_param;
            memcpy(m_sec->dh_param, &data[1], m_sec->dh_param_len);
            int ret = mbedtls_dhm_read_params(m_sec->dhm, &param, &param[m_sec->dh_param_len]);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_read_params failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }

            const int dhm_len = mbedtls_dhm_get_len(m_sec->dhm);
            ret = mbedtls_dhm_make_public(m_sec->dhm, dhm_len, m_sec->self_public_key, DH_SELF_PUB_KEY_LEN, myrand,
                                          NULL);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_make_public failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }

            ret = mbedtls_dhm_calc_secret(m_sec->dhm, m_sec->share_key, SHARE_KEY_LEN, &m_sec->share_len, myrand, NULL);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_dhm_calc_secret failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_ENCRYPT_ERROR);
                return;
            }

            ret = mbedtls_md5(m_sec->share_key, m_sec->share_len, m_sec->psk);
            if (ret) {
                ESP_LOGE(BLUFI_TAG, "mbedtls_md5 failed %d", ret);
                btc_blufi_report_error(ESP_BLUFI_CALC_MD5_ERROR);
                return;
            }

            mbedtls_aes_setkey_enc(m_sec->aes, m_sec->psk, PSK_LEN * 8);

            *output_data = &m_sec->self_public_key[0];
            *output_len = dhm_len;
            *need_free = false;

            free(m_sec->dh_param);
            m_sec->dh_param = NULL;
        }
        break;
        default:
            ESP_LOGE(BLUFI_TAG, "DH handler unknown type: %d", type);
    }
}

int Blufi::_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    if (!m_sec) return -1;
    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    int ret = mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_ENCRYPT, crypt_len, &iv_offset, iv0, crypt_data,
                                    crypt_data);
    // ESP-IDF BluFi 期望返回处理的数据长度，而不是错误码
    return (ret == 0) ? crypt_len : ret;
}

int Blufi::_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    if (!m_sec) return -1;
    size_t iv_offset = 0;
    uint8_t iv0[16];
    memcpy(iv0, m_sec->iv, 16);
    iv0[0] = iv8;
    int ret = mbedtls_aes_crypt_cfb128(m_sec->aes, MBEDTLS_AES_DECRYPT, crypt_len, &iv_offset, iv0, crypt_data,
                                    crypt_data);
    // ESP-IDF BluFi 期望返回处理的数据长度，而不是错误码
    return (ret == 0) ? crypt_len : ret;
}

uint16_t Blufi::_crc_checksum(uint8_t iv8, uint8_t *data, int len) {
    return esp_crc16_be(0, data, len);
}


int Blufi::_get_softap_conn_num() {
    wifi_sta_list_t sta_list{};
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

void Blufi::_handle_event(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(BLUFI_TAG, "BLUFI init finish");
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble connect");
            m_ble_is_connected = true;
            esp_blufi_adv_stop();
            _security_init();
            break;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(BLUFI_TAG, "BLUFI ble disconnect");
            m_ble_is_connected = false;
            _security_deinit();
            if (!m_provisioned) {
                esp_blufi_adv_start();
            } else {
                esp_blufi_adv_stop();
                if (!m_deinited) {
                    // Deinit BLE stack after provisioning completes to free resources.
                    xTaskCreate([](void *ctx) {
                        static_cast<Blufi *>(ctx)->deinit();
                        vTaskDelete(nullptr);
                    }, "blufi_deinit", 4096, this, 5, nullptr);
                }
            }
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE: {
            ESP_LOGI(BLUFI_TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            // Opmode change is handled by the application after credentials are received
            break;
        }
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP: {
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi connect to AP");
            std::string ssid(reinterpret_cast<const char *>(m_sta_config.sta.ssid));
            std::string password(reinterpret_cast<const char *>(m_sta_config.sta.password));

            // Save credentials through SsidManager
            SsidManager::GetInstance().AddSsid(ssid, password);

            // Track SSID for BLUFI status reporting.
            m_sta_ssid_len = static_cast<int>(std::min(ssid.size(), sizeof(m_sta_ssid)));
            memcpy(m_sta_ssid, ssid.c_str(), m_sta_ssid_len);
            memset(m_sta_bssid, 0, sizeof(m_sta_bssid));
            m_sta_connected = false;
            m_sta_got_ip = false;
            m_sta_is_connecting = true;
            m_sta_conn_info = {}; // Reset connection info
            m_sta_conn_info.sta_ssid = m_sta_ssid;
            m_sta_conn_info.sta_ssid_len = m_sta_ssid_len;
            m_provisioned = true;

            // 先通知小程序配置已保存，然后关闭蓝牙再启动WiFi（避免内存不足）
            wifi_mode_t mode = GetWifiModeWithFallback();
            esp_blufi_extra_info_t info = {};
            info.sta_ssid = m_sta_ssid;
            info.sta_ssid_len = m_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
            ESP_LOGI(BLUFI_TAG, "BluFi: config saved, notified client, will restart to connect WiFi...");

            // 延迟后重启设备，重启后会自动连接WiFi
            xTaskCreate([](void *ctx) {
                vTaskDelay(pdMS_TO_TICKS(500));  // 等待通知发送完成
                ESP_LOGI(BLUFI_TAG, "BluFi: config saved, restarting to connect WiFi...");
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
            }, "blufi_restart", 2048, nullptr, 5, nullptr);
            break;
        }
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(BLUFI_TAG, "BLUFI request wifi disconnect from AP");
            WifiStation::GetInstance().Stop();
            m_sta_is_connecting = false;
            m_sta_connected = false;
            m_sta_got_ip = false;
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
            auto &wifi = WifiStation::GetInstance();
            wifi_mode_t mode = GetWifiModeWithFallback();
            const int softap_conn_num = _get_softap_conn_num();

            if (wifi.IsConnected()) {
                m_sta_connected = true;
                m_sta_got_ip = true;

                auto current_ssid = wifi.GetSsid();
                if (!current_ssid.empty()) {
                    m_sta_ssid_len = static_cast<int>(std::min(current_ssid.size(), sizeof(m_sta_ssid)));
                    memcpy(m_sta_ssid, current_ssid.c_str(), m_sta_ssid_len);
                }

                esp_blufi_extra_info_t info;
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                memcpy(info.sta_bssid, m_sta_bssid, 6);
                info.sta_ssid = m_sta_ssid;
                info.sta_ssid_len = m_sta_ssid_len;
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_conn_num, &info);
            } else if (m_sta_is_connecting) {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_conn_num, &m_sta_conn_info);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_conn_num, &m_sta_conn_info);
            }
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi status");
            break;
        }
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(m_sta_config.sta.bssid, param->sta_bssid.bssid, 6);
            m_sta_config.sta.bssid_set = true;
            ESP_LOGI(BLUFI_TAG, "Recv STA BSSID");
            break;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy((char *) m_sta_config.sta.ssid, (char *) param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            m_sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA SSID: %s", m_sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char *) m_sta_config.sta.password, (char *) param->sta_passwd.passwd,
                    param->sta_passwd.passwd_len);
            m_sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            ESP_LOGI(BLUFI_TAG, "Recv STA PASSWORD");
            break;
        case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
            ESP_LOGI(BLUFI_TAG, "Recv slave disconnect BLE");
            esp_blufi_disconnect();
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
            ESP_LOGI(BLUFI_TAG, "BLUFI get wifi list");
            // 确保WiFi已初始化（STA模式用于扫描）
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            esp_err_t err = esp_wifi_init(&cfg);
            if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) {
                ESP_LOGE(BLUFI_TAG, "WiFi init failed: %s", esp_err_to_name(err));
                esp_blufi_send_wifi_list(0, nullptr);
                break;
            }
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
            
            // 扫描周围WiFi并发送给小程序
            wifi_scan_config_t scan_config = {};
            scan_config.show_hidden = false;
            scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
            scan_config.scan_time.active.min = 100;
            scan_config.scan_time.active.max = 300;
            
            err = esp_wifi_scan_start(&scan_config, true);
            if (err != ESP_OK) {
                ESP_LOGE(BLUFI_TAG, "WiFi scan failed: %s", esp_err_to_name(err));
                esp_blufi_send_wifi_list(0, nullptr);
                break;
            }
            
            uint16_t ap_count = 0;
            esp_wifi_scan_get_ap_num(&ap_count);
            if (ap_count == 0) {
                ESP_LOGI(BLUFI_TAG, "No WiFi AP found");
                esp_blufi_send_wifi_list(0, nullptr);
                break;
            }
            
            // 限制最多20个AP
            if (ap_count > 20) ap_count = 20;
            
            wifi_ap_record_t *ap_list = new (std::nothrow) wifi_ap_record_t[ap_count];
            if (ap_list == nullptr) {
                ESP_LOGE(BLUFI_TAG, "Failed to allocate memory for AP list");
                esp_blufi_send_wifi_list(0, nullptr);
                break;
            }
            
            esp_wifi_scan_get_ap_records(&ap_count, ap_list);
            
            // 转换为BluFi格式
            esp_blufi_ap_record_t *blufi_ap_list = new (std::nothrow) esp_blufi_ap_record_t[ap_count];
            if (blufi_ap_list == nullptr) {
                ESP_LOGE(BLUFI_TAG, "Failed to allocate memory for BluFi AP list");
                delete[] ap_list;
                esp_blufi_send_wifi_list(0, nullptr);
                break;
            }
            
            for (uint16_t i = 0; i < ap_count; i++) {
                memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
                blufi_ap_list[i].rssi = ap_list[i].rssi;
            }
            
            ESP_LOGI(BLUFI_TAG, "Sending %d WiFi APs to client", ap_count);
            esp_blufi_send_wifi_list(ap_count, blufi_ap_list);
            
            delete[] ap_list;
            delete[] blufi_ap_list;
            
            // 扫描完成后释放WiFi资源以节省内存
            esp_wifi_stop();
            esp_wifi_deinit();
            ESP_LOGI(BLUFI_TAG, "WiFi resources released after scan");
            break;
        }
        default:
            ESP_LOGW(BLUFI_TAG, "Unhandled event: %d", event);
            break;
    }
}


void Blufi::_event_callback_trampoline(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    GetInstance()._handle_event(event, param);
}

void Blufi::_negotiate_data_handler_trampoline(uint8_t *data, int len, uint8_t **output_data, int *output_len,
                                               bool *need_free) {
    GetInstance()._dh_negotiate_data_handler(data, len, output_data, output_len, need_free);
}

int Blufi::_encrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    return GetInstance()._aes_encrypt(iv8, crypt_data, crypt_len);
}

int Blufi::_decrypt_func_trampoline(uint8_t iv8, uint8_t *crypt_data, int crypt_len) {
    return GetInstance()._aes_decrypt(iv8, crypt_data, crypt_len);
}

uint16_t Blufi::_checksum_func_trampoline(uint8_t iv8, uint8_t *data, int len) {
    return _crc_checksum(iv8, data, len);
}
