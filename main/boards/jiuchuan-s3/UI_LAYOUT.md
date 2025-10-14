# Jiuchuan-S3 UI布局说明

## UI层级结构

从底层到顶层：

```
lv_scr_act() (屏幕)
├── img_container (图片容器 - 背景层)
│   └── img_obj (图片对象)
└── container_ (UI主容器 - 前景层)
    ├── status_bar_ (状态栏)
    │   ├── network_label_ (WiFi图标)
    │   ├── notification_label_ (通知消息)
    │   ├── status_label_ (状态消息)
    │   ├── mute_label_ (静音图标)
    │   └── battery_label_ (电池图标) ← 最右边
    └── content_ (内容区)
        └── chat_message_label_ (聊天消息)
```

## 关键设计

### 1. 透明背景设计
所有UI容器都设置为完全透明，不遮挡背景图片：

```cpp
lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
```

### 2. 图片缩放和位置设置
为了更好的视觉效果，图片放大到110%显示，并向下偏移3像素：

```cpp
// 设置图片缩放为110% (LVGL缩放值: 256 = 100%, 282 = 110%)
lv_img_set_zoom(img_obj, 282);

// 居中对齐，并向下偏移3个像素
lv_obj_align(img_obj, LV_ALIGN_CENTER, 0, 3);  // X偏移=0, Y偏移=3
```

**缩放值说明：**
- LVGL的缩放值计算：`zoom_value = 256 * scale_percentage`
- 100% = 256
- 110% = 282 (256 × 1.1) ← 当前使用
- 120% = 307 (256 × 1.2)
- 150% = 384 (256 × 1.5)

**位置偏移说明：**
- `lv_obj_align(obj, align, x_offset, y_offset)`
- Y偏移为正值：向下移动
- Y偏移为负值：向上移动

### 3. 层级管理
- **图片容器**：不调用 `lv_obj_move_foreground()`，保持在背景层
- **UI容器**：调用 `lv_obj_move_foreground(container_)`，确保在图片之上

### 4. 状态栏布局

状态栏使用 Flex 横向布局 (`LV_FLEX_FLOW_ROW`)，元素从左到右排列：

| 位置 | 元素 | 说明 | Flex Grow |
|------|------|------|-----------|
| 左侧 | network_label_ | WiFi信号图标 | 0 |
| 中间 | notification_label_ | 通知消息 | 1 |
| 中间 | status_label_ | 状态消息 | 1 |
| 右侧 | mute_label_ | 静音图标 | 0 |
| **右侧** | **battery_label_** | **电池图标** | **0** |

**注意：** `notification_label_` 和 `status_label_` 的 `flex_grow=1` 使它们占据中间可用空间，将左右两侧的图标分别推到边缘。

### 5. 消息位置优化

将消息显示在屏幕底部，减少对背景图片的遮挡：

```cpp
// content_是垂直布局(LV_FLEX_FLOW_COLUMN)：主轴=垂直，交叉轴=水平
// 修改content_的flex对齐方式，将内容推到底部
lv_obj_set_flex_align(content_, 
    LV_FLEX_ALIGN_END,      // 参数1：主轴对齐(垂直) -> 推到底部
    LV_FLEX_ALIGN_CENTER,   // 参数2：交叉轴对齐(水平) -> 水平居中
    LV_FLEX_ALIGN_CENTER    // 参数3：内容对齐 -> 居中
);

// 添加底部内边距，适配圆形屏幕
lv_obj_set_style_pad_bottom(content_, 25, 0);  // 25像素底部边距
```

**效果：**
- ✅ 文字消息显示在**屏幕底部附近**（保留适当间距）
- ✅ 圆形屏幕底部边缘不会切割文字
- ✅ 图片的主要区域完全可见，不被文字遮挡
- ✅ 文字水平居中，清晰可读

### 6. 圆形屏幕适配

```cpp
// 调整状态栏padding以适配圆形屏幕，避免边缘被切割
lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.167, 0);   // 左边距 16.7%
lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.167, 0);  // 右边距 16.7%
```

## 显示效果

```
┌─────────────────────────────────┐
│ [WiFi]  [状态消息]  [静音][电池] │  ← 状态栏（透明背景，顶部）
├─────────────────────────────────┤
│                                 │
│                                 │
│        [图片显示区域]            │  ← 背景图片（中间大部分区域）
│                                 │
│                                 │
│                                 │
│      "你好，我是小智"            │  ← 文字消息（透明背景，底部附近）
│                                 │  ← 底部边距（25px）
└─────────────────────────────────┘
```

## 颜色设置

- **文字颜色**：白色 (`lv_color_white()`)
- **背景颜色**：完全透明 (`LV_OPA_TRANSP`)
- **图标字体**：`font_awesome_20_4`
- **文本字体**：`font_puhui_20_4`

## 动态更新

电池图标会根据电量自动更新，显示不同的图标：
- 充电中
- 电量高/中/低
- 电量百分比

文字消息会自动换行，居中显示，不遮挡背景图片。
