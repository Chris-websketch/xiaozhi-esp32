#include "esp_mqtt.h"
#include <esp_crt_bundle.h>
#include <esp_log.h>

static const char *TAG = "esp_mqtt";

EspMqtt::EspMqtt() {
    event_group_handle_ = xEventGroupCreate();
}

EspMqtt::~EspMqtt() {
    if (event_group_handle_ != nullptr) {
        Disconnect();
    }

    vEventGroupDelete(event_group_handle_);
}

bool EspMqtt::Connect(const std::string broker_address, int broker_port, const std::string client_id, const std::string username, const std::string password) {
    if (mqtt_client_handle_ != nullptr) {
        Disconnect();
    }

    esp_mqtt_client_config_t mqtt_config = {};
    mqtt_config.broker.address.hostname = broker_address.c_str();
    mqtt_config.broker.address.port = broker_port;
    if (broker_port == 8883) {
        mqtt_config.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
        mqtt_config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    } else {
        mqtt_config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    }
    mqtt_config.credentials.client_id = client_id.c_str();
    mqtt_config.credentials.username = username.c_str();
    mqtt_config.credentials.authentication.password = password.c_str();
    mqtt_config.session.keepalive = keep_alive_seconds_;

    // 配置LWT（遗嘱消息）- 设备异常断线时Broker自动发布
    if (lwt_enabled_) {
        mqtt_config.session.last_will.topic = lwt_topic_.c_str();
        mqtt_config.session.last_will.msg = lwt_message_.c_str();
        mqtt_config.session.last_will.msg_len = lwt_message_.length();
        mqtt_config.session.last_will.qos = lwt_qos_;
        mqtt_config.session.last_will.retain = lwt_retain_ ? 1 : 0;
        ESP_LOGI(TAG, "LWT configured: topic=%s, qos=%d, retain=%d", 
                 lwt_topic_.c_str(), lwt_qos_, lwt_retain_);
    }

    mqtt_client_handle_ = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqtt_client_handle_, MQTT_EVENT_ANY, [](void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
        ((EspMqtt*)handler_args)->MqttEventCallback(base, event_id, event_data);
    }, this);
    esp_mqtt_client_start(mqtt_client_handle_);

    auto bits = xEventGroupWaitBits(event_group_handle_, MQTT_CONNECTED_EVENT | MQTT_DISCONNECTED_EVENT | MQTT_ERROR_EVENT,
        pdTRUE, pdFALSE, pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
    connected_ = (bits & MQTT_CONNECTED_EVENT) != 0;
    return connected_;
}

void EspMqtt::MqttEventCallback(esp_event_base_t base, int32_t event_id, void *event_data) {
    auto event = (esp_mqtt_event_t*)event_data;
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        connected_ = true;
        xEventGroupSetBits(event_group_handle_, MQTT_CONNECTED_EVENT);
        // 调用连接成功回调，用于重连后恢复订阅和发布状态
        if (on_connected_callback_) {
            on_connected_callback_();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        connected_ = false;
        xEventGroupSetBits(event_group_handle_, MQTT_DISCONNECTED_EVENT);
        // 调用断开连接回调
        if (on_disconnected_callback_) {
            on_disconnected_callback_();
        }
        break;
    case MQTT_EVENT_DATA: {
        auto topic = std::string(event->topic, event->topic_len);
        auto payload = std::string(event->data, event->data_len);
        if (event->data_len == event->total_data_len) {
            if (on_message_callback_) {
                on_message_callback_(topic, payload);
            }
        } else {
            message_payload_.append(payload);
            if (message_payload_.size() >= event->total_data_len && on_message_callback_) {
                on_message_callback_(topic, message_payload_);
                message_payload_.clear();
            }
        }
        break;
    }
    case MQTT_EVENT_BEFORE_CONNECT:
        break;
    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_ERROR:
        xEventGroupSetBits(event_group_handle_, MQTT_ERROR_EVENT);
        ESP_LOGI(TAG, "MQTT error occurred: %s", esp_err_to_name(event->error_handle->esp_tls_last_esp_err));
        break;
    default:
        ESP_LOGI(TAG, "Unhandled event id %ld", event_id);
        break;
    }
}

void EspMqtt::Disconnect() {
    esp_mqtt_client_stop(mqtt_client_handle_);
    esp_mqtt_client_destroy(mqtt_client_handle_);
    mqtt_client_handle_ = nullptr;
    connected_ = false;
    xEventGroupClearBits(event_group_handle_, MQTT_CONNECTED_EVENT | MQTT_DISCONNECTED_EVENT | MQTT_ERROR_EVENT);
}

bool EspMqtt::Publish(const std::string topic, const std::string payload, int qos, bool retain) {
    if (!connected_) {
      return false;
    }
    return esp_mqtt_client_publish(mqtt_client_handle_, topic.c_str(), payload.data(), payload.size(), qos, retain ? 1 : 0) >= 0;
}

bool EspMqtt::Subscribe(const std::string topic, int qos) {
    if (!connected_) {
      return false;
    }
    return esp_mqtt_client_subscribe_single(mqtt_client_handle_, topic.c_str(), qos) >= 0;
}

bool EspMqtt::Unsubscribe(const std::string topic) {
    if (!connected_) {
      return false;
    }
    return esp_mqtt_client_unsubscribe(mqtt_client_handle_, topic.c_str()) >= 0;
}

bool EspMqtt::IsConnected() {
    return connected_;
}
