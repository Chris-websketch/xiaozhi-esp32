# abrobot-1.28tft-wifi 板子内存与任务分配架构

## 硬件配置

| 项目 | 规格 |
|------|------|
| **MCU** | ESP32-S3 双核 240MHz |
| **内部SRAM** | ~320KB |
| **PSRAM** | 8MB (Octal PSRAM) |
| **Flash** | 16MB |
| **显示屏** | 1.28寸 GC9A01 圆形LCD (240×240) |

---

## FreeRTOS 任务分配

### 任务总览

| 任务名 | 优先级 | 栈大小 | CPU亲和性 | 功能描述 |
|--------|--------|--------|-----------|----------|
| `audio_loop` | 9 (最高) | 8KB | Core 1 | 音频编解码循环 |
| `main_loop` | 4 | 8KB | Core 0 | 主事件循环 |
| `audio_communication` | 3 | 4KB | 任意 | AFE音频前端处理 |
| `mqtt_heartbeat` | 3 | 4KB | 任意 | MQTT心跳上报 |
| `img_resource_check` | 3 | 8KB | 任意 | 图片资源检查下载 |
| `background_task` | 2 | 8KB | 任意 | 后台异步操作 |
| `img_slideshow` | 2* | 8KB | 任意 | 图片轮播动画 |
| `acoustic_wifi_config` | 5 | 8KB | 任意 | 声波配网 (可选) |
| `nimble_host` | 21 | 3KB | 任意 | NimBLE蓝牙主机 (配网时) |
| `wake_word_encode` | - | 32KB (PSRAM) | 任意 | 唤醒词编码 (静态创建) |

> *`img_slideshow` 任务初始优先级为1，启动后动态调整为2

---

## 任务详细说明

### 1. 音频循环任务 (`audio_loop`)

```
位置: main/application.cc:560-564
优先级: 9 (系统最高)
栈大小: 8KB (4096*2)
CPU: Core 1 固定绑定
```

**职责**:
- Opus 音频编解码
- 音频数据收发
- 实时语音处理

**关键代码**:
```cpp
xTaskCreatePinnedToCore([](void* arg) {
    Application* app = (Application*)arg;
    app->AudioLoop();
    vTaskDelete(NULL);
}, "audio_loop", 4096 * 2, this, 9, &audio_loop_task_handle_, 1);
```

### 2. 主循环任务 (`main_loop`)

```
位置: main/application.cc:567-571
优先级: 4
栈大小: 8KB (4096*2)
CPU: Core 0 固定绑定
```

**职责**:
- 设备状态管理
- 事件处理分发
- UI更新协调

### 3. 音频处理任务 (`audio_communication`)

```
位置: main/audio_processing/audio_processor.cc:56-60
优先级: 3
栈大小: 4KB
```

**职责**:
- AFE (Audio Front End) 处理
- 音频降噪
- 回声消除
- 自动增益控制

### 4. MQTT心跳任务 (`mqtt_heartbeat`)

```
位置: main/notifications/mqtt_notifier.cc:208-307
优先级: 3
栈大小: 4KB
心跳间隔: 10秒
```

**职责**:
- 定期上报设备状态
- 收集并发送系统指标:
  - 内存使用情况
  - 电池状态
  - WiFi信号强度 (RSSI)
  - IoT设备状态

**上报数据结构**:
```json
{
  "memory": {
    "free_sram": 123456,
    "min_free_sram": 100000
  },
  "wifi": {
    "rssi": -45
  },
  "battery": { ... },
  "iot_states": [ ... ]
}
```

### 5. 图片轮播任务 (`img_slideshow`)

```
位置: main/boards/moon/abrobot-1.28tft-wifi.cc:2493
初始优先级: 1 → 动态调整为 2
栈大小: 8KB
```

**职责**:
- 背景图片循环显示
- 动画效果播放
- 时钟页面切换

**智能音频保护机制**:

任务根据音频活动级别动态调整行为：

| 音频级别 | 行为 |
|----------|------|
| `AUDIO_IDLE` | 正常播放 |
| `AUDIO_STANDBY` | 降低帧率 |
| `AUDIO_ACTIVE` | 进一步降低帧率 |
| `AUDIO_CRITICAL` | 完全暂停 |

```cpp
switch (audioLevel) {
    case Application::AUDIO_IDLE:
        // 正常播放
        break;
    case Application::AUDIO_STANDBY:
        // 降低帧率
        break;
    case Application::AUDIO_ACTIVE:
        // 进一步降低
        break;
    case Application::AUDIO_CRITICAL:
        shouldPauseCompletely = true;
        break;
}
```

### 6. 后台任务 (`background_task`)

