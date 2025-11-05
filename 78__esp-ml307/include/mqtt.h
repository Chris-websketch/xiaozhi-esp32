#ifndef MQTT_INTERFACE_H
#define MQTT_INTERFACE_H

#include <string>
#include <functional>

class Mqtt {
public:
    virtual ~Mqtt() {}

    void SetKeepAlive(int keep_alive_seconds) { keep_alive_seconds_ = keep_alive_seconds; }
    void SetLastWill(const std::string& topic, const std::string& message, int qos = 1, bool retain = true) {
        lwt_topic_ = topic;
        lwt_message_ = message;
        lwt_qos_ = qos;
        lwt_retain_ = retain;
        lwt_enabled_ = true;
    }
    virtual bool Connect(const std::string broker_address, int broker_port, const std::string client_id, const std::string username, const std::string password) = 0;
    virtual void Disconnect() = 0;
    virtual bool Publish(const std::string topic, const std::string payload, int qos = 0, bool retain = false) = 0;
    virtual bool Subscribe(const std::string topic, int qos = 0) = 0;
    virtual bool Unsubscribe(const std::string topic) = 0;
    virtual bool IsConnected() = 0;

    virtual void OnConnected(std::function<void()> callback) { on_connected_callback_ = callback; }
    virtual void OnDisconnected(std::function<void()> callback) { on_disconnected_callback_ = callback; }
    virtual void OnMessage(std::function<void(const std::string& topic, const std::string& payload)> callback) { on_message_callback_ = callback; }

protected:
    int keep_alive_seconds_ = 60;
    bool lwt_enabled_ = false;
    std::string lwt_topic_;
    std::string lwt_message_;
    int lwt_qos_ = 1;
    bool lwt_retain_ = true;
    std::function<void(const std::string& topic, const std::string& payload)> on_message_callback_;
    std::function<void()> on_connected_callback_;
    std::function<void()> on_disconnected_callback_;
};

#endif // MQTT_INTERFACE_H
