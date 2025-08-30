#include "mqtt_notifier.h"

#include <cstring>
#include <esp_log.h>
#include "board.h"
#include "sdkconfig.h"
#include <esp_wifi.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <esp_app_desc.h>
#include <sstream>
#include <iomanip>
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
	
	// 停止ACK监控任务
	if (ack_monitor_task_handle_ != nullptr) {
		vTaskDelete(ack_monitor_task_handle_);
		ack_monitor_task_handle_ = nullptr;
	}
	
	// 清理待确认消息
	{
		std::lock_guard<std::mutex> lock(pending_acks_mutex_);
		pending_acks_.clear();
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
		if (root) {
			// 检查是否是ACK确认回复
			cJSON* type = cJSON_GetObjectItem(root, "type");
			if (type && cJSON_IsString(type) && std::strcmp(type->valuestring, "ack_receipt") == 0) {
				HandleAckReceipt(root);
			} else if (on_message_) {
				// 其他消息交给回调处理
				on_message_(root);
			}
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

	// 启动心跳任务（每60秒上报在线状态+指标）
	if (heartbeat_task_handle_ == nullptr) {
		xTaskCreate([](void* arg){
			static const int kIntervalMs = 60000;  // 调整为60秒，减少网络负载
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

					// IoT设备状态（快照）：仅在状态变化时上报，避免频繁上报大量数据
					static std::string last_iot_states_json;
					auto& thing_manager = iot::ThingManager::GetInstance();
					std::string iot_states_json;
					thing_manager.GetStatesJson(iot_states_json, false);
					if (iot_states_json != last_iot_states_json && !iot_states_json.empty() && iot_states_json != "[]") {
						cJSON* iot_states = cJSON_Parse(iot_states_json.c_str());
						if (iot_states && cJSON_IsArray(iot_states) && cJSON_GetArraySize(iot_states) > 0) {
							cJSON_AddItemToObject(root, "iot_states", iot_states);
							last_iot_states_json = iot_states_json;
						} else if (iot_states) {
							cJSON_Delete(iot_states);
						}
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
	
	// 启动ACK确认监控任务（增加栈空间到6KB）
	if (ack_monitor_task_handle_ == nullptr) {
		xTaskCreate([](void* arg){
			MqttNotifier* self = static_cast<MqttNotifier*>(arg);
			for(;;){
				self->CheckPendingAcks();
				vTaskDelay(pdMS_TO_TICKS(5000));  // 每5秒检查一次
			}
		}, "ack_monitor", 6144, this, 2, &ack_monitor_task_handle_);
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



// =============================================================================
// ACK发布方法（内置服务器确认机制）
// =============================================================================

bool MqttNotifier::PublishAck(const char* json, int qos) {
	if (mqtt_ == nullptr || ack_topic_.empty()) {
		ESP_LOGW(TAG, "PublishAck skipped: mqtt not ready or ack topic empty");
		return false;
	}
	
	if (json == nullptr || std::strlen(json) == 0) {
		ESP_LOGW(TAG, "PublishAck skipped: empty json");
		return false;
	}
	
	// 解析现有JSON并添加message_id
	cJSON* root = cJSON_Parse(json);
	if (root == nullptr) {
		ESP_LOGE(TAG, "Failed to parse JSON for ACK confirmation");
		return false;
	}
	
	// 生成唯一消息ID
	std::string message_id = GenerateMessageId();
	cJSON_AddStringToObject(root, "message_id", message_id.c_str());
	
	// 转换回JSON字符串
	char* payload = cJSON_PrintUnformatted(root);
	if (!payload) {
		ESP_LOGE(TAG, "Failed to stringify JSON for ACK confirmation");
		cJSON_Delete(root);
		return false;
	}
	
	int attempt_qos = qos;
	if (attempt_qos < 0) attempt_qos = 0;
	if (attempt_qos > 2) attempt_qos = 2;
	
	// 发送消息
	// 无论发送状态如何，都添加到待确认列表，让超时机制处理真正的失败
	{
		std::lock_guard<std::mutex> lock(pending_acks_mutex_);
		pending_acks_[message_id] = PendingAck(std::string(payload), attempt_qos);
	}
	
	bool ok = mqtt_->Publish(ack_topic_, payload, attempt_qos);
	if (ok) {
		ESP_LOGI(TAG, "PublishAck %s (QoS=%d): sent with message_id=%s", 
			 ack_topic_.c_str(), attempt_qos, message_id.c_str());
	} else {
		ESP_LOGW(TAG, "PublishAck %s (QoS=%d): publish returned false for message_id=%s (but may still reach server)", 
			 ack_topic_.c_str(), attempt_qos, message_id.c_str());
	}
	
	cJSON_free(payload);
	cJSON_Delete(root);
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

// =============================================================================
// ACK确认处理相关方法
// =============================================================================

void MqttNotifier::HandleAckReceipt(const cJSON* receipt) {
	cJSON* message_id_json = cJSON_GetObjectItem(receipt, "message_id");
	cJSON* status_json = cJSON_GetObjectItem(receipt, "status");
	cJSON* received_at_json = cJSON_GetObjectItem(receipt, "received_at");
	
	if (!message_id_json || !cJSON_IsString(message_id_json)) {
		ESP_LOGW(TAG, "Invalid ACK receipt: missing or invalid message_id");
		return;
	}
	
	std::string message_id = message_id_json->valuestring;
	std::string status = (status_json && cJSON_IsString(status_json)) ? status_json->valuestring : "unknown";
	uint32_t received_at = (received_at_json && cJSON_IsNumber(received_at_json)) ? (uint32_t)received_at_json->valuedouble : 0;
	
	// 从待确认列表中移除
	std::lock_guard<std::mutex> lock(pending_acks_mutex_);
	auto it = pending_acks_.find(message_id);
	if (it != pending_acks_.end()) {
		auto duration = std::chrono::steady_clock::now() - it->second.sent_time;
		auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
		
		ESP_LOGI(TAG, "ACK confirmation received: message_id=%s, status=%s, rtt=%lldms, server_time=%lu", 
			 message_id.c_str(), status.c_str(), (long long)duration_ms, (unsigned long)received_at);
		
		pending_acks_.erase(it);
	} else {
		ESP_LOGW(TAG, "Received ACK confirmation for unknown message_id: %s", message_id.c_str());
	}
}

std::string MqttNotifier::GenerateMessageId() {
	auto now = std::chrono::system_clock::now();
	auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	
	std::stringstream ss;
	ss << "msg_" << timestamp << "_" << (next_message_id_++);
	return ss.str();
}

void MqttNotifier::CheckPendingAcks() {
	std::lock_guard<std::mutex> lock(pending_acks_mutex_);
	
	if (pending_acks_.empty()) {
		return;
	}
	
	auto now = std::chrono::steady_clock::now();
	std::vector<std::string> to_remove;  // 收集需要删除的key
	std::vector<std::string> to_retry;   // 收集需要重试的key
	
	// 先遍历找出超时的消息
	for (const auto& pair : pending_acks_) {
		auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.sent_time).count();
		
		if (duration_ms > ACK_TIMEOUT_MS) {
			if (pair.second.retry_count < MAX_ACK_RETRIES) {
				to_retry.push_back(pair.first);
			} else {
				to_remove.push_back(pair.first);
				ESP_LOGE(TAG, "ACK permanently failed for message_id=%s after %d attempts", 
					 pair.first.c_str(), pair.second.retry_count + 1);
			}
		}
	}
	
	// 处理重试
	for (const auto& message_id : to_retry) {
		auto it = pending_acks_.find(message_id);
		if (it != pending_acks_.end()) {
			ESP_LOGW(TAG, "ACK timeout for message_id=%s (attempt %d), retrying...", 
				 message_id.c_str(), it->second.retry_count + 1);
			
			// 尝试重发（不使用延迟）
			if (mqtt_ && !ack_topic_.empty()) {
				bool ok = mqtt_->Publish(ack_topic_, it->second.payload, it->second.qos);
				if (ok) {
					it->second.retry_count++;
					it->second.sent_time = now;
					ESP_LOGI(TAG, "ACK message resent: message_id=%s (QoS=%d)", message_id.c_str(), it->second.qos);
				} else {
					to_remove.push_back(message_id);
					ESP_LOGW(TAG, "Failed to resend ACK message: message_id=%s", message_id.c_str());
				}
			} else {
				to_remove.push_back(message_id);
			}
		}
	}
	
	// 删除失败的消息
	for (const auto& message_id : to_remove) {
		pending_acks_.erase(message_id);
	}
	
	// 记录待确认消息数量
	ESP_LOGD(TAG, "Pending ACK confirmations: %zu", pending_acks_.size());
}




