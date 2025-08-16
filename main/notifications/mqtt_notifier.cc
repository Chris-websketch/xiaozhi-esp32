#include "mqtt_notifier.h"

#include <esp_log.h>
#include "board.h"
#include "sdkconfig.h"
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <esp_app_desc.h>
#include "system_info.h"
#include "iot/thing_manager.h"

static const char* TAG = "MqttNotifier";

MqttNotifier::MqttNotifier() {}
MqttNotifier::~MqttNotifier() { Stop(); }

bool MqttNotifier::LoadSettings() {
	Settings settings("mqtt", false);
	endpoint_ = settings.GetString("endpoint");
	client_id_ = settings.GetString("client_id");
	username_ = settings.GetString("username");
	password_ = settings.GetString("password");
	downlink_topic_ = settings.GetString("downlink_topic");

	// 使用内置的 EMQX 地址和测试账号作为默认值
	if (endpoint_.empty()) {
		endpoint_ = "x6bf310e.ala.cn-hangzhou.emqxsl.cn";
	}
	if (client_id_.empty()) {
	#ifdef CONFIG_WEBSOCKET_CLIENT_ID
		client_id_ = CONFIG_WEBSOCKET_CLIENT_ID;
	#endif
	}
	
	// 强制使用测试账号（覆盖 NVS 中可能存在的旧数据）
	username_ = "xiaoqiao";
	password_ = "dzkj0000";

	if (downlink_topic_.empty() && !client_id_.empty()) {
		downlink_topic_ = std::string("devices/") + client_id_ + "/downlink";
	}
	// 计算上行主题（心跳与事件上报）
	if (!client_id_.empty()) {
		uplink_topic_ = std::string("devices/") + client_id_ + "/uplink";
		// 计算 ACK 主题（指令执行结果）
		ack_topic_ = std::string("devices/") + client_id_ + "/ack";
	}
	if (endpoint_.empty() || client_id_.empty()) {
		ESP_LOGW(TAG, "MQTT notifier settings incomplete (endpoint/client_id)");
		return false;
	}
	return true;
}

void MqttNotifier::Start() {
	if (started_) return;
	if (!LoadSettings()) {
		ESP_LOGW(TAG, "Start skipped due to invalid settings");
		return;
	}
	started_ = ConnectInternal();
}

void MqttNotifier::Stop() {
	if (mqtt_ != nullptr) {
		delete mqtt_;
		mqtt_ = nullptr;
	}
	if (heartbeat_task_handle_ != nullptr) {
		vTaskDelete(heartbeat_task_handle_);
		heartbeat_task_handle_ = nullptr;
	}
	started_ = false;
}

bool MqttNotifier::ConnectInternal() {
	if (mqtt_ != nullptr) {
		delete mqtt_;
		mqtt_ = nullptr;
	}
	mqtt_ = Board::GetInstance().CreateMqtt();
	mqtt_->SetKeepAlive(90);

	mqtt_->OnMessage([this](const std::string& topic, const std::string& payload) {
		ESP_LOGI(TAG, "MQTT message received: topic=%s, payload=%s", topic.c_str(), payload.c_str());
		
		// 过滤：忽略自发主题（uplink/ack），设备不应该接收自己发布的消息
		if ((!uplink_topic_.empty() && topic == uplink_topic_) || (!ack_topic_.empty() && topic == ack_topic_)) {
			ESP_LOGW(TAG, "Ignoring unexpected uplink message (device should not subscribe to uplink)");
			return;
		}
		
		cJSON* root = cJSON_Parse(payload.c_str());
		if (root && on_message_) {
			on_message_(root);
		}
		if (root) {
			cJSON_Delete(root);
		}
	});

	ESP_LOGI(TAG, "Connecting to %s (client_id=%s, username=%s)", 
		endpoint_.c_str(), client_id_.c_str(), username_.c_str());
	if (!mqtt_->Connect(endpoint_, 8883, client_id_, username_, password_)) {
		ESP_LOGE(TAG, "Failed to connect");
		delete mqtt_;
		mqtt_ = nullptr;
		return false;
	}
	
	// 确保连接稳定
	vTaskDelay(pdMS_TO_TICKS(500));
	
	if (!mqtt_->IsConnected()) {
		ESP_LOGE(TAG, "Connection lost after connect");
		return false;
	}

	// 启动心跳任务（每30秒上报在线状态+指标）
	if (heartbeat_task_handle_ == nullptr) {
		xTaskCreate([](void* arg){
			static const int kIntervalMs = 30000;
			MqttNotifier* self = static_cast<MqttNotifier*>(arg);
			for(;;){
				if (self->mqtt_ && self->mqtt_->IsConnected()) {
					cJSON* root = cJSON_CreateObject();
					cJSON_AddStringToObject(root, "type", "telemetry");
					cJSON_AddBoolToObject(root, "online", true);
					cJSON_AddNumberToObject(root, "ts", (double)time(NULL));

					// device info
					auto app_desc = esp_app_get_description();
					cJSON_AddStringToObject(root, "device_name", BOARD_NAME);
					if (app_desc) {
						cJSON_AddStringToObject(root, "ota_version", app_desc->version);
					}
					cJSON_AddStringToObject(root, "mac", SystemInfo::GetMacAddress().c_str());
					cJSON_AddStringToObject(root, "client_id", self->client_id_.c_str());

					// battery
					int battery_level = -1; bool charging = false; bool discharging = false;
					if (Board::GetInstance().GetBatteryLevel(battery_level, charging, discharging)) {
						cJSON* battery = cJSON_CreateObject();
						cJSON_AddNumberToObject(battery, "level", battery_level);
						cJSON_AddBoolToObject(battery, "charging", charging);
						cJSON_AddBoolToObject(battery, "discharging", discharging);
						cJSON_AddItemToObject(root, "battery", battery);
					}

					// memory
					int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
					int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
					cJSON* memory = cJSON_CreateObject();
					cJSON_AddNumberToObject(memory, "free_internal", free_sram);
					cJSON_AddNumberToObject(memory, "min_free_internal", min_free_sram);
					cJSON_AddItemToObject(root, "memory", memory);

					// wifi rssi
					wifi_ap_record_t ap = {};
					if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
						cJSON* wifi = cJSON_CreateObject();
						cJSON_AddNumberToObject(wifi, "rssi", ap.rssi);
						cJSON_AddItemToObject(root, "wifi", wifi);
					}

					// IoT设备状态（快照）：解析成功且非空数组才附加，避免依赖返回布尔值语义
					auto& thing_manager = iot::ThingManager::GetInstance();
					std::string iot_states_json;
					thing_manager.GetStatesJson(iot_states_json, false);
					cJSON* iot_states = cJSON_Parse(iot_states_json.c_str());
					if (iot_states && cJSON_IsArray(iot_states) && cJSON_GetArraySize(iot_states) > 0) {
						cJSON_AddItemToObject(root, "iot_states", iot_states);
					} else if (iot_states) {
						cJSON_Delete(iot_states);
					}

					char* payload = cJSON_PrintUnformatted(root);
					if (payload) {
						if (!self->uplink_topic_.empty()) {
							bool ok = self->mqtt_->Publish(self->uplink_topic_, payload, 0);
							ESP_LOGI(TAG, "Heartbeat publish %s: %s", self->uplink_topic_.c_str(), ok ? "ok" : "fail");
						}
						cJSON_free(payload);
					}
					cJSON_Delete(root);
				}
				vTaskDelay(pdMS_TO_TICKS(kIntervalMs));
			}
		}, "mqtt_heartbeat", 4096, this, 3, &heartbeat_task_handle_);
	}

	// 显式订阅下行主题，确保服务端推送能到达
	if (!downlink_topic_.empty()) {
		// 可靠控制：订阅使用 QoS 2
		bool sub_ok = mqtt_->Subscribe(downlink_topic_, 2);
		ESP_LOGI(TAG, "Subscribe %s: %s", downlink_topic_.c_str(), sub_ok ? "ok" : "fail");
	}
	
	ESP_LOGI(TAG, "MQTT notifier connected and ready");
	return true;
}

