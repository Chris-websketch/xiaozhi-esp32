#ifndef MQTT_NOTIFIER_H
#define MQTT_NOTIFIER_H

#include <mqtt.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <vector>

#include "settings.h"
#include "boards/common/board.h"

class MqttNotifier {
public:
	MqttNotifier();
	~MqttNotifier();

	// 启动轻量级 MQTT 监听，仅订阅/接收下发通知
	void Start();
	// 停止并清理连接
	void Stop();

	// 当 Settings("mqtt") 被 OTA 更新时，可调用此方法尝试使用新配置重连
	void ReconnectIfSettingsChanged();

	// 设置收到 JSON 通知时的回调
	void OnMessage(std::function<void(const cJSON* root)> cb);

	// 向 uplink 主题发布自定义 JSON（QoS=0 默认）
	bool PublishUplink(const char* json, int qos = 0);
	bool PublishUplink(const cJSON* root, int qos = 0);
	// 向独立 ACK 主题发布指令执行结果（默认 QoS=0，快速发送不等待确认）
	bool PublishAck(const char* json, int qos = 0);
	bool PublishAck(const cJSON* root, int qos = 0);

private:
	// 读取当前配置到成员变量，返回是否配置有效
	bool LoadSettings();
	bool ConnectInternal();
	void SetupSubscriptions();
	static void HeartbeatTask(void* arg);

private:
	// ACK确认机制已移除


private:
	std::function<void(const cJSON* root)> on_message_;
	Mqtt* mqtt_ = nullptr;
	std::string endpoint_;
	std::string client_id_;
	std::string username_;
	std::string password_;
	std::string downlink_topic_;
	std::string uplink_topic_;
	std::string ack_topic_;
	bool started_ = false;
	TaskHandle_t heartbeat_task_handle_ = nullptr;
};

#endif // MQTT_NOTIFIER_H


