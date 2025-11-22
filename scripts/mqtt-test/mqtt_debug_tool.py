#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MQTT调试工具 - 基于PySide6和paho-mqtt
支持多主题订阅、消息历史记录、连接配置管理
"""

import sys
import os
import random
import json
import time
from datetime import datetime
from typing import Optional, Dict, List

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLineEdit, QPushButton, QTextEdit, QLabel,
    QTableWidget, QTableWidgetItem, QComboBox, QCheckBox,
    QSplitter, QHeaderView, QMessageBox, QSpinBox, QGridLayout,
    QScrollArea, QFrame, QColorDialog
)
from PySide6.QtCore import Qt, Signal, QObject
from PySide6.QtGui import QFont, QColor, QTextCursor

from paho.mqtt import client as mqtt_client


# 默认配置 - 项目专用MQTT服务器
DEFAULT_BROKER = '110.42.35.132'
DEFAULT_PORT = 1883
DEFAULT_USERNAME = 'xiaoqiao'
DEFAULT_PASSWORD = 'dzkj0000'
DEVICE_CLIENT_ID = '719ae1ad-9f2c-4277-9c99-1a317a478979'  # ESP32设备ID
DEBUG_CLIENT_ID = 'mqtt-debug-tool-' + ''.join(['{:02x}'.format(random.randint(0, 255)) for _ in range(4)])  # 调试工具专用ID
CA_CERT_FILE = 'emqx_ca.crt'


# 全局QSS样式
APP_STYLESHEET = """
/* 主窗口样式 */
QMainWindow {
    background-color: #f5f5f5;
}

/* 按钮通用样式 */
QPushButton {
    border-radius: 4px;
    padding: 6px 12px;
    font-size: 9pt;
    border: 1px solid #ccc;
    background-color: #ffffff;
}

QPushButton:hover {
    background-color: #e8f4f8;
    border-color: #0078d4;
}

QPushButton:pressed {
    background-color: #c7e0f4;
}

QPushButton:disabled {
    background-color: #f0f0f0;
    color: #999;
}

/* IoT模板按钮样式 */
.template-btn {
    border: none;
    border-radius: 4px;
    padding: 8px 12px;
    font-size: 9pt;
    text-align: left;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #ffffff, stop:1 #f0f0f0);
}

.template-btn:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                stop:0 #e3f2fd, stop:1 #bbdefb);
    border: 1px solid #2196f3;
}

.template-btn:pressed {
    background: #90caf9;
}

/* 屏幕控制按钮 */
.template-btn-screen {
    color: #1976d2;
    border-left: 3px solid #2196f3;
}

/* 音频控制按钮 */
.template-btn-audio {
    color: #7b1fa2;
    border-left: 3px solid #9c27b0;
}

/* 闹钟控制按钮 */
.template-btn-alarm {
    color: #f57c00;
    border-left: 3px solid #ff9800;
}

/* 图片显示按钮 */
.template-btn-image {
    color: #388e3c;
    border-left: 3px solid #4caf50;
}

/* 音乐播放器按钮 */
.template-btn-music {
    color: #c62828;
    border-left: 3px solid #f44336;
}

/* 字幕控制按钮 */
.template-btn-subtitle {
    color: #00796b;
    border-left: 3px solid #009688;
}

/* 系统控制按钮 */
.template-btn-system {
    color: #d32f2f;
    border-left: 3px solid #f44336;
}

/* 通知按钮 */
.template-btn-notify {
    color: #0288d1;
    border-left: 3px solid #03a9f4;
}

/* GroupBox样式 */
QGroupBox {
    font-weight: bold;
    border: 2px solid #ddd;
    border-radius: 6px;
    margin-top: 10px;
    padding-top: 10px;
    background-color: #ffffff;
}

QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
}

/* 连接按钮特殊样式 */
#connect_btn {
    background-color: #4caf50;
    color: white;
    font-weight: bold;
    border: none;
}

#connect_btn:hover {
    background-color: #66bb6a;
}

#disconnect_btn {
    background-color: #f44336;
    color: white;
    font-weight: bold;
    border: none;
}

#disconnect_btn:hover {
    background-color: #ef5350;
}

/* 发送按钮样式 */
#publish_btn {
    background-color: #2196f3;
    color: white;
    font-weight: bold;
    padding: 8px 20px;
    border: none;
}

#publish_btn:hover {
    background-color: #42a5f5;
}

/* 输入框样式 */
QLineEdit, QTextEdit {
    border: 1px solid #ccc;
    border-radius: 4px;
    padding: 5px;
    background-color: #ffffff;
}

QLineEdit:focus, QTextEdit:focus {
    border-color: #2196f3;
}

/* 表格样式 */
QTableWidget {
    border: 1px solid #ddd;
    gridline-color: #e0e0e0;
    background-color: #ffffff;
}

QTableWidget::item:selected {
    background-color: #bbdefb;
}

QHeaderView::section {
    background-color: #f5f5f5;
    padding: 5px;
    border: 1px solid #ddd;
    font-weight: bold;
}