void MqttNotifier::SetupSubscriptions() {
	// 已验证：即使订阅失败，消息接收功能仍正常工作
	// 保留此函数以备将来需要时使用
}

void MqttNotifier::ReconnectIfSettingsChanged() {
	std::string old_endpoint = endpoint_;
	std::string old_client_id = client_id_;
	std::string old_username = username_;
	std::string old_password = password_;
	std::string old_downlink = downlink_topic_;
	if (!LoadSettings()) {
		return;
	}
	if (endpoint_ != old_endpoint || client_id_ != old_client_id ||
		username_ != old_username || password_ != old_password ||
		downlink_topic_ != old_downlink) {
		ESP_LOGI(TAG, "MQTT notifier settings changed, reconnecting");
		ConnectInternal();
	}
}

void MqttNotifier::OnMessage(std::function<void(const cJSON* root)> cb) {
	on_message_ = std::move(cb);
}


bool MqttNotifier::PublishUplink(const char* json, int qos) {
	if (mqtt_ == nullptr || uplink_topic_.empty()) {
		ESP_LOGW(TAG, "PublishUplink skipped: mqtt not ready or uplink topic empty");
		return false;
	}
	int attempt_qos = qos;
	if (attempt_qos < 0) attempt_qos = 0;
	if (attempt_qos > 2) attempt_qos = 2;
	bool ok = mqtt_->Publish(uplink_topic_, json, attempt_qos);
	if (ok) {
		ESP_LOGI(TAG, "PublishUplink %s (QoS=%d): sent", uplink_topic_.c_str(), attempt_qos);
	} else {
		ESP_LOGW(TAG, "PublishUplink %s (QoS=%d): send failed, but message may still reach server", uplink_topic_.c_str(), attempt_qos);
	}
	return ok;
}

bool MqttNotifier::PublishUplink(const cJSON* root, int qos) {
	if (root == nullptr) return false;
	char* payload = cJSON_PrintUnformatted(root);
	if (!payload) return false;
	bool ok = PublishUplink(payload, qos);
	cJSON_free(payload);
	return ok;
}

bool MqttNotifier::PublishAck(const char* json, int qos) {
	if (mqtt_ == nullptr || ack_topic_.empty()) {
		ESP_LOGW(TAG, "PublishAck skipped: mqtt not ready or ack topic empty");
		return false;
	}
	int attempt_qos = qos;
	if (attempt_qos < 0) attempt_qos = 0;
	if (attempt_qos > 2) attempt_qos = 2;
	bool ok = mqtt_->Publish(ack_topic_, json, attempt_qos);
	if (ok) {
		ESP_LOGI(TAG, "PublishAck %s (QoS=%d): sent", ack_topic_.c_str(), attempt_qos);
	} else {
		ESP_LOGW(TAG, "PublishAck %s (QoS=%d): send failed, but message may still reach server", ack_topic_.c_str(), attempt_qos);
	}
	return ok;
}

bool MqttNotifier::PublishAck(const cJSON* root, int qos) {
	if (root == nullptr) return false;
	char* payload = cJSON_PrintUnformatted(root);
	if (!payload) return false;
	bool ok = PublishAck(payload, qos);
	cJSON_free(payload);
	return ok;
}