```
位置: main/background_task.cc:8-12
优先级: 2
栈大小: 8KB (默认)
```

**职责**:
- 异步操作调度
- 非实时任务处理

### 7. 唤醒词编码任务 (`wake_word_encode`)

```
位置: main/audio_processing/wake_word_detect.cc:143-149
栈大小: 32KB (PSRAM静态分配)
创建方式: xTaskCreateStatic
```

**职责**:
- 唤醒词音频编码
- Opus 16kHz/60ms帧编码

### 8. NimBLE蓝牙主机任务 (`nimble_host`) - 配网模式

```
位置: ESP-IDF bt组件 (nimble_port_freertos)
优先级: 21 (configMAX_PRIORITIES - 1)
栈大小: 3KB (CONFIG_BT_NIMBLE_TASK_STACK_SIZE)
启用条件: CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING
```

**职责**:
- BLE GATT服务
- BluFi配网协议处理
- 蓝牙连接管理

---

## 蓝牙配网 (BluFi) 内存分析

### NimBLE vs Bluedroid 对比

| 蓝牙堆栈 | 内存占用 | 说明 |
|----------|----------|------|
| **Bluedroid** | ~100-120KB | 功能完整但内存占用大 |
| **NimBLE** | ~50-60KB | 轻量级，适合资源受限设备 |
| **节省** | **~50-60KB** | 切换到NimBLE后节省的内存 |

### NimBLE 配置优化

```
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1       # 最小连接数
CONFIG_BT_NIMBLE_MAX_BONDS=1             # 最小配对数
CONFIG_BT_NIMBLE_MAX_CCCDS=4             # CCC描述符数量
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y       # 只启用外设角色
CONFIG_BT_NIMBLE_ROLE_CENTRAL=n          # 禁用中心角色
CONFIG_BT_NIMBLE_ROLE_OBSERVER=n         # 禁用观察者
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=n      # 禁用广播者
CONFIG_BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL=y  # 使用PSRAM
CONFIG_BT_NIMBLE_TASK_STACK_SIZE=3072    # 任务栈3KB
CONFIG_BT_NIMBLE_BLUFI_ENABLE=y          # 启用BluFi
```

### 配网方式内存对比

| 配网方式 | 额外内存消耗 | 备注 |
|----------|-------------|------|
| AP配网 (HTTP) | ~30-40KB | WiFi SoftAP + HTTP服务器 |
| 蓝牙配网 (NimBLE) | ~50-60KB | NimBLE堆栈 |
| 蓝牙配网 (Bluedroid) | ~100-120KB | ❌ 内存不足 |

> ⚠️ BluFi配网与AP配网**互斥使用**，避免同时运行导致内存不足

### 关键代码位置

| 功能 | 文件路径 |
|------|----------|
| BluFi初始化 | `main/boards/common/blufi.cpp` |
| BluFi头文件 | `main/boards/common/blufi.h` |
| 配网模式选择 | `main/boards/common/wifi_board.cc:52-62` |
| Kconfig选项 | `main/Kconfig.projbuild` (CONFIG_USE_ESP_BLUFI_WIFI_PROVISIONING) |

---

## 内存分配策略

### PSRAM 使用 (8MB)

| 用途 | 大小 | 位置 |
|------|------|------|
| **壁纸数据** | 6×115.2KB = 691.2KB | `bg_images_data_[]` |
| **Canvas缓冲区** | 115.2KB | `Display::canvas_buffer_` |
| **LVGL双缓冲** | 2×115.2KB = 230.4KB | `esp_lvgl_port` |
| **AFE缓冲区** | 动态分配 | `esp_afe_sr` |
| **唤醒词任务栈** | 32KB | `wake_word_encode_task_stack_` |
| **图片资源缓冲区** | 动态分配 | `PackedLoader` |

### 内部SRAM 使用

| 用途 | 说明 |
|------|------|
| **FreeRTOS任务栈** | 除唤醒词任务外的其他任务 |
| **系统堆** | 小于4KB的动态分配 |
| **静态变量** | 全局和静态数据 |
| **DMA缓冲区** | 需要DMA访问的缓冲区 |

### 内存分配规则

```cpp
// 大于4KB的分配自动使用PSRAM
uint8_t* data = (uint8_t*)malloc(file_size);  // 115.2KB → PSRAM

// 显式指定PSRAM
void* buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

// 优先PSRAM，回退内部RAM
void* canvas = heap_caps_malloc(buf_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
```

---

## 任务优先级架构图