/* 滚动区域样式 */
QScrollArea {
    border: none;
}
"""

# 消息模板库
MESSAGE_TEMPLATES = {
    "IoT控制": {
        "屏幕 - 设置亮度": {
            "type": "iot",
            "commands": [
                {"name": "Screen", "method": "SetBrightness", "parameters": {"brightness": 80}}
            ]
        },
        "屏幕 - 设置主题(dark)": {
            "type": "iot",
            "commands": [
                {"name": "Screen", "method": "SetTheme", "parameters": {"theme_name": "dark"}}
            ]
        },
        "屏幕 - 设置主题(light)": {
            "type": "iot",
            "commands": [
                {"name": "Screen", "method": "SetTheme", "parameters": {"theme_name": "light"}}
            ]
        },
        "扬声器 - 设置音量": {
            "type": "iot",
            "commands": [
                {"name": "Speaker", "method": "SetVolume", "parameters": {"volume": 80}}
            ]
        },
        "闹钟 - 一次性闹钟(60秒后)": {
            "type": "iot",
            "commands": [
                {"name": "Alarm", "method": "SetAlarm", "parameters": {"second_from_now": 60, "alarm_name": "测试闹钟"}}
            ]
        },
        "闹钟 - 每天重复闹钟": {
            "type": "iot",
            "commands": [
                {"name": "Alarm", "method": "SetAlarm", "parameters": {"second_from_now": 120, "alarm_name": "每日提醒", "repeat_type": 1}}
            ]
        },
        "闹钟 - 工作日闹钟": {
            "type": "iot",
            "commands": [
                {"name": "Alarm", "method": "SetAlarm", "parameters": {"second_from_now": 300, "alarm_name": "起床闹钟", "repeat_type": 3}}
            ]
        },
        "闹钟 - 取消闹钟": {
            "type": "iot",
            "commands": [
                {"name": "Alarm", "method": "CancelAlarm", "parameters": {"alarm_name": "测试闹钟"}}
            ]
        },
        "图片显示 - 动态模式": {
            "type": "iot",
            "commands": [
                {"name": "ImageDisplay", "method": "SetAnimatedMode", "parameters": {}}
            ]
        },
        "图片显示 - 静态模式": {
            "type": "iot",
            "commands": [
                {"name": "ImageDisplay", "method": "SetStaticMode", "parameters": {}}
            ]
        },
        "图片显示 - 表情包模式": {
            "type": "iot",
            "commands": [
                {"name": "ImageDisplay", "method": "SetEmoticonMode", "parameters": {}}
            ]
        },
        "图片显示 - 切换显示模式": {
            "type": "iot",
            "commands": [
                {"name": "ImageDisplay", "method": "ToggleDisplayMode", "parameters": {}}
            ]
        },
        "音乐播放器 - 显示": {
            "type": "iot",
            "commands": [
                {"name": "MusicPlayer", "method": "Show", "parameters": {"duration_ms": 30000, "song_title": "夜曲", "artist_name": "周杰伦"}}
            ]
        },
        "音乐播放器 - 隐藏": {
            "type": "iot",
            "commands": [
                {"name": "MusicPlayer", "method": "Hide", "parameters": {}}
            ]
        },
        "字幕控制 - 显示字幕": {
            "type": "iot",
            "commands": [
                {"name": "SubtitleControl", "method": "ShowSubtitle", "parameters": {}}
            ]
        },
        "字幕控制 - 隐藏字幕": {
            "type": "iot",
            "commands": [
                {"name": "SubtitleControl", "method": "HideSubtitle", "parameters": {}}
            ]
        },
        "字幕控制 - 切换显示状态": {
            "type": "iot",
            "commands": [
                {"name": "SubtitleControl", "method": "ToggleSubtitle", "parameters": {}}
            ]
        }
    },
    "系统控制": {
        "设备重启(1秒延迟)": {
            "type": "system",
            "action": "reboot",
            "delay_ms": 1000
        },
        "设备重启(5秒延迟)": {
            "type": "system",
            "action": "reboot",
            "delay_ms": 5000
        }
    },
    "通知消息": {
        "简单通知": {
            "type": "notify",
            "title": "通知标题",
            "body": "通知内容"
        },
        "仅标题": {
            "type": "notify",
            "title": "这是一个通知"
        },
        "仅内容": {
            "type": "notify",
            "body": "这是通知的详细内容"
        }
    },
    "广播测试": {
        "广播通知 - 系统维护": {
            "type": "notify",
            "title": "系统维护通知",
            "body": "服务器将于今晚22:00进行维护，预计持续30分钟"
        },
        "广播通知 - 固件更新": {
            "type": "notify",
            "title": "固件更新提醒",
            "body": "新版本固件已发布，请及时更新"
        },
        "广播IoT - 统一调整亮度": {
            "type": "iot",
            "commands": [
                {"name": "Screen", "method": "SetBrightness", "parameters": {"brightness": 50}}
            ]
        },
        "广播IoT - 统一设置音量": {
            "type": "iot",
            "commands": [
                {"name": "Speaker", "method": "SetVolume", "parameters": {"volume": 60}}
            ]
        }
    }
}


class MQTTSignals(QObject):
    """MQTT信号，用于线程间通信"""
    connected = Signal(bool, str)  # (success, message)
    disconnected = Signal(str)  # message
    message_received = Signal(str, str, str)  # (timestamp, topic, payload)
    published = Signal(bool, str)  # (success, message)
    device_status_changed = Signal(bool, str, str)  # (online, reason, timestamp)


class MQTTClientWrapper:
    """MQTT客户端封装类"""
    
    def __init__(self):
        self.client: Optional[mqtt_client.Client] = None
        self.signals = MQTTSignals()
        self.is_connected = False
        self.broker = DEFAULT_BROKER
        self.port = DEFAULT_PORT
        self.username = DEFAULT_USERNAME
        self.password = DEFAULT_PASSWORD
        self.client_id = DEBUG_CLIENT_ID
        self.use_ssl = False
        self.ca_cert_path = ''
        
    def connect(self, broker: str, port: int, username: str, password: str, 
                use_ssl: bool = False, ca_cert: str = ''):
        """连接到MQTT Broker"""
        try:
            self.broker = broker
            self.port = port
            self.username = username
            self.password = password
            self.use_ssl = use_ssl
            self.ca_cert_path = ca_cert
            
            # 创建客户端（paho-mqtt 2.0+ API）
            self.client = mqtt_client.Client(
                client_id=self.client_id,
                callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2
            )
            
            # 设置用户名密码
            if username and password:
                self.client.username_pw_set(username, password)
            
            # 设置SSL
            if use_ssl:
                if ca_cert:
                    self.client.tls_set(ca_certs=ca_cert)
                else:
                    self.client.tls_set()  # 使用系统默认CA
            
            # 设置回调
            self.client.on_connect = self._on_connect
            self.client.on_message = self._on_message
            self.client.on_disconnect = self._on_disconnect
            
            # 连接
            self.client.connect(broker, port, keepalive=60)
            self.client.loop_start()
            
        except Exception as e:
            self.signals.connected.emit(False, f"连接失败: {str(e)}")
    
    def disconnect(self):
        """断开连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.is_connected = False
    
    def subscribe(self, topic: str, qos: int = 0):
        """订阅主题"""
        if self.client and self.is_connected:
            self.client.subscribe(topic, qos)
            return True
        return False
    
    def unsubscribe(self, topic: str):
        """取消订阅"""
        if self.client and self.is_connected:
            self.client.unsubscribe(topic)
            return True
        return False
    
    def publish(self, topic: str, payload: str, qos: int = 0):
        """发布消息"""
        if self.client and self.is_connected:
            try:
                result = self.client.publish(topic, payload, qos)
                if result.rc == mqtt_client.MQTT_ERR_SUCCESS:
                    self.signals.published.emit(True, f"消息已发送到 {topic}")
                else:
                    self.signals.published.emit(False, f"发送失败: {result.rc}")
            except Exception as e:
                self.signals.published.emit(False, f"发送异常: {str(e)}")
        else:
            self.signals.published.emit(False, "未连接到Broker")
    
    def _on_connect(self, client, userdata, flags, reason_code, properties):
        """连接回调（API v2）"""
        if reason_code == 0:
            self.is_connected = True
            self.signals.connected.emit(True, "连接成功")
        else:
            self.is_connected = False
            error_msg = f"连接失败，返回码: {reason_code}"
            self.signals.connected.emit(False, error_msg)
    
    def _on_message(self, client, userdata, msg):
        """消息接收回调"""
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        topic = msg.topic
        try:
            payload = msg.payload.decode('utf-8')
        except:
            payload = str(msg.payload)
        
        self.signals.message_received.emit(timestamp, topic, payload)
    
    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        """断开连接回调（API v2）"""
        self.is_connected = False
        if reason_code != 0:
            self.signals.disconnected.emit(f"意外断开连接，返回码: {reason_code}")
        else:
            self.signals.disconnected.emit("已断开连接")


