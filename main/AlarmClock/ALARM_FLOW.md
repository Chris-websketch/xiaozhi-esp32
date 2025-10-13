# 闹钟系统运行逻辑流程图

## 1. 系统初始化流程

```mermaid
flowchart TD
    A[系统启动] --> B[AlarmManager 构造函数]
    B --> C[从NVS加载所有闹钟]
    C --> D{遍历闹钟列表}
    D --> E{是重复闹钟?}
    E -->|是| F{time <= now?}
    E -->|否| D
    F -->|是| G[错过的闹钟!]
    F -->|否| D
    G --> H[计算下次触发时间]
    H --> I[更新闹钟时间]
    I --> J[保存到NVS]
    J --> K[记录日志: Missed alarm]
    K --> D
    D --> L[清除过期的ONCE类型闹钟]
    L --> M[获取最近的闹钟]
    M --> N{有闹钟?}
    N -->|是| O[启动ESP32定时器]
    N -->|否| P[初始化完成]
    O --> P
```

## 2. 设置闹钟流程

```mermaid
flowchart TD
    A[收到SetAlarm请求] --> B[参数验证]
    B --> C{参数有效?}
    C -->|否| D[记录错误日志]
    C -->|是| E{同名闹钟存在?}
    D --> Z[结束]
    E -->|是| F[更新现有闹钟]
    E -->|否| G{闹钟数量 < 10?}
    G -->|否| H[错误: 已达上限]
    G -->|是| I[查找空闲槽位]
    H --> Z
    I --> J[创建新闹钟对象]
    J --> K[设置闹钟属性]
    K --> L[保存到NVS]
    F --> L
    L --> M[更新name_to_slot映射]
    M --> N[记录日志: Created/Updated]
    N --> O[停止当前定时器]
    O --> P[重新计算最近闹钟]
    P --> Q[启动新定时器]
    Q --> Z
```

## 3. 闹钟触发流程

```mermaid
flowchart TD
    A[定时器触发] --> B[OnAlarm 回调]
    B --> C[获取当前时间 now]
    C --> D{遍历闹钟列表}
    D --> E{enabled && time <= now?}
    E -->|否| D
    E -->|是| F[设置ring_flag]
    F --> G[显示闹钟名称]
    G --> H[播放闹钟音效]
    H --> I{闹钟类型?}
    I -->|ONCE| J[标记待删除]
    I -->|DAILY/WEEKLY<br/>WORKDAYS/WEEKENDS| K[计算下次触发时间]
    K --> L[更新闹钟时间]
    L --> M[保存到NVS]
    M --> N[记录日志: Rescheduled]
    N --> O[清除过期ONCE闹钟]
    J --> O
    O --> P[重新计算最近闹钟]
    P --> Q[启动新定时器]
    Q --> R[结束]
```

## 4. 取消闹钟流程

```mermaid
flowchart TD
    A[收到CancelAlarm请求] --> B{闹钟存在?}
    B -->|否| C[记录警告日志]
    B -->|是| D[从内存列表删除]
    C --> Z[结束]
    D --> E[从NVS删除]
    E --> F[从映射表删除]
    F --> G[记录日志: Removed]
    G --> H[停止当前定时器]
    H --> I[重新计算最近闹钟]
    I --> J{有其他闹钟?}
    J -->|是| K[启动新定时器]
    J -->|否| L[无闹钟运行]
    K --> Z
    L --> Z
```

## 5. 启用/禁用闹钟流程

```mermaid
flowchart TD
    A[收到EnableAlarm请求] --> B{闹钟存在?}
    B -->|否| C[记录警告日志]
    B -->|是| D[更新enabled状态]
    C --> Z[结束]
    D --> E[保存到NVS]
    E --> F[记录日志: Enabled/Disabled]
    F --> G[停止当前定时器]
    G --> H[重新计算最近闹钟]
    H --> I{有启用的闹钟?}
    I -->|是| J[启动新定时器]
    I -->|否| K[无闹钟运行]
    J --> Z
    K --> Z
```

## 6. 下次触发时间计算流程

