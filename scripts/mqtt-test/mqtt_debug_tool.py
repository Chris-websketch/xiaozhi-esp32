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
    QSplitter, QHeaderView, QMessageBox, QSpinBox
)
from PySide6.QtCore import Qt, Signal, QObject
from PySide6.QtGui import QFont

from paho.mqtt import client as mqtt_client


# 默认配置 - 项目专用MQTT服务器
DEFAULT_BROKER = '110.42.35.132'
DEFAULT_PORT = 1883
DEFAULT_USERNAME = 'xiaoqiao'
DEFAULT_PASSWORD = 'dzkj0000'
DEVICE_CLIENT_ID = '719ae1ad-9f2c-4277-9c99-1a317a478979'  # ESP32设备ID
DEBUG_CLIENT_ID = 'mqtt-debug-tool-' + ''.join(['{:02x}'.format(random.randint(0, 255)) for _ in range(4)])  # 调试工具专用ID
CA_CERT_FILE = 'emqx_ca.crt'


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
    }
}


class MQTTSignals(QObject):
    """MQTT信号，用于线程间通信"""
    connected = Signal(bool, str)  # (success, message)
    disconnected = Signal(str)  # message
    message_received = Signal(str, str, str)  # (timestamp, topic, payload)
    published = Signal(bool, str)  # (success, message)


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
        self.device_id = DEVICE_CLIENT_ID  # 设备ID用于主题拼接
        self.init_ui()
        self.connect_signals()
        
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle('MQTT调试工具')
        self.setGeometry(100, 100, 1200, 800)
        
        # 主部件
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # 连接配置区
        conn_group = self.create_connection_group()
        main_layout.addWidget(conn_group)
        
        # 分割器 - 上下分割
        main_splitter = QSplitter(Qt.Vertical)
        
        # 上半部分 - 左右分割
        top_splitter = QSplitter(Qt.Horizontal)
        
        # 订阅管理区（左）
        sub_group = self.create_subscription_group()
        top_splitter.addWidget(sub_group)
        
        # 发布区（右）
        pub_group = self.create_publish_group()
        top_splitter.addWidget(pub_group)
        
        top_splitter.setSizes([400, 600])
        main_splitter.addWidget(top_splitter)
        
        # 消息历史区（下）
        msg_group = self.create_message_history_group()
        main_splitter.addWidget(msg_group)
        
        main_splitter.setSizes([300, 400])
        main_layout.addWidget(main_splitter)
        
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
        self.ssl_checkbox.setChecked(True)  # 默认启用SSL
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
        
        quick_layout.addStretch()
        layout.addLayout(quick_layout)
        
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
        self.sub_table.setColumnCount(3)
        self.sub_table.setHorizontalHeaderLabels(['主题', 'QoS', '操作'])
        self.sub_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.sub_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.sub_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.sub_table.setSelectionBehavior(QTableWidget.SelectRows)
        layout.addWidget(self.sub_table)
        
        group.setLayout(layout)
        return group
    
    def create_publish_group(self):
        """创建发布组"""
        group = QGroupBox("消息发布")
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
        
        # 消息模板选择
        template_layout = QHBoxLayout()
        template_layout.addWidget(QLabel("消息模板:"))
        
        self.template_category_combo = QComboBox()
        self.template_category_combo.addItem("-- 选择分类 --")
        self.template_category_combo.addItems(list(MESSAGE_TEMPLATES.keys()))
        self.template_category_combo.currentTextChanged.connect(self.on_template_category_changed)
        template_layout.addWidget(self.template_category_combo)
        
        self.template_item_combo = QComboBox()
        self.template_item_combo.addItem("-- 选择模板 --")
        self.template_item_combo.setEnabled(False)
        self.template_item_combo.currentTextChanged.connect(self.on_template_selected)
        template_layout.addWidget(self.template_item_combo)
        
        template_layout.addStretch()
        layout.addLayout(template_layout)
        
        # 消息内容
        layout.addWidget(QLabel("消息内容:"))
        self.pub_message_input = QTextEdit()
        self.pub_message_input.setPlaceholderText('输入消息内容或从上方选择模板\n支持JSON格式')
        self.pub_message_input.setMaximumHeight(150)
        layout.addWidget(self.pub_message_input)
        
        # 发送按钮
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        self.publish_btn = QPushButton("发送")
        self.publish_btn.setMaximumWidth(100)
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
            
            # 添加到表格
            row = self.sub_table.rowCount()
            self.sub_table.insertRow(row)
            self.sub_table.setItem(row, 0, QTableWidgetItem(topic))
            self.sub_table.setItem(row, 1, QTableWidgetItem(str(qos)))
            
            # 删除按钮
            remove_btn = QPushButton("删除")
            remove_btn.clicked.connect(lambda: self.remove_subscription(topic))
            self.sub_table.setCellWidget(row, 2, remove_btn)
            
            # 清空输入
            self.sub_topic_input.clear()
            
            self.append_log(f"[订阅] {topic} (QoS {qos})")
    
    def remove_subscription(self, topic: str):
        """删除订阅"""
        if topic in self.subscribed_topics:
            self.mqtt_client.unsubscribe(topic)
            del self.subscribed_topics[topic]
            
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
            self.append_log(f"[系统] {message}")
        else:
            self.status_label.setText("连接失败")
            self.status_label.setStyleSheet("color: red; font-weight: bold;")
            self.connect_btn.setText("连接")
            QMessageBox.critical(self, "连接失败", message)
    
    def on_disconnected(self, message: str):
        """断开连接"""
        self.status_label.setText("未连接")
        self.status_label.setStyleSheet("color: gray; font-weight: bold;")
        self.connect_btn.setText("连接")
        self.connect_btn.setEnabled(True)
        self.append_log(f"[系统] {message}")
        
        # 清空订阅列表
        self.subscribed_topics.clear()
        self.sub_table.setRowCount(0)
    
    def on_message_received(self, timestamp: str, topic: str, payload: str):
        """接收到消息"""
        # 格式化显示
        msg = f"[{timestamp}] 📩 {topic}\n"
        
        # 尝试格式化JSON
        try:
            json_obj = json.loads(payload)
            payload_display = json.dumps(json_obj, indent=2, ensure_ascii=False)
            msg += f"{payload_display}\n"
            
            # 检查是否为ACK消息，需要自动回复
            # 只要是发送到ack主题且包含message_id的消息就回复
            if self.auto_ack_checkbox.isChecked() and '/ack' in topic and 'message_id' in json_obj:
                self.auto_reply_ack(json_obj['message_id'])
        except:
            msg += f"{payload}\n"
        
        msg += "-" * 80 + "\n"
        
        self.msg_history.append(msg)
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