```
优先级
  9  ┌─────────────────────────┐
     │     audio_loop          │  ← Core 1 固定
     │     (音频编解码)          │
  8  └─────────────────────────┘
  7  
  6  
  5  ┌─────────────────────────┐
     │  acoustic_wifi_config   │  ← 配网时创建
     └─────────────────────────┘
  4  ┌─────────────────────────┐
     │      main_loop          │  ← Core 0 固定
     │     (主事件循环)          │
     └─────────────────────────┘
  3  ┌─────────────────────────┬─────────────────────────┐
     │  audio_communication    │    mqtt_heartbeat       │
     │     (AFE处理)           │    img_resource_check   │
     └─────────────────────────┴─────────────────────────┘
  2  ┌─────────────────────────┬─────────────────────────┐
     │   background_task       │    img_slideshow        │
     │     (后台任务)           │    (图片轮播)            │
     └─────────────────────────┴─────────────────────────┘
  1  ┌─────────────────────────┐
     │       IDLE              │
     └─────────────────────────┘
```

---

## 双核任务分配

```
┌────────────────────────────────────────────────────────────┐
│                      ESP32-S3 双核                          │
├────────────────────────────┬───────────────────────────────┤
│         Core 0             │           Core 1              │
├────────────────────────────┼───────────────────────────────┤
│  main_loop (优先级4)        │  audio_loop (优先级9)         │
│  - 设备状态管理              │  - Opus编解码                 │
│  - 事件处理                  │  - 音频收发                   │
│  - UI更新协调                │  - 实时语音处理               │
├────────────────────────────┼───────────────────────────────┤
│  其他任务 (无CPU亲和性)       │                               │
│  - mqtt_heartbeat          │                               │
│  - img_slideshow           │                               │
│  - background_task         │                               │
│  - audio_communication     │                               │
└────────────────────────────┴───────────────────────────────┘
```

---

## 显示系统内存布局

```
┌─────────────────────────────────────────────┐
│              PSRAM (8MB)                    │
├─────────────────────────────────────────────┤
│  壁纸1 (115.2KB) ─┐                         │
│  壁纸2 (115.2KB)  │                         │
│  壁纸3 (115.2KB)  ├── bg_images_data_[]     │
│  壁纸4 (115.2KB)  │   共 691.2KB            │
│  壁纸5 (115.2KB)  │                         │
│  壁纸6 (115.2KB) ─┘                         │
├─────────────────────────────────────────────┤
│  Canvas缓冲区 (115.2KB)                      │
│  240×240×2 = 115,200 bytes                  │
├─────────────────────────────────────────────┤
│  LVGL缓冲区1 (115.2KB)  ┐                   │
│  LVGL缓冲区2 (115.2KB)  ┴ 双缓冲            │
├─────────────────────────────────────────────┤
│  唤醒词任务栈 (32KB)                         │
├─────────────────────────────────────────────┤
│  AFE音频缓冲区 (动态)                        │
├─────────────────────────────────────────────┤
│  图片资源缓冲区 (动态)                       │
└─────────────────────────────────────────────┘
```

---

## 关键代码位置索引

| 功能 | 文件路径 | 行号 |
|------|----------|------|
| 音频任务创建 | `main/application.cc` | 560-564 |
| 主循环任务创建 | `main/application.cc` | 567-571 |
| 后台任务创建 | `main/background_task.cc` | 8-12 |
| 图片轮播任务 | `main/boards/moon/abrobot-1.28tft-wifi.cc` | 2493 |
| 音频优先级保护 | `main/boards/moon/abrobot-1.28tft-wifi.cc` | 2678-2706 |
| MQTT心跳任务 | `main/notifications/mqtt_notifier.cc` | 208-307 |
| Canvas缓冲区分配 | `main/display/display.cc` | 314 |
| 壁纸加载 | `main/boards/moon/abrobot-1.28tft-wifi.cc` | 291-309 |
| AFE缓冲区分配 | `managed_components/espressif__esp-sr/` | - |
| 唤醒词任务栈 | `main/audio_processing/wake_word_detect.cc` | 143-149 |
| BluFi初始化 | `main/boards/common/blufi.cpp` | 112-140 |
| BluFi事件处理 | `main/boards/common/blufi.cpp` | 450-650 |
| 配网模式入口 | `main/boards/common/wifi_board.cc` | 48-134 |

---

## 内存监控

系统通过以下方式监控内存使用：

```cpp
// 配网模式下每10秒打印一次
int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);

// MQTT心跳上报
cJSON_AddNumberToObject(memory, "free_sram", free_sram);
cJSON_AddNumberToObject(memory, "min_free_sram", min_free_sram);
```

---

*文档更新时间: 2025-12-15*
*适用板型: abrobot-1.28tft-wifi (moon)*
*蓝牙堆栈: NimBLE (已从Bluedroid切换以优化内存)*
