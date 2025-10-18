#include "dns_server.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#define TAG "DnsServer"

DnsServer::DnsServer() {
}

DnsServer::~DnsServer() {
}

void DnsServer::Start(esp_ip4_addr_t gateway) {
    ESP_LOGI(TAG, "Starting DNS server");
    gateway_ = gateway;

    fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    if (bind(fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "failed to bind port %d", port_);
        close(fd_);
        return;
    }

    xTaskCreate([](void* arg) {
        DnsServer* dns_server = static_cast<DnsServer*>(arg);
        dns_server->Run();
    }, "DnsServerTask", 4096, this, 5, NULL);
}

void DnsServer::Stop() {
    ESP_LOGI(TAG, "Stopping DNS server");
}

void DnsServer::Run() {
    char buffer[512];
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int len = recvfrom(fd_, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed, errno=%d", errno);
            continue;
        }

        // 提取查询的域名（用于日志）
        char domain[256] = {0};
        int pos = 12;  // DNS头部是12字节
        int domain_pos = 0;
        while (pos < len && buffer[pos] != 0 && domain_pos < 255) {
            uint8_t label_len = buffer[pos];
            if (label_len > 63) break;  // 标签长度异常
            pos++;
            for (int i = 0; i < label_len && pos < len && domain_pos < 254; i++, pos++) {
                domain[domain_pos++] = buffer[pos];
            }
            if (buffer[pos] != 0 && domain_pos < 255) {
                domain[domain_pos++] = '.';
            }
        }
        domain[domain_pos] = '\0';
        
        ESP_LOGI(TAG, "DNS query for: %s from %s", domain, inet_ntoa(client_addr.sin_addr));

        // Simple DNS response: point all queries to gateway (192.168.4.1)
        buffer[2] |= 0x80;  // Set response flag
        buffer[3] |= 0x80;  // Set Recursion Available
        buffer[7] = 1;      // Set answer count to 1

        // Add answer section
        memcpy(&buffer[len], "\xc0\x0c", 2);  // Name pointer
        len += 2;
        memcpy(&buffer[len], "\x00\x01\x00\x01\x00\x00\x00\x1c\x00\x04", 10);  // Type, class, TTL, data length
        len += 10;
        memcpy(&buffer[len], &gateway_.addr, 4);  // 192.168.4.1
        len += 4;
        
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&gateway_));
        ESP_LOGI(TAG, "DNS response: %s -> %s", domain, ip_str);

        sendto(fd_, buffer, len, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }
}