class MainWindow(QMainWindow):
    """主窗口"""
    
    def __init__(self):
        super().__init__()
        self.mqtt_client = MQTTClientWrapper()
        self.subscribed_topics = {}  # {topic: qos}
        self.topic_colors = {}  # {topic: color_hex}  主题颜色映射
        self.device_id = DEVICE_CLIENT_ID  # 设备ID用于主题拼接
        self.device_online = False  # 设备在线状态
        self.online_count = 0  # 上线次数
        self.offline_count = 0  # 离线次数
        self.last_online_time = None  # 最后上线时间
        self.last_offline_time = None  # 最后离线时间
        # 预设主题颜色
        self.preset_colors = ['#2196F3', '#4CAF50', '#FF9800', '#9C27B0', '#F44336', '#00BCD4', '#FFEB3B', '#E91E63']
        self.color_index = 0  # 用于自动分配颜色
        self.init_ui()
        self.connect_signals()
        
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle('MQTT调试工具 - IoT命令控制台')
        self.setGeometry(100, 100, 1400, 900)
        
        # 应用样式表
        self.setStyleSheet(APP_STYLESHEET)
        
        # 主部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setSpacing(8)
        main_layout.setContentsMargins(8, 8, 8, 8)
        
        # 连接配置区（固定大小，不随窗口缩放）
        conn_group = self.create_connection_group()
        conn_group.setMaximumHeight(120)  # 设置最大高度
        conn_group.setMinimumHeight(120)  # 设置最小高度，实现固定大小
        main_layout.addWidget(conn_group, 0)  # stretch=0 固定大小
        
        # 主分割器 - 左右分割
        main_splitter = QSplitter(Qt.Horizontal)
        
        # 左侧区域 - 垂直分割（订阅管理 + 消息历史）
        left_splitter = QSplitter(Qt.Vertical)
        
        # 订阅管理区
        sub_group = self.create_subscription_group()
        left_splitter.addWidget(sub_group)
        
        # 消息历史区
        msg_group = self.create_message_history_group()
        left_splitter.addWidget(msg_group)
        
        left_splitter.setSizes([400, 400])
        main_splitter.addWidget(left_splitter)
        
        # 右侧区域 - 垂直分割（IoT命令面板 + 发布区）
        right_splitter = QSplitter(Qt.Vertical)
        
        # IoT命令快捷按钮面板
        template_panel = self.create_template_buttons_panel()
        right_splitter.addWidget(template_panel)
        
        # 发布区
        pub_group = self.create_publish_group()
        right_splitter.addWidget(pub_group)
        
        right_splitter.setSizes([550, 250])
        main_splitter.addWidget(right_splitter)
        
        # 左右等宽分割
        main_splitter.setSizes([700, 700])
        main_layout.addWidget(main_splitter, 1)  # stretch=1 响应式填充剩余空间
        
    def create_connection_group(self):
        """创建连接配置组"""
        group = QGroupBox("连接配置")
        main_layout = QVBoxLayout()
        
        # 第一行：Broker、Port、SSL
        row1 = QHBoxLayout()
        row1.addWidget(QLabel("Broker:"))
        self.broker_input = QLineEdit(DEFAULT_BROKER)
        self.broker_input.setMaximumWidth(280)
        row1.addWidget(self.broker_input)
        
        row1.addWidget(QLabel("Port:"))
        self.port_input = QSpinBox()
        self.port_input.setRange(1, 65535)
        self.port_input.setValue(DEFAULT_PORT)
        self.port_input.setMaximumWidth(80)
        row1.addWidget(self.port_input)
        
        self.ssl_checkbox = QCheckBox("SSL/TLS")
        self.ssl_checkbox.setChecked(False)  # 默认关闭SSL
        row1.addWidget(self.ssl_checkbox)
        
        self.auto_ack_checkbox = QCheckBox("自动回复ACK")
        self.auto_ack_checkbox.setChecked(True)  # 默认启用
        self.auto_ack_checkbox.setToolTip("设备发送ACK后自动回复ack_receipt确认")
        row1.addWidget(self.auto_ack_checkbox)
        
        row1.addStretch()
        main_layout.addLayout(row1)
        
        # 第二行：Username、Password、ClientID、连接按钮
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("Username:"))
        self.username_input = QLineEdit(DEFAULT_USERNAME)
        self.username_input.setMaximumWidth(100)
        row2.addWidget(self.username_input)
        
        row2.addWidget(QLabel("Password:"))
        self.password_input = QLineEdit(DEFAULT_PASSWORD)
        self.password_input.setEchoMode(QLineEdit.Password)
        self.password_input.setMaximumWidth(100)
        row2.addWidget(self.password_input)
        
        row2.addWidget(QLabel("设备ID:"))
        self.device_id_label = QLabel(DEVICE_CLIENT_ID)
        self.device_id_label.setStyleSheet("color: #0066cc; font-family: 'Consolas'; font-size: 9pt;")
        self.device_id_label.setToolTip(f"ESP32设备ID\n调试工具ID: {DEBUG_CLIENT_ID}")
        row2.addWidget(self.device_id_label)
        
        row2.addStretch()
        
        # 连接按钮
        self.connect_btn = QPushButton("连接")
        self.connect_btn.setObjectName("connect_btn")
        self.connect_btn.setMaximumWidth(100)
        self.connect_btn.clicked.connect(self.toggle_connection)
        row2.addWidget(self.connect_btn)
        
        # 状态指示
        self.status_label = QLabel("未连接")
        self.status_label.setStyleSheet("color: gray; font-weight: bold;")
        row2.addWidget(self.status_label)
        
        main_layout.addLayout(row2)
        group.setLayout(main_layout)
        return group
    
    def create_device_status_panel(self):
        """创建设备在线状态监控面板"""
        panel = QGroupBox("设备在线状态 (LWT)")
        layout = QVBoxLayout()
        
        # 状态指示
        status_layout = QHBoxLayout()
        status_layout.addWidget(QLabel("状态:"))
        self.device_status_label = QLabel("未知")
        self.device_status_label.setStyleSheet("color: gray; font-weight: bold; font-size: 11pt;")
        status_layout.addWidget(self.device_status_label)
        status_layout.addStretch()
        layout.addLayout(status_layout)
        
        # 统计信息
        stats_layout = QHBoxLayout()
        stats_layout.addWidget(QLabel("上线次数:"))
        self.online_count_label = QLabel("0")
        self.online_count_label.setStyleSheet("color: green;")
        stats_layout.addWidget(self.online_count_label)
        
        stats_layout.addWidget(QLabel("离线次数:"))
        self.offline_count_label = QLabel("0")
        self.offline_count_label.setStyleSheet("color: red;")
        stats_layout.addWidget(self.offline_count_label)
        stats_layout.addStretch()
        layout.addLayout(stats_layout)
        
        # 时间信息
        time_layout = QVBoxLayout()
        self.last_online_label = QLabel("最后上线: --")
        self.last_online_label.setStyleSheet("font-size: 8pt; color: #666;")
        time_layout.addWidget(self.last_online_label)
        
        self.last_offline_label = QLabel("最后离线: --")
        self.last_offline_label.setStyleSheet("font-size: 8pt; color: #666;")
        time_layout.addWidget(self.last_offline_label)
        layout.addLayout(time_layout)
        
        panel.setLayout(layout)
        panel.setMaximumHeight(150)
        return panel
    
    def create_subscription_group(self):
        """创建订阅管理组"""
        group = QGroupBox("订阅管理")
        layout = QVBoxLayout()
        
        # 快捷主题按钮
        quick_layout = QHBoxLayout()
        quick_layout.addWidget(QLabel("快捷:"))
        
        downlink_btn = QPushButton("Downlink")
        downlink_btn.setMaximumWidth(80)
        downlink_btn.clicked.connect(lambda: self.fill_topic("downlink", True))
        quick_layout.addWidget(downlink_btn)
        
        uplink_btn = QPushButton("Uplink")
        uplink_btn.setMaximumWidth(80)
        uplink_btn.clicked.connect(lambda: self.fill_topic("uplink", True))
        quick_layout.addWidget(uplink_btn)
        
        ack_btn = QPushButton("ACK")
        ack_btn.setMaximumWidth(80)
        ack_btn.clicked.connect(lambda: self.fill_topic("ack", True))
        quick_layout.addWidget(ack_btn)
        
        status_btn = QPushButton("Status")
        status_btn.setMaximumWidth(80)
        status_btn.setToolTip("订阅设备在线状态主题（LWT机制）")
        status_btn.clicked.connect(lambda: self.fill_topic("status", True))
        quick_layout.addWidget(status_btn)
        
        broadcast_btn = QPushButton("Broadcast")
        broadcast_btn.setMaximumWidth(80)
        broadcast_btn.setToolTip("订阅全局广播主题（所有设备共享）")
        broadcast_btn.clicked.connect(lambda: self.fill_topic("broadcast", True))
        quick_layout.addWidget(broadcast_btn)
        
        quick_layout.addStretch()
        layout.addLayout(quick_layout)
        
        # 设备在线状态监控面板
        status_panel = self.create_device_status_panel()
        layout.addWidget(status_panel)
        
        # 添加订阅控件
        add_layout = QHBoxLayout()
        add_layout.addWidget(QLabel("主题:"))
        self.sub_topic_input = QLineEdit()
        self.sub_topic_input.setPlaceholderText(f"例: devices/{DEVICE_CLIENT_ID}/downlink")
        add_layout.addWidget(self.sub_topic_input)
        
        add_layout.addWidget(QLabel("QoS:"))
        self.sub_qos_combo = QComboBox()
        self.sub_qos_combo.addItems(['0', '1', '2'])
        self.sub_qos_combo.setCurrentText('2')  # 默认QoS 2
        self.sub_qos_combo.setMaximumWidth(60)
        add_layout.addWidget(self.sub_qos_combo)
        
        self.add_sub_btn = QPushButton("添加")
        self.add_sub_btn.clicked.connect(self.add_subscription)
        add_layout.addWidget(self.add_sub_btn)
        
        layout.addLayout(add_layout)
        
        # 订阅列表
        self.sub_table = QTableWidget()
        self.sub_table.setColumnCount(4)
        self.sub_table.setHorizontalHeaderLabels(['主题', 'QoS', '颜色', '操作'])
        self.sub_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.sub_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.sub_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.sub_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)
        self.sub_table.setSelectionBehavior(QTableWidget.SelectRows)
        layout.addWidget(self.sub_table)
        
        group.setLayout(layout)
        return group
    
    def create_template_buttons_panel(self):
        """创建IoT模板按钮面板"""
        group = QGroupBox("⚡ IoT命令快捷面板")
        main_layout = QVBoxLayout()
        
        # 创建滚动区域
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        
        scroll_widget = QWidget()
        scroll_layout = QVBoxLayout(scroll_widget)
        scroll_layout.setSpacing(10)
        
        # 按类别组织按钮
        categories = [
            ("屏幕控制", "screen", ["屏幕 - 设置亮度", "屏幕 - 设置主题(dark)", "屏幕 - 设置主题(light)"]),
            ("音频控制", "audio", ["扬声器 - 设置音量"]),
            ("闹钟管理", "alarm", ["闹钟 - 一次性闹钟(60秒后)", "闹钟 - 每天重复闹钟", "闹钟 - 工作日闹钟", "闹钟 - 取消闹钟"]),
            ("图片显示", "image", ["图片显示 - 动态模式", "图片显示 - 静态模式", "图片显示 - 表情包模式", "图片显示 - 切换显示模式"]),
            ("音乐播放器", "music", ["音乐播放器 - 显示", "音乐播放器 - 隐藏"]),
            ("字幕控制", "subtitle", ["字幕控制 - 显示字幕", "字幕控制 - 隐藏字幕", "字幕控制 - 切换显示状态"]),
            ("系统控制", "system", ["设备重启(1秒延迟)", "设备重启(5秒延迟)"]),
            ("通知消息", "notify", ["简单通知", "仅标题", "仅内容"]),
            ("广播测试", "broadcast", ["广播通知 - 系统维护", "广播通知 - 固件更新", "广播IoT - 统一调整亮度", "广播IoT - 统一设置音量"])
        ]
        
        for category_name, category_type, templates in categories:
            # 分类标题
            category_label = QLabel(f"━━ {category_name} ━━")
            category_label.setStyleSheet("font-weight: bold; color: #666; font-size: 9pt; padding: 5px 0;")
            scroll_layout.addWidget(category_label)
            
            # 按钮网格
            grid = QGridLayout()
            grid.setSpacing(6)
            
            for idx, template_name in enumerate(templates):
                btn = QPushButton(template_name)
                btn.setProperty("class", "template-btn")
                btn.setProperty("category", category_type)
                btn.setStyleSheet(f"""QPushButton {{
                    border: none;
                    border-radius: 4px;
                    padding: 8px 12px;
                    font-size: 9pt;
                    text-align: left;
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                                stop:0 #ffffff, stop:1 #f8f8f8);
                    border-left: 3px solid {self._get_category_color(category_type)};
                }}
                QPushButton:hover {{
                    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                                stop:0 #e3f2fd, stop:1 #bbdefb);
                    border-left: 3px solid {self._get_category_color(category_type)};
                }}
                QPushButton:pressed {{
                    background: #90caf9;
                }}""")
                btn.setCursor(Qt.PointingHandCursor)
                btn.setMinimumHeight(35)
                
                # 设置工具提示
                template_data = self._get_template_data(template_name)
                if template_data:
                    tooltip = json.dumps(template_data, indent=2, ensure_ascii=False)
                    btn.setToolTip(f"点击填充模板\n\n{tooltip}")
                
                btn.clicked.connect(lambda checked=False, name=template_name: self.on_template_button_clicked(name))
                
                # 2列布局
                row = idx // 2
                col = idx % 2
                grid.addWidget(btn, row, col)
            
            scroll_layout.addLayout(grid)
        
        scroll_layout.addStretch()
        scroll.setWidget(scroll_widget)
        main_layout.addWidget(scroll)
        
        group.setLayout(main_layout)
        return group
    
    def _get_category_color(self, category: str) -> str:
        """获取分类颜色"""
        colors = {
            "screen": "#2196f3",
            "audio": "#9c27b0",
            "alarm": "#ff9800",
            "image": "#4caf50",
            "music": "#f44336",
            "subtitle": "#009688",
            "system": "#f44336",
            "notify": "#03a9f4",
            "broadcast": "#ff5722"
        }
        return colors.get(category, "#999")
    
    def _get_template_data(self, template_name: str):
        """获取模板数据"""
        for category in MESSAGE_TEMPLATES.values():
            if template_name in category:
                return category[template_name]
        return None
    
    def on_template_button_clicked(self, template_name: str):
        """模板按钮点击处理"""
        template_data = self._get_template_data(template_name)
        if template_data:
            # 根据模板类型填充主题
            if template_name.startswith("广播"):
                self.fill_topic("broadcast", False)
            else:
                self.fill_topic("downlink", False)
            
            # 填充消息内容
            json_str = json.dumps(template_data, indent=2, ensure_ascii=False)
            self.pub_message_input.setPlainText(json_str)
            
            # 记录日志
            self.append_log(f"[模板] 已加载: {template_name}")
    
    def create_publish_group(self):
        """创建发布组"""
        group = QGroupBox("📤 消息发布")
        layout = QVBoxLayout()
        
        # 主题和QoS
        top_layout = QHBoxLayout()
        
        # 快捷主题按钮
        downlink_btn = QPushButton("↓Downlink")
        downlink_btn.setMaximumWidth(90)
        downlink_btn.clicked.connect(lambda: self.fill_topic("downlink", False))
        top_layout.addWidget(downlink_btn)
        
        uplink_btn = QPushButton("↑Uplink")
        uplink_btn.setMaximumWidth(90)
        uplink_btn.clicked.connect(lambda: self.fill_topic("uplink", False))
        top_layout.addWidget(uplink_btn)
        
        broadcast_btn = QPushButton("📡Broadcast")
        broadcast_btn.setMaximumWidth(90)
        broadcast_btn.setToolTip("全局广播主题（所有设备）")
        broadcast_btn.clicked.connect(lambda: self.fill_topic("broadcast", False))
        top_layout.addWidget(broadcast_btn)
        
        top_layout.addWidget(QLabel("主题:"))
        self.pub_topic_input = QLineEdit()
        self.pub_topic_input.setPlaceholderText(f"例: devices/{DEVICE_CLIENT_ID}/downlink")
        top_layout.addWidget(self.pub_topic_input)
        
        top_layout.addWidget(QLabel("QoS:"))
        self.pub_qos_combo = QComboBox()
        self.pub_qos_combo.addItems(['0', '1', '2'])
        self.pub_qos_combo.setCurrentText('2')  # 默认QoS 2
        self.pub_qos_combo.setMaximumWidth(60)
        top_layout.addWidget(self.pub_qos_combo)
        
        layout.addLayout(top_layout)
        
        # 消息内容
        content_label = QLabel("消息内容:")
        content_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(content_label)
        self.pub_message_input = QTextEdit()
        self.pub_message_input.setPlaceholderText('从上方快捷按钮选择命令模板，或手动输入JSON消息')
        self.pub_message_input.setMinimumHeight(80)
        self.pub_message_input.setFont(QFont("Consolas", 9))
        layout.addWidget(self.pub_message_input)
        
        # 发送按钮
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        self.publish_btn = QPushButton("📨 发送消息")
        self.publish_btn.setObjectName("publish_btn")
        self.publish_btn.setMinimumWidth(120)
        self.publish_btn.setMinimumHeight(35)
        self.publish_btn.clicked.connect(self.publish_message)
        btn_layout.addWidget(self.publish_btn)
        layout.addLayout(btn_layout)
        
        group.setLayout(layout)
        return group
    
    def create_message_history_group(self):
        """创建消息历史组"""
        group = QGroupBox("消息历史")
        layout = QVBoxLayout()
        
        # 工具栏
        toolbar = QHBoxLayout()
        clear_btn = QPushButton("清空")
        clear_btn.setMaximumWidth(80)
        clear_btn.clicked.connect(lambda: self.msg_history.clear())
        toolbar.addStretch()
        toolbar.addWidget(clear_btn)
        layout.addLayout(toolbar)
        
        # 消息显示区
        self.msg_history = QTextEdit()
        self.msg_history.setReadOnly(True)
        self.msg_history.setFont(QFont("Consolas", 9))
        layout.addWidget(self.msg_history)
        
        group.setLayout(layout)
        return group
    
    def connect_signals(self):
        """连接信号"""
        self.mqtt_client.signals.connected.connect(self.on_connected)
        self.mqtt_client.signals.disconnected.connect(self.on_disconnected)
        self.mqtt_client.signals.message_received.connect(self.on_message_received)
        self.mqtt_client.signals.published.connect(self.on_published)
        self.mqtt_client.signals.device_status_changed.connect(self.on_device_status_changed)
    
    def toggle_connection(self):
        """切换连接状态"""
        if not self.mqtt_client.is_connected:
            # 连接
            broker = self.broker_input.text().strip()
            port = self.port_input.value()
            username = self.username_input.text().strip()
            password = self.password_input.text().strip()
            use_ssl = self.ssl_checkbox.isChecked()
            
            if not broker:
                QMessageBox.warning(self, "错误", "请输入Broker地址")
                return
            
            self.connect_btn.setEnabled(False)
            self.status_label.setText("连接中...")
            self.status_label.setStyleSheet("color: orange; font-weight: bold;")
            
            # 获取CA证书路径
            ca_cert = ''
            if use_ssl:
                ca_cert_path = os.path.join(os.path.dirname(__file__), CA_CERT_FILE)
                if os.path.exists(ca_cert_path):
                    ca_cert = ca_cert_path
                else:
                    self.append_log(f"[警告] CA证书文件未找到: {ca_cert_path}")
            
            self.mqtt_client.connect(broker, port, username, password, use_ssl, ca_cert)
        else:
            # 断开
            self.mqtt_client.disconnect()
    
    def add_subscription(self):
        """添加订阅"""
        topic = self.sub_topic_input.text().strip()
        qos = int(self.sub_qos_combo.currentText())
        
        if not topic:
            QMessageBox.warning(self, "错误", "请输入订阅主题")
            return
        
        if topic in self.subscribed_topics:
            QMessageBox.warning(self, "错误", "该主题已订阅")
            return
        
        if not self.mqtt_client.is_connected:
            QMessageBox.warning(self, "错误", "请先连接到Broker")
            return
        
        # 订阅
        if self.mqtt_client.subscribe(topic, qos):
            self.subscribed_topics[topic] = qos
            
            # 自动分配颜色（如果该主题还没有颜色）
            if topic not in self.topic_colors:
                self.topic_colors[topic] = self.preset_colors[self.color_index % len(self.preset_colors)]
                self.color_index += 1
            
            # 添加到表格
            row = self.sub_table.rowCount()
            self.sub_table.insertRow(row)
            self.sub_table.setItem(row, 0, QTableWidgetItem(topic))
            self.sub_table.setItem(row, 1, QTableWidgetItem(str(qos)))
            
            # 颜色选择按钮
            color_btn = QPushButton("██")
            color_btn.setMaximumWidth(50)
            current_color = self.topic_colors.get(topic, '#000000')
            color_btn.setStyleSheet(f"background-color: {current_color}; color: white; font-weight: bold; border: 1px solid #999;")
            color_btn.clicked.connect(lambda checked, t=topic: self.choose_topic_color(t))
            self.sub_table.setCellWidget(row, 2, color_btn)
            
            # 删除按钮
            remove_btn = QPushButton("删除")
            remove_btn.clicked.connect(lambda: self.remove_subscription(topic))
            self.sub_table.setCellWidget(row, 3, remove_btn)
            
            # 清空输入
            self.sub_topic_input.clear()
            
            self.append_log(f"[订阅] {topic} (QoS {qos}) 颜色: {current_color}")
    
    def choose_topic_color(self, topic: str):
        """选择主题颜色"""
        current_color = QColor(self.topic_colors.get(topic, '#000000'))
        color = QColorDialog.getColor(current_color, self, f"选择主题颜色: {topic}")
        
        if color.isValid():
            color_hex = color.name()
            self.topic_colors[topic] = color_hex
            
            # 更新表格中的颜色按钮
            for row in range(self.sub_table.rowCount()):
                if self.sub_table.item(row, 0).text() == topic:
                    color_btn = self.sub_table.cellWidget(row, 2)
                    if color_btn:
                        color_btn.setStyleSheet(f"background-color: {color_hex}; color: white; font-weight: bold; border: 1px solid #999;")
                    break
            
            self.append_log(f"[颜色] {topic} 设置为 {color_hex}")
    
    def remove_subscription(self, topic: str):
        """删除订阅"""
        if topic in self.subscribed_topics:
            self.mqtt_client.unsubscribe(topic)
            del self.subscribed_topics[topic]
            
            # 删除颜色映射（可选，保留可以记忆颜色）
            # if topic in self.topic_colors:
            #     del self.topic_colors[topic]
            
            # 从表格删除
            for row in range(self.sub_table.rowCount()):
                if self.sub_table.item(row, 0).text() == topic:
                    self.sub_table.removeRow(row)
                    break
            
            self.append_log(f"[取消订阅] {topic}")
    
    def publish_message(self):
        """发布消息"""
        topic = self.pub_topic_input.text().strip()
        message = self.pub_message_input.toPlainText().strip()
        qos = int(self.pub_qos_combo.currentText())
        
        if not topic:
            QMessageBox.warning(self, "错误", "请输入发布主题")
            return
        
        if not message:
            QMessageBox.warning(self, "错误", "请输入消息内容")
            return
        
        if not self.mqtt_client.is_connected:
            QMessageBox.warning(self, "错误", "请先连接到Broker")
            return
        
        self.mqtt_client.publish(topic, message, qos)
    
    def on_connected(self, success: bool, message: str):
        """连接完成"""
        self.connect_btn.setEnabled(True)
        
        if success:
            self.status_label.setText("已连接")
            self.status_label.setStyleSheet("color: green; font-weight: bold;")
            self.connect_btn.setText("断开")
            self.connect_btn.setObjectName("disconnect_btn")
            self.connect_btn.setStyle(self.connect_btn.style())  # 刷新样式
            self.append_log(f"[系统] {message}")
        else:
            self.status_label.setText("连接失败")
            self.status_label.setStyleSheet("color: red; font-weight: bold;")
            self.connect_btn.setText("连接")
            self.connect_btn.setObjectName("connect_btn")
            self.connect_btn.setStyle(self.connect_btn.style())  # 刷新样式
            QMessageBox.critical(self, "连接失败", message)
    
    def on_disconnected(self, message: str):
        """断开连接"""
        self.status_label.setText("未连接")
        self.status_label.setStyleSheet("color: gray; font-weight: bold;")
        self.connect_btn.setText("连接")
        self.connect_btn.setObjectName("connect_btn")
        self.connect_btn.setStyle(self.connect_btn.style())  # 刷新样式
        self.connect_btn.setEnabled(True)
        self.append_log(f"[系统] {message}")
        
        # 清空订阅列表
        self.subscribed_topics.clear()
        self.sub_table.setRowCount(0)
    
    def on_message_received(self, timestamp: str, topic: str, payload: str):
        """接收到消息"""
        # 检查是否为status主题（LWT消息）
        is_status_topic = '/status' in topic
        
        # 获取主题颜色（默认黑色）
        topic_color = self.topic_colors.get(topic, '#000000')
        
        # 使用HTML格式化显示，应用主题颜色
        if is_status_topic:
            msg = f'<span style="color: {topic_color}; font-weight: bold;">[{timestamp}] 🔔 [LWT] {topic}</span><br>'
        else:
            msg = f'<span style="color: {topic_color}; font-weight: bold;">[{timestamp}] 📩 {topic}</span><br>'
        
        # 尝试格式化JSON
        try:
            json_obj = json.loads(payload)
            payload_display = json.dumps(json_obj, indent=2, ensure_ascii=False)
            
            # 如果是status主题，检查在线状态
            if is_status_topic and 'online' in json_obj:
                online = json_obj.get('online', False)
                reason = json_obj.get('reason', '')
                
                # 发送设备状态变化信号
                self.mqtt_client.signals.device_status_changed.emit(online, reason, timestamp)
                
                # 高亮显示
                if online:
                    msg += '<span style="color: green;">🟢 设备上线</span><br>'
                else:
                    if reason == 'abnormal_disconnect':
                        msg += '<span style="color: red;">🔴 设备异常离线（LWT触发）</span><br>'
                    elif reason == 'normal_shutdown':
                        msg += '<span style="color: orange;">🟠 设备正常离线</span><br>'
                    else:
                        msg += '<span style="color: red;">🔴 设备离线</span><br>'
            
            # JSON内容使用主题颜色显示
            msg += f'<pre style="color: {topic_color}; margin: 5px 0;">{payload_display}</pre>'
            
            # 检查是否为ACK消息，需要自动回复
            # 只要是发送到ack主题且包含message_id的消息就回复
            if self.auto_ack_checkbox.isChecked() and '/ack' in topic and 'message_id' in json_obj:
                self.auto_reply_ack(json_obj['message_id'])
        except:
            # 非-JSON数据使用主题颜色显示
            msg += f'<pre style="color: {topic_color}; margin: 5px 0;">{payload}</pre>'
        
        msg += '<hr style="border: none; border-top: 1px solid #ddd; margin: 10px 0;">'
        
        # 使用insertHtml而不append以支持HTML格式
        cursor = self.msg_history.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        self.msg_history.setTextCursor(cursor)
        self.msg_history.insertHtml(msg)
        
        # 滚动到底部
        self.msg_history.verticalScrollBar().setValue(
            self.msg_history.verticalScrollBar().maximum()
        )
    
    def on_published(self, success: bool, message: str):
        """发布完成"""
        if success:
            self.append_log(f"[发送] {message}")
        else:
            QMessageBox.warning(self, "发送失败", message)
    
    def append_log(self, text: str):
        """追加日志"""
        timestamp = datetime.now().strftime('%H:%M:%S')
        self.msg_history.append(f"[{timestamp}] {text}")
        self.msg_history.verticalScrollBar().setValue(
            self.msg_history.verticalScrollBar().maximum()
        )
    
    def fill_topic(self, topic_type: str, for_subscription: bool):
        """填充快捷主题"""
        # 广播主题不拼接设备ID
        if topic_type == "broadcast":
            topic = "devices/broadcast"
        else:
            topic = f"devices/{self.device_id}/{topic_type}"
        
        if for_subscription:
            self.sub_topic_input.setText(topic)
        else:
            self.pub_topic_input.setText(topic)
    
    def on_template_category_changed(self, category: str):
        """模板分类改变"""
        self.template_item_combo.clear()
        self.template_item_combo.addItem("-- 选择模板 --")
        
        if category and category != "-- 选择分类 --" and category in MESSAGE_TEMPLATES:
            templates = MESSAGE_TEMPLATES[category]
            self.template_item_combo.addItems(list(templates.keys()))
            self.template_item_combo.setEnabled(True)
        else:
            self.template_item_combo.setEnabled(False)
    
    def on_template_selected(self, template_name: str):
        """模板选择"""
        if not template_name or template_name == "-- 选择模板 --":
            return
        
        category = self.template_category_combo.currentText()
        if category and category != "-- 选择分类 --" and category in MESSAGE_TEMPLATES:
            templates = MESSAGE_TEMPLATES[category]
            if template_name in templates:
                template_data = templates[template_name]
                json_str = json.dumps(template_data, indent=2, ensure_ascii=False)
                self.pub_message_input.setPlainText(json_str)
    
    def on_device_status_changed(self, online: bool, reason: str, timestamp: str):
        """设备在线状态变化"""
        self.device_online = online
        
        if online:
            # 设备上线
            self.online_count += 1
            self.last_online_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            self.device_status_label.setText("🟢 在线")
            self.device_status_label.setStyleSheet("color: green; font-weight: bold; font-size: 11pt;")
            self.last_online_label.setText(f"最后上线: {self.last_online_time}")
            self.online_count_label.setText(str(self.online_count))
        else:
            # 设备离线
            self.offline_count += 1
            self.last_offline_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            
            if reason == 'abnormal_disconnect':
                self.device_status_label.setText("🔴 离线(异常)")
                self.device_status_label.setStyleSheet("color: red; font-weight: bold; font-size: 11pt;")
            elif reason == 'normal_shutdown':
                self.device_status_label.setText("🟠 离线(正常)")
                self.device_status_label.setStyleSheet("color: orange; font-weight: bold; font-size: 11pt;")
            else:
                self.device_status_label.setText("🔴 离线")
                self.device_status_label.setStyleSheet("color: red; font-weight: bold; font-size: 11pt;")
            
            self.last_offline_label.setText(f"最后离线: {self.last_offline_time} ({reason})")
            self.offline_count_label.setText(str(self.offline_count))
    
    def auto_reply_ack(self, message_id: str):
        """自动回复ACK确认"""
        if not self.mqtt_client.is_connected:
            return
        
        # 构造ack_receipt消息
        ack_receipt = {
            "type": "ack_receipt",
            "message_id": message_id,
            "received_at": int(datetime.now().timestamp()),
            "status": "processed"
        }
        
        # 发送到downlink主题
        downlink_topic = f"devices/{self.device_id}/downlink"
        payload = json.dumps(ack_receipt, ensure_ascii=False)
        
        # 使用QoS 1发送
        self.mqtt_client.publish(downlink_topic, payload, qos=1)
        
        # 记录日志
        self.append_log(f"[自动回复] ACK确认 -> {message_id}")
    
    def closeEvent(self, event):
        """关闭窗口"""
        if self.mqtt_client.is_connected:
            self.mqtt_client.disconnect()
        event.accept()


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
