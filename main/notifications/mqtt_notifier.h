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
	// 同时启动ACK确认监控任务
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
	// 向独立 ACK 主题发布指令执行结果（默认 QoS=2，内置服务器确认机制）
	bool PublishAck(const char* json, int qos = 2);
	bool PublishAck(const cJSON* root, int qos = 2);
	
	// 检查待确认消息的超时状态
	void CheckPendingAcks();

private:
	// 读取当前配置到成员变量，返回是否配置有效
	bool LoadSettings();
	bool ConnectInternal();
	void SetupSubscriptions();
	static void HeartbeatTask(void* arg);

private:
	// ACK 确认跟踪结构
	struct PendingAck {
		std::string payload;
		std::chrono::steady_clock::time_point sent_time;
		int retry_count;
		int qos;
		
		// 默认构造函数
		PendingAck() : payload(""), sent_time(std::chrono::steady_clock::now()), retry_count(0), qos(0) {}
		
		// 参数化构造函数
		PendingAck(const std::string& p, int q) 
			: payload(p), sent_time(std::chrono::steady_clock::now()), retry_count(0), qos(q) {}
	};
	
	// 处理服务器确认回复
	void HandleAckReceipt(const cJSON* receipt);
	
	// 生成消息ID
	std::string GenerateMessageId();
	


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
	TaskHandle_t ack_monitor_task_handle_ = nullptr;
	
	// ACK 确认跟踪相关
	std::map<std::string, PendingAck> pending_acks_;
	std::mutex pending_acks_mutex_;
	uint32_t next_message_id_ = 1;
	
	// 配置常量
	static constexpr int ACK_TIMEOUT_MS = 10000;  // 10秒超时
	static constexpr int MAX_ACK_RETRIES = 2;     // 最多重试2次
	static constexpr int RETRY_DELAY_MS = 2000;   // 重试延迟2秒
};

#endif // MQTT_NOTIFIER_H


