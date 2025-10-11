# LittleFS 迁移文档

## 概述

本项目已从 SPIFFS 迁移到 LittleFS 文件系统，以解决 SPIFFS 的碎片化问题并提升文件操作性能。

使用的 LittleFS 组件：**joltwallet/littlefs v1.20.1**

## 迁移日期

2025年10月11日

## 主要变更

### 1. 文件系统 API 替换

所有 SPIFFS API 已替换为对应的 LittleFS API：

| 原 SPIFFS API | 新 LittleFS API |
|--------------|----------------|
| `esp_vfs_spiffs_conf_t` | `esp_vfs_littlefs_conf_t` |
| `esp_vfs_spiffs_register()` | `esp_vfs_littlefs_register()` |
| `esp_vfs_spiffs_unregister()` | `esp_vfs_littlefs_unregister()` |
| `esp_spiffs_format()` | `esp_littlefs_format()` |
| `esp_spiffs_info()` | `esp_littlefs_info()` |

### 2. 修改的文件

#### 核心文件系统管理
- `main/image_resource/storage/spiffs_manager.cc`
  - 替换所有 SPIFFS API 为 LittleFS API
  - 简化 `OptimizeSpace()` 函数（移除手动 GC 逻辑）
  - 更新日志输出
  
- `main/image_resource/storage/spiffs_manager.h`
  - 更新注释说明已迁移到 LittleFS
  - 保持类名 `SpiffsManager` 不变（兼容性考虑）

#### 打包加载器
- `main/image_resource/loader/packed_loader.cc`
  - 移除 `#include <esp_spiffs.h>`
  - 简化 `TriggerLightGC()` 函数（LittleFS 自动管理碎片）
  - 更新日志输出
  
- `main/image_resource/loader/packed_loader.h`
  - 更新 `TriggerLightGC()` 注释

#### 板级配置
- `main/boards/kevin-box-1/kevin_box_board.cc`
  - 替换 `esp_vfs_spiffs_conf_t` 为 `esp_vfs_littlefs_conf_t`
  - 替换 `esp_vfs_spiffs_register()` 为 `esp_vfs_littlefs_register()`

#### 分区表
- `partitions.csv`
- `partitions_8M.csv`
- `partitions_32M_sensecap.csv`
  - 添加注释说明分区已使用 LittleFS
  - SubType 保持为 `spiffs`（LittleFS 兼容）

## LittleFS 优势

### 1. 更好的碎片管理
- **自动磨损均衡**：LittleFS 内置智能磨损均衡算法
- **动态空间回收**：无需手动垃圾回收
- **稳定的性能**：即使在重复删除-写入场景下，性能也保持稳定

### 2. 性能提升
- **更快的写入速度**：特别是在更换资源重新打包时
- **更少的延迟**：不需要等待 GC 完成
- **更高的效率**：文件系统开销更小

### 3. 更现代的设计
- **专为嵌入式设计**：比 SPIFFS 更适合 Flash 存储
- **ESP-IDF 官方推荐**：ESP-IDF 5.x+ 推荐使用 LittleFS

## 兼容性说明

### 保留的接口
- `SpiffsManager` 类名保持不变
- `OptimizeSpace()` 和 `TriggerLightGC()` 接口保留（内部简化）
- 分区表 SubType 保持为 `spiffs`

### 不兼容的地方
- **现有 SPIFFS 分区数据将丢失**
- 首次使用需要格式化分区
- 旧的 SPIFFS 镜像无法直接使用

## 测试建议

### 基本功能测试
1. **分区挂载测试**
   - 验证 `model` 和 `resources` 分区能正常挂载
   - 检查挂载日志

2. **文件操作测试**
   - 创建、读取、写入、删除文件
   - 验证文件完整性

3. **打包性能测试**
   - 第一次打包速度
   - 更换资源后重新打包速度
   - 对比 SPIFFS 时的性能

### 压力测试
1. **重复打包测试**
   - 连续多次更换资源并重新打包
   - 验证性能不会随时间下降

2. **空间管理测试**
   - 填满分区后删除文件
   - 验证空间能正确回收

3. **断电恢复测试**
   - 在写入过程中模拟断电
   - 验证文件系统能正确恢复

## 回滚方案

如果需要回滚到 SPIFFS：

1. 恢复以下文件的 Git 历史版本：
   - `main/image_resource/storage/spiffs_manager.cc`
   - `main/image_resource/storage/spiffs_manager.h`
   - `main/image_resource/loader/packed_loader.cc`
   - `main/image_resource/loader/packed_loader.h`
   - `main/boards/kevin-box-1/kevin_box_board.cc`

2. 移除分区表中的 LittleFS 注释

3. 重新格式化分区为 SPIFFS 格式

## 注意事项

1. **首次烧录后需要格式化分区**
   - LittleFS 会在首次挂载时自动格式化（如果 `format_if_mount_failed = true`）

2. **备份重要数据**
   - 迁移前备份所有重要文件
   - LittleFS 无法读取 SPIFFS 格式的数据

3. **测试所有功能**
   - 全面测试文件操作相关功能
   - 特别关注图片打包和资源更新流程

4. **监控日志**
   - 注意观察文件系统相关的错误日志
   - 检查空间使用情况

## 已知问题

目前无已知问题。

## 参考资料

- [ESP-IDF LittleFS 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/vfs.html#littlefs-filesystem)
- [LittleFS GitHub](https://github.com/littlefs-project/littlefs)
- [SPIFFS vs LittleFS 对比](https://github.com/espressif/esp-idf/tree/master/examples/storage/littlefs)

## 维护者

如有问题请联系项目维护团队。