```mermaid
flowchart TD
    A[CalculateNextTriggerTime] --> B{闹钟类型?}
    B -->|ONCE| C[返回当前时间<br/>不会被调用]
    B -->|DAILY| D[获取今天的时:分]
    D --> E{时间已过?}
    E -->|是| F[+1天]
    E -->|否| G[使用今天]
    F --> H[返回新时间]
    G --> H
    
    B -->|WEEKLY| I[获取今天时:分]
    I --> J[从今天开始遍历]
    J --> K{匹配repeat_days?}
    K -->|是| L[找到下次时间]
    K -->|否| M[+1天继续]
    M --> N{超过7天?}
    N -->|否| K
    N -->|是| O[错误:无有效日期]
    L --> H
    O --> H
    
    B -->|WORKDAYS| P[获取今天时:分]
    P --> Q{是工作日且<br/>时间未过?}
    Q -->|是| R[使用今天]
    Q -->|否| S[找下一工作日]
    S --> R
    R --> H
    
    B -->|WEEKENDS| T[获取今天时:分]
    T --> U{是周末且<br/>时间未过?}
    U -->|是| V[使用今天]
    U -->|否| W[找下一周末]
    W --> V
    V --> H
```

## 7. 定时器管理流程

```mermaid
flowchart TD
    A[RestartTimerForNextAlarm] --> B{定时器运行中?}
    B -->|是| C[停止定时器]
    B -->|否| D[获取最近闹钟]
    C --> D
    D --> E[GetProximateAlarmUnlocked]
    E --> F{找到闹钟?}
    F -->|否| G[记录日志: No alarms]
    F -->|是| H[计算剩余秒数]
    G --> Z[结束]
    H --> I{剩余秒数 > 0?}
    I -->|否| J[警告: 时间已过]
    I -->|是| K[启动定时器]
    J --> Z
    K --> L[记录日志: Setting timer]
    L --> M[标记running_flag]
    M --> Z
```

## 8. 完整状态机

```mermaid
stateDiagram-v2
    [*] --> 初始化
    初始化 --> 加载NVS数据
    加载NVS数据 --> 修复错过闹钟
    修复错过闹钟 --> 待机
    
    待机 --> 设置闹钟: SetAlarm
    待机 --> 取消闹钟: CancelAlarm
    待机 --> 启用禁用: EnableAlarm
    待机 --> 闹钟触发: 定时器触发
    
    设置闹钟 --> 更新定时器
    取消闹钟 --> 更新定时器
    启用禁用 --> 更新定时器
    更新定时器 --> 待机
    
    闹钟触发 --> 播放音效
    播放音效 --> 判断类型
    判断类型 --> 删除闹钟: ONCE
    判断类型 --> 重新调度: REPEAT
    删除闹钟 --> 更新定时器
    重新调度 --> 更新定时器
    
    待机 --> 系统关机: 断电
    系统关机 --> 初始化: 重启
```

## 9. 数据流图

```mermaid
flowchart LR
    A[MQTT/IoT命令] --> B[AlarmIot]
    B --> C[AlarmManager]
    C --> D[闹钟列表alarms_]
    C --> E[映射表name_to_slot_]
    C --> F[ESP32定时器]
    C --> G[Settings/NVS]
    
    D --> H[GetProximateAlarm]
    H --> F
    F --> I[OnAlarm回调]
    I --> C
    
    G --> J[持久化存储]
    J --> K[重启恢复]
    K --> C
    
    C --> L[Display显示]
    C --> M[Audio播放]
```

## 10. 关键时间节点

```mermaid
gantt
    title 每日闹钟生命周期示例
    dateFormat HH:mm
    axisFormat %H:%M
    
    section 正常流程
    创建闹钟(08:00)      :done, a1, 07:00, 1h
    等待触发             :active, a2, 08:00, 0h
    触发&播放音效        :crit, a3, 08:00, 1m
    重新调度(明天08:00)  :done, a4, 08:01, 1m
    
    section 错过闹钟
    设备关机             :crit, b1, 07:00, 3h
    错过触发时间         :b2, 08:00, 0h
    10:00开机            :milestone, b3, 10:00, 0h
    检测并修复           :done, b4, 10:00, 1m
    调度到明天08:00      :done, b5, 10:01, 1m
```

## 关键概念说明

### 互斥锁保护
所有公开API都使用 `std::lock_guard<std::mutex>` 保护，确保线程安全。

### 递归锁避免
内部方法使用 `GetProximateAlarmUnlocked` 避免死锁问题。

### 持久化策略
- 每次修改闹钟立即保存到NVS
- 重启时自动加载并修复错过的闹钟

### 定时器精度
- 使用ESP32硬件定时器
- 理论精度：微秒级
- 实际精度：±100ms（考虑任务调度延迟）
