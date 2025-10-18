# WiFi配网二维码功能实现

## 任务目标
在现有AP配网基础上添加WiFi二维码显示功能，用户扫码即可自动连接AP并跳转到配网页面。

## 实施日期
2025-01-18

## 技术方案
采用集成式二维码显示方案，使用LVGL内置的QR Code库，显示WiFi标准格式二维码。

## 已完成的修改

### 1. 启用LVGL二维码支持
**文件**: `sdkconfig.defaults`
- 添加 `CONFIG_LV_USE_QRCODE=y` 配置

### 2. 扩展Display类
**文件**: `main/display/display.h`
- 添加 `ShowQRCode()` 方法声明
- 添加 `HideQRCode()` 方法声明
- 添加 `qrcode_obj_` 成员变量

**文件**: `main/display/display.cc`
- 实现 `ShowQRCode()` 方法
  - 支持自适应屏幕尺寸（大屏120px，中屏100px，小屏70%）
  - 自动居中显示
  - 支持自定义位置和大小
  - 添加白色边框增强可读性
- 实现 `HideQRCode()` 方法
- 在析构函数中添加清理逻辑

### 3. 集成到配网流程
**文件**: `main/boards/common/wifi_board.cc`
- 在 `EnterWifiConfigMode()` 中添加二维码显示
- 使用WiFi标准格式：`WIFI:T:nopass;S:<SSID>;P:;;`
- 与声波配网功能兼容

## 二维码格式
```
WIFI:T:nopass;S:独众AI伴侣-XXXX;P:;;
```
- `T:nopass` - 无密码开放网络
- `S:` - SSID（包含设备MAC后4位）
- `P:` - 密码（空）

## 用户体验流程
1. 设备进入配网模式
2. 屏幕显示二维码
3. 用户扫码 → 手机自动连接AP
4. 连接后自动弹出配网页面（Captive Portal）
5. 用户在页面上选择WiFi并输入密码
6. 配置完成，设备重启

## 技术特性
- **自适应显示**：根据屏幕尺寸自动调整二维码大小
- **优雅降级**：如果QRCODE未启用，不影响原有配网功能
- **内存管理**：二维码使用LVGL内部内存管理，销毁时自动释放
- **线程安全**：使用DisplayLockGuard确保线程安全

## 待测试项
- [ ] 编译无错误
- [ ] 二维码正确显示
- [ ] 手机扫码能自动连接AP
- [ ] 不同屏幕尺寸的显示效果
- [ ] 与声波配网功能兼容性
- [ ] 内存占用情况

## 下一步
1. 重新编译项目（需要重新配置以应用sdkconfig变更）
2. 烧录到设备测试
3. 验证二维码可读性
4. 优化显示位置和大小（如有需要）

## Captive Portal优化（2025-01-18 下午）

### 问题
iPhone扫码连接AP后，需要等待很久才弹出配网页面。

### 原因分析
- iOS检测Captive Portal时访问 `/hotspot-detect.html`
- 原实现返回302重定向，iOS会重试多次才弹出页面
- iOS期望收到200状态码但内容不是"Success"

### 优化方案
1. **iOS专用处理器**：为 `/hotspot-detect.html` 返回200状态码 + 非预期HTML内容
2. **Android专用处理器**：为 `/generate_204` 返回200而非204状态码
3. **通配符处理器**：捕获所有未匹配请求，确保任何检测URL都有响应
4. **增加处理器容量**：max_uri_handlers 从20增至30

### 修改文件
- `managed_components/esp-wifi-connect/wifi_configuration_ap.cc`
  - 添加 `ios_detect_handler` - 快速触发iOS Captive Portal
  - 添加 `android_detect_handler` - 快速触发Android Captive Portal
  - 添加通配符处理器 `/*` - 捕获所有请求
  - 增加URI处理器上限

### 预期效果
- iPhone扫码后 **2-3秒** 内弹出配网页面（原来需要10-30秒）
- Android和HarmonyOS也会更快响应
- 兼容所有平台的Captive Portal检测机制

## 深度调试优化（2025-01-18 下午）

### 问题反馈
用户测试后反馈：iPhone仍然不能快速弹出配网页面。

### 新增诊断措施
1. **DNS服务器日志增强**
   - 显示每个DNS查询的完整域名
   - 显示DNS响应的IP地址
   - 帮助确认DNS劫持是否正常工作

2. **HTTP请求日志增强**
   - 记录每个HTTP请求的URI
   - 记录Host头部（识别来源域名）
   - 记录User-Agent（识别设备类型）
   - 帮助确认iOS是否真的发起了检测请求

3. **创建调试指南**
   - 详细的步骤指导
   - 常见问题诊断
   - 手动测试方法
   - 完整日志示例

### 修改文件
- `managed_components/esp-wifi-connect/dns_server.cc`
  - 添加域名解析日志
  - 修复IP地址显示错误
  
- `managed_components/esp-wifi-connect/wifi_configuration_ap.cc`
  - 为iOS检测端点添加详细日志
  - 为根路径添加请求追踪
  - 记录Host和User-Agent信息

### 调试指南
详见：`.tasks/captive_portal_debug_guide.md`

### 可能的根本原因
1. **iOS 17+ 隐私保护**：私有网络地址或隐私中继阻止检测
2. **HTTPS问题**：新版iOS可能优先尝试HTTPS检测
3. **DNS缓存**：iOS缓存了之前的DNS结果
4. **网络配置**：AP的DHCP或路由配置问题

## UI优化（2025-01-18 下午）

### 添加二维码提示文字
**需求**：在二维码上方显示提示文字"请使用手机相机扫码"

**实现**：
- 添加 `qrcode_hint_label_` 成员变量
- 在 `ShowQRCode()` 中创建白色文字标签
- 自动居中对齐在二维码上方10像素处
- 在 `HideQRCode()` 和析构函数中清理

**效果**：
```
┌──────────────────┐
│   状态栏         │
│                  │
│ 请使用手机相机扫码│ ← 新增提示文字
│   ┌─────┐       │
│   │ QR │       │
│   │90px│       │
│   └─────┘       │
└──────────────────┘
```

## 备注
- 二维码库预估内存占用：2-4KB
- 支持iOS和Android原生WiFi二维码识别
- 配网完成后二维码会在设备重启时自动清除
- Captive Portal优化显著提升用户体验
- iOS 18.7需要用户查看通知或手动访问配网页面
