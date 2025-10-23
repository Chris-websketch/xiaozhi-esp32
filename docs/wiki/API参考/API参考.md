# API参考

<cite>
**本文档引用的文件**   
- [application.h](file://main/application.h#L1-L238) - *在最近的提交中更新*
- [board.h](file://main/boards/common/board.h#L1-L56)
- [thing.h](file://main/iot/thing.h#L1-L301)
- [protocol.h](file://main/protocols/protocol.h#L1-L80)
</cite>

## 更新摘要
**已做更改**   
- 在Application类中新增了`StartMqttNotifier`和`StopMqttNotifier`方法的文档说明
- 更新了Application单例类的公共方法列表以反映最新代码状态
- 修正了与省电模式下MQTT通知服务管理相关的使用场景描述
- 增强了源码跟踪系统，明确标注了更新的文件和变更内容

## 目录
1. [Application单例类](#application单例类)  
2. [Board抽象接口](#board抽象接口)  
3. [Thing基类](#thing基类)  
4. [Protocol通用协议接口](#protocol通用协议接口)

## Application单例类

`Application` 类是整个系统的核心单例，负责管理设备状态、音频处理、OTA升级、协议通信等核心功能。该类通过 `GetInstance()` 方法提供全局访问点，禁止拷贝和赋值以确保单例性。

### 公共方法

#### `Start()`
- **功能**: 启动应用程序主循环和后台任务。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 在系统初始化完成后调用，启动主事件循环。
- **线程安全性**: 线程安全，内部使用互斥锁保护共享资源。
- **示例**:
```cpp
Application::GetInstance().Start();
```

#### `GetDeviceState()`
- **功能**: 获取当前设备状态。
- **参数**: 无
- **返回值**: `DeviceState` 枚举值
- **使用场景**: 查询设备当前所处状态（如空闲、监听、说话等）。
- **示例**:
```cpp
DeviceState state = Application::GetInstance().GetDeviceState();
```

#### `IsVoiceDetected()`
- **功能**: 检查是否检测到语音活动。
- **参数**: 无
- **返回值**: `bool`，`true` 表示检测到语音
- **使用场景**: 判断是否需要启动语音识别或响应。
- **示例**:
```cpp
if (Application::GetInstance().IsVoiceDetected()) {
    // 处理语音输入
}
```

#### `Schedule(std::function<void()> callback)`
- **功能**: 在主线程中调度一个回调函数执行。
- **参数**: `callback` - 要执行的函数对象
- **返回值**: 无
- **使用场景**: 从其他线程安全地调度UI更新或主逻辑操作。
- **内存管理**: 回调函数的生命周期由调用者管理。
- **示例**:
```cpp
Application::GetInstance().Schedule([]() {
    ESP_LOGI("APP", "This runs in main thread");
});
```

#### `SetDeviceState(DeviceState state)`
- **功能**: 设置设备状态并触发相应逻辑。
- **参数**: `state` - 目标设备状态
- **返回值**: 无
- **使用场景**: 状态机转换，如从监听状态切换到说话状态。
- **示例**:
```cpp
Application::GetInstance().SetDeviceState(kDeviceStateSpeaking);
```

#### `Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "")`
- **功能**: 显示系统警报。
- **参数**: 
  - `status`: 状态码
  - `message`: 显示消息
  - `emotion`: 情感标识（可选）
  - `sound`: 提示音资源（可选）
- **返回值**: 无
- **使用场景**: 系统错误提示或重要状态通知。
- **示例**:
```cpp
Application::GetInstance().Alert("ERROR", "网络连接失败", "sad", "error.wav");
```

#### `DismissAlert()`
- **功能**: 消除当前显示的警报。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 用户确认或问题解决后清除警报。
- **示例**:
```cpp
Application::GetInstance().DismissAlert();
```

#### `AbortSpeaking(AbortReason reason)`
- **功能**: 中断当前的语音播放。
- **参数**: `reason` - 中断原因
- **返回值**: 无
- **使用场景**: 用户按键取消或检测到唤醒词时中断语音输出。
- **示例**:
```cpp
Application::GetInstance().AbortSpeaking(kAbortReasonWakeWordDetected);
```

#### `ToggleChatState()`
- **功能**: 切换聊天状态（开始/停止监听）。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 用户手动触发语音交互。
- **示例**:
```cpp
Application::GetInstance().ToggleChatState();
```

#### `StartListening()`
- **功能**: 开始语音监听。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 启动语音识别流程。
- **示例**:
```cpp
Application::GetInstance().StartListening();
```

#### `StopListening()`
- **功能**: 正常停止语音监听。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 完成语音输入后停止录音。
- **示例**:
```cpp
Application::GetInstance().StopListening();
```

#### `StopListeningFast(bool close_channel_after = false)`
- **功能**: 快速停止监听，立即返回空闲状态。
- **参数**: `close_channel_after` - 是否在停止后关闭音频通道
- **返回值**: 无
- **使用场景**: 需要快速响应的用户交互场景。
- **示例**:
```cpp
Application::GetInstance().StopListeningFast(true);
```

#### `UpdateIotStates()`
- **功能**: 更新所有IoT设备的状态并同步到云端。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 定期或事件触发时同步设备状态。
- **示例**:
```cpp
Application::GetInstance().UpdateIotStates();
```

#### `Reboot()`
- **功能**: 重启设备。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 系统升级或严重错误恢复。
- **示例**:
```cpp
Application::GetInstance().Reboot();
```

#### `WakeWordInvoke(const std::string& wake_word)`
- **功能**: 唤醒词触发回调。
- **参数**: `wake_word` - 检测到的唤醒词
- **返回值**: 无
- **使用场景**: 唤醒词检测模块通知应用层。
- **示例**:
```cpp
Application::GetInstance().WakeWordInvoke("小智同学");
```

#### `PlaySound(const std::string_view& sound)`
- **功能**: 播放指定音效。
- **参数**: `sound` - 音效资源标识
- **返回值**: 无
- **使用场景**: 播放提示音、按键音等。
- **示例**:
```cpp
Application::GetInstance().PlaySound("button_click.wav");
```

#### `CanEnterSleepMode()`
- **功能**: 检查设备是否可以进入睡眠模式。
- **参数**: 无
- **返回值**: `bool`，`true` 表示可以睡眠
- **使用场景**: 电源管理决策。
- **示例**:
```cpp
if (Application::GetInstance().CanEnterSleepMode()) {
    EnterSleepMode();
}
```

#### `GetProtocol()`
- **功能**: 获取协议实例。
- **参数**: 无
- **返回值**: `Protocol&`，协议对象引用
- **内存管理**: 返回引用，无需释放。
- **使用场景**: 直接与通信协议层交互。
- **示例**:
```cpp
Protocol& protocol = Application::GetInstance().GetProtocol();
```

#### `GetOta()`
- **功能**: 获取OTA升级管理实例。
- **参数**: 无
- **返回值**: `Ota&`，OTA对象引用
- **内存管理**: 返回引用，无需释放。
- **使用场景**: 执行固件升级操作。
- **示例**:
```cpp
Ota& ota = Application::GetInstance().GetOta();
```

#### `ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples)`
- **功能**: 读取原始音频数据（用于声波配网）。
- **参数**: 
  - `data`: 输出音频数据缓冲区
  - `sample_rate`: 采样率
  - `samples`: 样本数量
- **返回值**: 无
- **使用场景**: 声波配网功能获取音频数据。
- **示例**:
```cpp
std::vector<int16_t> audio_buffer(16000);
Application::GetInstance().ReadAudio(audio_buffer, 16000, 16000);
```

#### `SetImageResourceCallback(std::function<void()> callback)`
- **功能**: 设置图片资源状态变化的回调。
- **参数**: `callback` - 回调函数
- **返回值**: 无
- **使用场景**: 监听图片内存使用情况。
- **示例**:
```cpp
Application::GetInstance().SetImageResourceCallback([]() {
    ESP_LOGI("APP", "Image resource changed");
});
```

#### `IsOtaCheckCompleted()`
- **功能**: 检查OTA检查是否已完成。
- **参数**: 无
- **返回值**: `bool`，`true` 表示已完成
- **使用场景**: 启动流程中判断是否已检查过更新。
- **示例**:
```cpp
if (!Application::GetInstance().IsOtaCheckCompleted()) {
    // 执行OTA检查
}
```

#### `PauseAudioProcessing()`
- **功能**: 暂停音频处理。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 图片播放等高负载操作时降低音频优先级。
- **示例**:
```cpp
Application::GetInstance().PauseAudioProcessing();
```

#### `ResumeAudioProcessing()`
- **功能**: 恢复音频处理。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 高负载操作完成后恢复音频处理。
- **示例**:
```cpp
Application::GetInstance().ResumeAudioProcessing();
```

#### `IsAudioQueueEmpty()`
- **功能**: 检查音频队列是否为空。
- **参数**: 无
- **返回值**: `bool`，`true` 表示为空
- **使用场景**: 判断开机提示音是否播放完成。
- **示例**:
```cpp
if (Application::GetInstance().IsAudioQueueEmpty()) {
    // 开机提示音播放完成
}
```

#### `DiscardPendingAudioForAlarm()`
- **功能**: 闹钟预处理：停止音频录制并丢弃已收集的音频数据。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 闹钟触发前清理音频缓冲区。
- **示例**:
```cpp
Application::GetInstance().DiscardPendingAudioForAlarm();
```

#### `SendAlarmMessage()`
- **功能**: 发送闹钟触发消息。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 闹钟时间到达时通知系统。
- **示例**:
```cpp
Application::GetInstance().SendAlarmMessage();
```

#### `GetDeviceConfig()`
- **功能**: 获取设备配置信息。
- **参数**: 无
- **返回值**: `const DeviceConfig&`，设备配置常量引用
- **内存管理**: 返回引用，无需释放。
- **使用场景**: 获取MQTT连接参数等配置信息。
- **示例**:
```cpp
const DeviceConfig& config = Application::GetInstance().GetDeviceConfig();
ESP_LOGI("APP", "MQTT Host: %s", config.mqtt_host.c_str());
```

#### `OnMqttNotification(const cJSON* root)`
- **功能**: MQTT通知回调处理。
- **参数**: `root` - JSON格式的通知数据
- **返回值**: 无
- **使用场景**: 处理来自MQTT服务器的异步通知。
- **示例**:
```cpp
// 通常由MQTT客户端内部调用
```

#### `IsAudioActivityHigh()`
- **功能**: 检查音频活动是否处于高水平。
- **参数**: 无
- **返回值**: `bool`，`true` 表示音频活动高
- **使用场景**: 电源管理和资源调度决策。
- **示例**:
```cpp
if (Application::GetInstance().IsAudioActivityHigh()) {
    // 降低其他任务优先级
}
```

#### `IsAudioProcessingCritical()`
- **功能**: 检查音频处理是否处于关键状态。
- **参数**: 无
- **返回值**: `bool`，`true` 表示关键状态
- **使用场景**: 确保关键音频处理不被中断。
- **示例**:
```cpp
if (Application::GetInstance().IsAudioProcessingCritical()) {
    // 保持高性能模式
}
```

#### `SetAudioPriorityMode(bool enabled)`
- **功能**: 设置音频优先级模式。
- **参数**: `enabled` - 是否启用高优先级模式
- **返回值**: 无
- **使用场景**: 动态调整音频处理的系统优先级。
- **示例**:
```cpp
Application::GetInstance().SetAudioPriorityMode(true);
```

#### `GetAudioPerformanceScore()`
- **功能**: 获取音频性能评分。
- **参数**: 无
- **返回值**: `int`，性能评分（0-100）
- **使用场景**: 性能监控和诊断。
- **示例**:
```cpp
int score = Application::GetInstance().GetAudioPerformanceScore();
```

#### `GetAudioActivityLevel()`
- **功能**: 获取音频活动等级。
- **参数**: 无
- **返回值**: `AudioActivityLevel` 枚举值
- **使用场景**: 智能分级音频保护机制。
- **示例**:
```cpp
AudioActivityLevel level = Application::GetInstance().GetAudioActivityLevel();
```

#### `IsRealAudioProcessing()`
- **功能**: 检查是否正在进行真实的音频处理（而非测试或模拟）。
- **参数**: 无
- **返回值**: `bool`，`true` 表示真实处理
- **使用场景**: 区分真实音频流和测试数据。
- **示例**:
```cpp
if (Application::GetInstance().IsRealAudioProcessing()) {
    // 处理真实音频数据
}
```

#### `StartMqttNotifier()`
- **功能**: 启动MQTT通知服务，用于从省电模式恢复。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 当设备从省电模式唤醒时，重新启动MQTT通知服务以接收云端消息。
- **示例**:
```cpp
Application::GetInstance().StartMqttNotifier();
```

#### `StopMqttNotifier()`
- **功能**: 停止MQTT通知服务，用于省电模式。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 进入省电模式前安全关闭MQTT连接，节省电力。
- **示例**:
```cpp
Application::GetInstance().StopMqttNotifier();
```

**Section sources**
- [application.h](file://main/application.h#L1-L238) - *在最近的提交中更新*

## Board抽象接口

`Board` 类是一个抽象基类，定义了不同硬件平台的统一接口。通过工厂模式 `create_board()` 创建具体实例，实现硬件抽象。

### 虚函数规范

#### `GetBoardType()`
- **功能**: 获取板型标识字符串。
- **参数**: 无
- **返回值**: `std::string`，板型名称
- **使用场景**: 设备识别和配置加载。
- **示例**:
```cpp
Board& board = Board::GetInstance();
std::string type = board.GetBoardType();
```

#### `GetUuid()`
- **功能**: 获取设备唯一标识符。
- **参数**: 无
- **返回值**: `std::string`，UUID字符串
- **使用场景**: 设备身份认证和云端注册。
- **示例**:
```cpp
std::string uuid = board.GetUuid();
```

#### `GetBacklight()`
- **功能**: 获取背光控制实例。
- **参数**: 无
- **返回值**: `Backlight*`，背光对象指针，可能为`nullptr`
- **内存管理**: 指针由Board管理，调用者无需释放。
- **使用场景**: 屏幕背光亮度调节。
- **示例**:
```cpp
Backlight* backlight = board.GetBacklight();
if (backlight) {
    backlight->SetBrightness(50);
}
```

#### `GetAudioCodec()`
- **功能**: 获取音频编解码器实例。
- **参数**: 无
- **返回值**: `AudioCodec*`，音频编解码器指针
- **内存管理**: 指针由Board管理，调用者无需释放。
- **使用场景**: 音频输入输出设备控制。
- **示例**:
```cpp
AudioCodec* codec = board.GetAudioCodec();
codec->StartRecord();
```

#### `GetDisplay()`
- **功能**: 获取显示设备实例。
- **参数**: 无
- **返回值**: `Display*`，显示设备指针
- **内存管理**: 指针由Board管理，调用者无需释放。
- **使用场景**: 屏幕内容渲染。
- **示例**:
```cpp
Display* display = board.GetDisplay();
display->DrawString("Hello World");
```

#### `CreateHttp()`
- **功能**: 创建HTTP客户端实例。
- **参数**: 无
- **返回值**: `Http*`，HTTP客户端指针
- **内存管理**: 调用者负责释放返回的对象。
- **使用场景**: HTTP网络请求。
- **示例**:
```cpp
Http* http = board.CreateHttp();
http->Get("http://example.com");
delete http; // 必须释放
```

#### `CreateWebSocket()`
- **功能**: 创建WebSocket客户端实例。
- **参数**: 无
- **返回值**: `WebSocket*`，WebSocket客户端指针
- **内存管理**: 调用者负责释放返回的对象。
- **使用场景**: WebSocket长连接通信。
- **示例**:
```cpp
WebSocket* ws = board.CreateWebSocket();
ws->Connect("ws://example.com");
delete ws; // 必须释放
```

#### `CreateMqtt()`
- **功能**: 创建MQTT客户端实例。
- **参数**: 无
- **返回值**: `Mqtt*`，MQTT客户端指针
- **内存管理**: 调用者负责释放返回的对象。
- **使用场景**: MQTT协议通信。
- **示例**:
```cpp
Mqtt* mqtt = board.CreateMqtt();
mqtt->Connect(config.mqtt_host.c_str(), config.mqtt_port);
delete mqtt; // 必须释放
```

#### `CreateUdp()`
- **功能**: 创建UDP客户端实例。
- **参数**: 无
- **返回值**: `Udp*`，UDP客户端指针
- **内存管理**: 调用者负责释放返回的对象。
- **使用场景**: UDP网络通信。
- **示例**:
```cpp
Udp* udp = board.CreateUdp();
udp->Send("192.168.1.1", 1234, "data");
delete udp; // 必须释放
```

#### `StartNetwork()`
- **功能**: 启动网络连接（Wi-Fi等）。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 系统初始化时连接网络。
- **示例**:
```cpp
board.StartNetwork();
```

#### `GetNetworkStateIcon()`
- **功能**: 获取网络状态图标。
- **参数**: 无
- **返回值**: `const char*`，图标字符指针
- **使用场景**: UI显示网络连接状态。
- **示例**:
```cpp
const char* icon = board.GetNetworkStateIcon();
```

#### `GetBatteryLevel(int &level, bool& charging, bool& discharging)`
- **功能**: 获取电池电量信息。
- **参数**: 
  - `level`: 输出电量百分比（0-100）
  - `charging`: 输出是否正在充电
  - `discharging`: 输出是否正在放电
- **返回值**: `bool`，`true` 表示获取成功
- **使用场景**: 电池状态监控。
- **示例**:
```cpp
int level;
bool charging, discharging;
if (board.GetBatteryLevel(level, charging, discharging)) {
    ESP_LOGI("BOARD", "Battery: %d%%", level);
}
```

#### `SetPowerSaveMode(bool enabled)`
- **功能**: 设置省电模式。
- **参数**: `enabled` - 是否启用省电模式
- **返回值**: 无
- **使用场景**: 电源管理，延长电池寿命。
- **示例**:
```cpp
board.SetPowerSaveMode(true);
```

**Section sources**
- [board.h](file://main/boards/common/board.h#L1-L56)

## Thing基类

`Thing` 类是IoT设备的抽象基类，采用属性-方法模型来描述设备功能，支持动态注册和创建。

### 生命周期方法

#### 构造函数 `Thing(const std::string& name, const std::string& description)`
- **功能**: 初始化Thing对象。
- **参数**: 
  - `name`: 设备名称
  - `description`: 设备描述
- **使用场景**: 派生类构造函数中调用。
- **示例**:
```cpp
class MyLamp : public Thing {
public:
    MyLamp() : Thing("lamp", "智能台灯") {}
};
```

#### 析构函数 `virtual ~Thing()`
- **功能**: 虚析构函数，确保正确销毁派生类对象。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 对象销毁时自动调用。
- **注意**: 必须为虚函数以支持多态。

### 事件回调机制

#### `GetDescriptorJson()`
- **功能**: 获取设备描述符的JSON表示。
- **参数**: 无
- **返回值**: `std::string`，JSON格式的描述符
- **使用场景**: 设备自描述和云端注册。
- **示例**:
```cpp
std::string descriptor = thing.GetDescriptorJson();
```

#### `GetStateJson()`
- **功能**: 获取设备当前状态的JSON表示。
- **参数**: 无
- **返回值**: `std::string`，JSON格式的状态
- **使用场景**: 状态同步和查询。
- **示例**:
```cpp
std::string state = thing.GetStateJson();
```

#### `Invoke(const cJSON* command)`
- **功能**: 异步调用设备方法。
- **参数**: `command` - JSON格式的命令
- **返回值**: 无
- **使用场景**: 接收云端或本地命令并执行。
- **示例**:
```cpp
cJSON* cmd = cJSON_Parse("{\"method\":\"turn_on\",\"params\":{}}");
thing.Invoke(cmd);
cJSON_Delete(cmd);
```

#### `InvokeSync(const cJSON* command, std::string* error)`
- **功能**: 同步调用设备方法。
- **参数**: 
  - `command`: JSON格式的命令
  - `error`: 输出错误信息（可选）
- **返回值**: `bool`，`true` 表示成功
- **使用场景**: 需要立即知道执行结果的场景。
- **示例**:
```cpp
std::string error;
bool success = thing.InvokeSync(cmd, &error);
```

### 属性系统

`Property` 类用于定义设备的可读属性，支持布尔、数字和字符串三种类型，通过回调函数获取实时值。

#### `AddBooleanProperty`
- **功能**: 添加布尔属性。
- **参数**: 名称、描述、获取值的回调函数
- **示例**:
```cpp
properties_.AddBooleanProperty("on", "灯是否开启", [this]() { return is_on_; });
```

#### `AddNumberProperty`
- **功能**: 添加数字属性。
- **参数**: 名称、描述、获取值的回调函数
- **示例**:
```cpp
properties_.AddNumberProperty("brightness", "亮度", [this]() { return brightness_; });
```

#### `AddStringProperty`
- **功能**: 添加字符串属性。
- **参数**: 名称、描述、获取值的回调函数
- **示例**:
```cpp
properties_.AddStringProperty("color", "颜色", [this]() { return color_; });
```

### 方法系统

`Method` 类用于定义设备的可调用方法，支持参数和回调。

#### `AddMethod`
- **功能**: 添加可调用方法。
- **参数**: 方法名、描述、参数列表、执行回调
- **示例**:
```cpp
ParameterList params;
params.AddParameter(Parameter("on", "是否开启", kValueTypeBoolean));
methods_.AddMethod("turn_on", "开启灯", params, [this](const ParameterList& params) {
    is_on_ = params["on"].boolean();
});
```

### 动态注册机制

使用 `DECLARE_THING` 宏实现类型的自动注册和创建。

#### `RegisterThing`
- **功能**: 注册新的Thing类型。
- **参数**: 类型名称、创建函数
- **使用场景**: 系统启动时注册所有支持的设备类型。
- **示例**:
```cpp
DECLARE_THING(Lamp)
DECLARE_THING(Speaker)
```

#### `CreateThing`
- **功能**: 根据类型名称创建Thing实例。
- **参数**: 类型名称
- **返回值**: `Thing*`，新创建的实例指针
- **内存管理**: 调用者负责释放返回的对象。
- **使用场景**: 动态创建设备实例。
- **示例**:
```cpp
Thing* lamp = CreateThing("Lamp");
// 使用后需 delete lamp;
```

**Section sources**
- [thing.h](file://main/iot/thing.h#L1-L301)

## Protocol通用协议接口

`Protocol` 类定义了通信协议的通用接口，支持音频和文本数据的双向传输。

### 虚函数规范

#### `Start()`
- **功能**: 启动协议连接。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 建立与服务器的连接。
- **示例**:
```cpp
protocol.Start();
```

#### `OpenAudioChannel()`
- **功能**: 打开音频通信通道。
- **参数**: 无
- **返回值**: `bool`，`true` 表示成功
- **使用场景**: 开始语音流传输。
- **示例**:
```cpp
if (protocol.OpenAudioChannel()) {
    // 开始发送音频数据
}
```

#### `CloseAudioChannel()`
- **功能**: 关闭音频通信通道。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 结束语音流传输。
- **示例**:
```cpp
protocol.CloseAudioChannel();
```

#### `IsAudioChannelOpened()`
- **功能**: 检查音频通道是否已打开。
- **参数**: 无
- **返回值**: `bool`，`true` 表示已打开
- **使用场景**: 状态检查和条件判断。
- **示例**:
```cpp
if (protocol.IsAudioChannelOpened()) {
    // 可以发送音频
}
```

#### `SendAudio(const std::vector<uint8_t>& data)`
- **功能**: 发送音频数据。
- **参数**: `data` - 音频数据缓冲区
- **返回值**: 无
- **使用场景**: 上传录制的语音到服务器。
- **示例**:
```cpp
std::vector<uint8_t> audio_data = EncodeAudio();
protocol.SendAudio(audio_data);
```

#### `SendText(const std::string& text)`
- **功能**: 发送文本消息。
- **参数**: `text` - 文本内容
- **返回值**: `bool`，`true` 表示成功
- **使用场景**: 文本聊天或命令传输。
- **示例**:
```cpp
protocol.SendText("Hello World");
```

### 事件回调设置

#### `OnIncomingAudio`
- **功能**: 设置音频数据到达的回调。
- **参数**: 回调函数，接收音频数据
- **使用场景**: 处理服务器下发的音频流。
- **示例**:
```cpp
protocol.OnIncomingAudio([](std::vector<uint8_t>&& data) {
    PlayAudio(data);
});
```

#### `OnIncomingJson`
- **功能**: 设置JSON数据到达的回调。
- **参数**: 回调函数，接收JSON根节点
- **使用场景**: 处理服务器下发的控制命令。
- **示例**:
```cpp
protocol.OnIncomingJson([](const cJSON* root) {
    ProcessCommand(root);
});
```

#### `OnAudioChannelOpened`
- **功能**: 设置音频通道打开的回调。
- **参数**: 回调函数
- **使用场景**: 音频通道就绪后开始发送数据。
- **示例**:
```cpp
protocol.OnAudioChannelOpened([]() {
    ESP_LOGI("PROTO", "音频通道已打开");
});
```

#### `OnAudioChannelClosed`
- **功能**: 设置音频通道关闭的回调。
- **参数**: 回调函数
- **使用场景**: 清理音频相关资源。
- **示例**:
```cpp
protocol.OnAudioChannelClosed([]() {
    ESP_LOGI("PROTO", "音频通道已关闭");
});
```

#### `OnNetworkError`
- **功能**: 设置网络错误的回调。
- **参数**: 回调函数，接收错误消息
- **使用场景**: 错误处理和重连逻辑。
- **示例**:
```cpp
protocol.OnNetworkError([](const std::string& message) {
    ESP_LOGE("PROTO", "网络错误: %s", message.c_str());
});
```

### 辅助方法

#### `server_sample_rate()`
- **功能**: 获取服务器期望的采样率。
- **参数**: 无
- **返回值**: `int`，采样率（Hz）
- **使用场景**: 音频重采样配置。
- **示例**:
```cpp
int rate = protocol.server_sample_rate();
```

#### `server_frame_duration()`
- **功能**: 获取服务器期望的帧时长。
- **参数**: 无
- **返回值**: `int`，帧时长（ms）
- **使用场景**: 音频编码分帧。
- **示例**:
```cpp
int duration = protocol.server_frame_duration();
```

#### `session_id()`
- **功能**: 获取当前会话ID。
- **参数**: 无
- **返回值**: `const std::string&`，会话ID
- **使用场景**: 会话跟踪和日志记录。
- **示例**:
```cpp
std::string sid = protocol.session_id();
```

#### `HasError()`
- **功能**: 检查协议层是否发生错误。
- **参数**: 无
- **返回值**: `bool`，`true` 表示有错误
- **使用场景**: 错误状态检查。
- **示例**:
```cpp
if (protocol.HasError()) {
    // 处理错误
}
```

#### `SendWakeWordDetected`
- **功能**: 发送唤醒词检测事件。
- **参数**: `wake_word` - 检测到的唤醒词
- **返回值**: 无
- **使用场景**: 通知服务器设备已被唤醒。
- **示例**:
```cpp
protocol.SendWakeWordDetected("小智同学");
```

#### `SendStartListening`
- **功能**: 发送开始监听事件。
- **参数**: `mode` - 监听模式
- **返回值**: 无
- **使用场景**: 通知服务器开始语音输入。
- **示例**:
```cpp
protocol.SendStartListening(kListeningModeAutoStop);
```

#### `SendStopListening`
- **功能**: 发送停止监听事件。
- **参数**: 无
- **返回值**: 无
- **使用场景**: 通知服务器结束语音输入。
- **示例**:
```cpp
protocol.SendStopListening();
```

#### `SendAbortSpeaking`
- **功能**: 发送中断说话事件。
- **参数**: `reason` - 中断原因
- **返回值**: 无
- **使用场景**: 通知服务器中断语音响应。
- **示例**:
```cpp
protocol.SendAbortSpeaking(kAbortReasonWakeWordDetected);
```

#### `SendIotDescriptors`
- **功能**: 发送IoT设备描述符。
- **参数**: `descriptors` - JSON格式的描述符
- **返回值**: 无
- **使用场景**: 设备自注册和发现。
- **示例**:
```cpp
protocol.SendIotDescriptors(thing.GetDescriptorJson());
```

#### `SendIotStates`
- **功能**: 发送IoT设备状态。
- **参数**: `states` - JSON格式的状态
- **返回值**: 无
- **使用场景**: 状态同步到云端。
- **示例**:
```cpp
protocol.SendIotStates(thing.GetStateJson());
```

**Section sources**
- [protocol.h](file://main/protocols/protocol.h#L1-L80)