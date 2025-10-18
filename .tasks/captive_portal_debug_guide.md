# iOS Captive Portal 调试指南

## 问题：iPhone扫码后不弹出配网页面

## 已实施的优化

### 1. DNS服务器增强
- ✅ 添加详细日志：显示每个DNS查询的域名
- ✅ 正确响应：所有域名解析到 192.168.4.1

### 2. HTTP端点优化
- ✅ iOS专用端点：`/hotspot-detect.html` 和 `/library/test/success.html`
- ✅ Android专用端点：`/generate_204`
- ✅ 通配符处理器：捕获所有未知请求
- ✅ 详细请求日志：记录URI、Host、User-Agent

### 3. 日志追踪
所有关键点都添加了日志，可以看到完整的请求流程。

---

## 📋 调试步骤

### 第一步：编译烧录
```bash
cd e:\work\xiaozhi-esp32
idf.py build
idf.py flash monitor
```

### 第二步：进入配网模式
让设备进入WiFi配网模式，观察日志输出。

### 第三步：iPhone操作
1. 打开iPhone相机
2. 扫描屏幕上的二维码
3. 点击"加入网络 独众AI伴侣-XXXX"
4. **不要关闭监视器**，保持查看日志

### 第四步：观察日志输出

**应该看到的日志顺序**：

```
[DnsServer] DNS query for: captive.apple.com from 192.168.4.2
[DnsServer] DNS response: captive.apple.com -> 192.168.4.1

[WifiConfigurationAp] iOS Captive Portal detection: /hotspot-detect.html
[WifiConfigurationAp] Host header: captive.apple.com
[WifiConfigurationAp] iOS detection response sent
```

如果看到这些日志，说明iOS正在检测，应该会弹出页面。

---

## 🔍 可能的问题和解决方案

### 问题1：没有DNS查询日志
**症状**：看不到 `[DnsServer] DNS query for:` 日志

**原因**：
- DNS服务器未启动
- iOS使用了其他DNS服务器
- iOS缓存了DNS结果

**解决方法**：
1. 检查DNS服务器是否正常启动
2. iPhone忘记该WiFi网络，重新连接
3. 重启iPhone后再试

### 问题2：有DNS查询但没有HTTP请求
**症状**：看到DNS日志，但没有HTTP请求日志

**原因**：
- iOS可能尝试HTTPS（我们只支持HTTP）
- 防火墙或路由问题
- iOS版本太新，使用了不同的检测机制

**解决方法**：
1. 确认iOS版本（建议iOS 14-17）
2. 尝试在iPhone上手动打开浏览器访问 `http://192.168.4.1`
3. 检查AP的IP配置是否正确

### 问题3：有HTTP请求但不是检测端点
**症状**：看到根路径 `/` 的访问，但没有 `/hotspot-detect.html`

**原因**：
- iOS认为网络正常，没有触发Captive Portal检测
- DNS劫持可能不完整

**解决方法**：
1. 查看通配符日志，确认iOS访问了哪些URL
2. 手动访问 `http://captive.apple.com/hotspot-detect.html`
3. 尝试其他iOS设备

### 问题4：iOS 17+ 使用隐私中继
**症状**：iOS连接后完全没有请求

**原因**：
iOS 17启用了"iCloud 隐私中继"或"私有网络地址"

**解决方法**：
1. 打开iPhone设置 → WiFi
2. 点击已连接的"独众AI伴侣-XXXX"后的ⓘ图标
3. 关闭"私有网络地址"
4. 关闭"限制IP地址跟踪"
5. 重新连接WiFi

---

## 🧪 手动测试方法

如果自动弹出失败，尝试手动测试：

### 测试1：手动DNS解析
在iPhone连接AP后，使用第三方App（如Network Analyzer）测试：
```
nslookup captive.apple.com
```
应该返回：`192.168.4.1`

### 测试2：手动HTTP请求
在iPhone浏览器中访问：
```
http://192.168.4.1
http://captive.apple.com
http://captive.apple.com/hotspot-detect.html
```

所有这些地址都应该能访问并显示配网页面。

### 测试3：强制触发
在iPhone上：
1. 打开Safari
2. 输入任何不存在的域名，如 `http://test.com`
3. 如果Captive Portal正常，应该重定向到配网页面

---

## 📊 完整日志示例

**正常工作时的日志**：
```
[WifiConfigurationAp] Access Point started with SSID 独众AI伴侣-1A2B
[DnsServer] Starting DNS server
[WifiConfigurationAp] Web server started with optimized Captive Portal detection

// iPhone连接
[WifiConfigurationAp] Station AA:BB:CC:DD:EE:FF joined, AID=1

// DNS查询
[DnsServer] DNS query for: captive.apple.com from 192.168.4.2
[DnsServer] DNS response: captive.apple.com -> 192.168.4.1

// HTTP检测
[WifiConfigurationAp] iOS Captive Portal detection: /hotspot-detect.html
[WifiConfigurationAp] Host header: captive.apple.com
[WifiConfigurationAp] iOS detection response sent

// 页面加载
[WifiConfigurationAp] Root path accessed from client
[WifiConfigurationAp] Host: 192.168.4.1, User-Agent: CaptiveNetworkSupport...
```

---

## 💡 实测建议

1. **先用Android测试**：Android的Captive Portal更宽容，更容易触发
2. **尝试多个iOS版本**：iOS 15, 16, 17行为可能不同
3. **对比其他设备**：如果有其他ESP32配网设备，对比其实现
4. **抓包分析**：使用Wireshark抓包，查看iPhone实际发送的请求

---

## 下一步行动

请按照上述步骤操作，并提供：
1. 完整的串口日志输出
2. iPhone的iOS版本
3. iPhone的任何特殊设置（VPN、隐私中继等）
4. 手动访问测试的结果

这样我们可以精确定位问题所在。
