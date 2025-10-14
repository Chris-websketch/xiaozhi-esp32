#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
闹钟MQTT中间件服务
提供HTTP API接口设置闹钟，通过MQTT下发给设备并等待确认
"""

import json
import uuid
import threading
import time
from typing import Dict, Optional
from datetime import datetime
from flask import Flask, request, jsonify
from flask_cors import CORS
import paho.mqtt.client as mqtt_client

# ============ 配置 ============
MQTT_BROKER = "x6bf310e.ala.cn-hangzhou.emqxsl.cn"
MQTT_PORT = 8883
MQTT_USERNAME = "xiaoqiao"
MQTT_PASSWORD = "dzkj0000"
MQTT_USE_SSL = True
MQTT_CA_CERT = "emqx_ca.crt"

HTTP_PORT = 6999

# ACK等待超时（秒）
ACK_TIMEOUT = 10

# ============ Flask应用 ============
app = Flask(__name__)
CORS(app)  # 允许跨域请求

# ============ MQTT中间件类 ============
class AlarmMQTTMiddleware:
    def __init__(self):
        self.client = None
        self.is_connected = False
        self.device_heartbeats = {}  # 设备心跳时间记录 {device_id: timestamp}
        self.ONLINE_THRESHOLD = 120  # 120秒内有心跳视为在线
        
    def connect(self):
        """连接到MQTT Broker"""
        try:
            # 创建客户端
            client_id = f"alarm_middleware_{uuid.uuid4().hex[:8]}"
            self.client = mqtt_client.Client(
                client_id=client_id,
                callback_api_version=mqtt_client.CallbackAPIVersion.VERSION2
            )
            
            # 设置用户名密码
            self.client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
            
            # 设置SSL
            if MQTT_USE_SSL:
                self.client.tls_set(ca_certs=MQTT_CA_CERT)
            
            # 设置回调
            self.client.on_connect = self._on_connect
            self.client.on_message = self._on_message
            self.client.on_disconnect = self._on_disconnect
            
            # 连接
            print(f"正在连接到MQTT Broker {MQTT_BROKER}:{MQTT_PORT}...")
            self.client.connect(MQTT_BROKER, MQTT_PORT, keepalive=60)
            self.client.loop_start()
            
            # 等待连接成功
            timeout = 10
            start_time = time.time()
            while not self.is_connected and (time.time() - start_time) < timeout:
                time.sleep(0.1)
            
            if self.is_connected:
                print("✅ MQTT连接成功")
                return True
            else:
                print("❌ MQTT连接超时")
                return False
                
        except Exception as e:
            print(f"❌ MQTT连接失败: {e}")
            return False
    
    def _on_connect(self, client, userdata, flags, rc, properties=None):
        """连接回调"""
        if rc == 0:
            print("MQTT连接成功")
            self.is_connected = True
            # 订阅所有设备的ACK主题（使用通配符）
            self.client.subscribe("devices/+/ack", qos=2)
            print("已订阅: devices/+/ack")
            # 订阅所有设备的Uplink主题（用于接收心跳）
            self.client.subscribe("devices/+/uplink", qos=0)
            print("已订阅: devices/+/uplink")
        else:
            print(f"MQTT连接失败，返回码: {rc}")
            self.is_connected = False
    
    def _on_disconnect(self, client, userdata, rc, properties=None):
        """断开连接回调"""
        print(f"MQTT连接断开，返回码: {rc}")
        self.is_connected = False
    
    def _on_message(self, client, userdata, msg):
        """消息接收回调"""
        try:
            payload = json.loads(msg.payload.decode())
            print(f"📨 收到消息: {msg.topic}")
            print(f"   内容: {json.dumps(payload, ensure_ascii=False)}")
            
            # 处理心跳消息，记录设备在线状态
            if payload.get('type') == 'telemetry':
                # 从主题中提取device_id: devices/{device_id}/uplink
                topic_parts = msg.topic.split('/')
                if len(topic_parts) >= 3:
                    device_id = topic_parts[1]
                    self.device_heartbeats[device_id] = time.time()
                    print(f"💓 设备心跳: device_id={device_id}")
            
            # 仅记录设备的ACK消息，不做处理
            elif payload.get('type') == 'ack':
                status = payload.get('status', 'unknown')
                request_id = payload.get('request_id', 'unknown')
                print(f"✅ 设备ACK: status={status}, request_id={request_id}")
                        
        except Exception as e:
            print(f"❌ 处理消息失败: {e}")
    
    def send_alarm_command(self, device_id: str, command: dict) -> dict:
        """
        发送闹钟命令（不等待ACK）
        
        Args:
            device_id: 设备ID
            command: 命令内容（SetAlarm或CancelAlarm）
            
        Returns:
            dict: 结果 {'success': bool, 'message': str}
        """
        if not self.is_connected:
            return {
                'success': False,
                'message': 'MQTT未连接'
            }
        
        # 生成唯一的request_id
        request_id = f"req_{int(time.time())}_{uuid.uuid4().hex[:8]}"
        
        # 构建IoT消息
        message = {
            "type": "iot",
            "commands": [command],
            "request_id": request_id
        }
        
        try:
            # 发布消息
            topic = f"devices/{device_id}/downlink"
            payload = json.dumps(message, ensure_ascii=False)
            
            print(f"📤 发送命令到: {topic}")
            print(f"   内容: {payload}")
            
            result = self.client.publish(topic, payload, qos=2)
            
            if result.rc != mqtt_client.MQTT_ERR_SUCCESS:
                return {
                    'success': False,
                    'message': f'MQTT发送失败: {result.rc}'
                }
            
            # 命令已发送，直接返回成功
            return {
                'success': True,
                'message': '命令已发送'
            }
                
        except Exception as e:
            return {
                'success': False,
                'message': f'发送失败: {str(e)}'
            }
    
    def get_device_status(self, device_id: str) -> dict:
        """
        获取设备在线状态
        
        Args:
            device_id: 设备ID
            
        Returns:
            dict: 设备状态信息
        """
        current_time = time.time()
        
        if device_id not in self.device_heartbeats:
            return {
                'device_id': device_id,
                'online': False,
                'last_heartbeat': None,
                'can_set_alarm': False,
                'seconds_since_heartbeat': None,
                'mqtt_connected': self.is_connected
            }
        
        last_heartbeat = self.device_heartbeats[device_id]
        seconds_since = int(current_time - last_heartbeat)
        is_online = seconds_since <= self.ONLINE_THRESHOLD
        
        return {
            'device_id': device_id,
            'online': is_online,
            'last_heartbeat': datetime.fromtimestamp(last_heartbeat).isoformat(),
            'can_set_alarm': is_online and self.is_connected,
            'seconds_since_heartbeat': seconds_since,
            'mqtt_connected': self.is_connected
        }
    
    def disconnect(self):
        """断开MQTT连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.is_connected = False
            print("MQTT已断开")

# 全局中间件实例
middleware = AlarmMQTTMiddleware()

# ============ HTTP API端点 ============

@app.route('/health', methods=['GET'])
def health_check():
    """健康检查"""
    return jsonify({
        'status': 'ok',
        'mqtt_connected': middleware.is_connected,
        'timestamp': datetime.now().isoformat()
    })

@app.route('/api/v1/device/status', methods=['GET'])
def device_status():
    """
    检查设备在线状态
    
    参数:
        device_id: 设备ID（query参数）
        
    返回:
        设备状态信息，包括是否在线、最后心跳时间、是否可设置闹钟
    """
    device_id = request.args.get('device_id')
    
    if not device_id:
        return jsonify({
            'success': False,
            'message': '缺少device_id参数'
        }), 400
    
    status = middleware.get_device_status(device_id)
    
    return jsonify({
        'success': True,
        'data': status
    }), 200

@app.route('/api/v1/alarm/set', methods=['POST'])
def set_alarm():
    """
    设置闹钟
    
    请求体:
    {
        "device_id": "设备ID",
        "id": "闹钟ID",
        "repeat_type": 0,  // 0=ONCE, 1=DAILY, 2=WEEKLY
        "seconds": 0,      // ONCE类型使用
        "hour": 0,         // DAILY/WEEKLY使用
        "minute": 0,       // DAILY/WEEKLY使用
        "repeat_days": 0   // WEEKLY使用（位掩码）
    }
    
    返回:
    {
        "success": true/false,
        "message": "消息",
        "data": {...}
    }
    """
    try:
        data = request.get_json()
        
        # 验证必需字段
        required_fields = ['device_id', 'id', 'repeat_type', 'seconds', 'hour', 'minute', 'repeat_days']
        missing = [f for f in required_fields if f not in data]
        if missing:
            return jsonify({
                'success': False,
                'message': f'缺少必需字段: {", ".join(missing)}'
            }), 400
        
        device_id = data['device_id']
        
        # 构建SetAlarm命令
        command = {
            "name": "Alarm",
            "method": "SetAlarm",
            "parameters": {
                "id": data['id'],
                "repeat_type": int(data['repeat_type']),
                "seconds": int(data['seconds']),
                "hour": int(data['hour']),
                "minute": int(data['minute']),
                "repeat_days": int(data['repeat_days'])
            }
        }
        
        # 发送命令
        result = middleware.send_alarm_command(device_id, command)
        
        return jsonify({
            'success': result['success'],
            'message': result['message'],
            'data': {
                'alarm_id': data['id']
            }
        }), 200 if result['success'] else 500
        
    except Exception as e:
        return jsonify({
            'success': False,
            'message': f'服务器错误: {str(e)}'
        }), 500

@app.route('/api/v1/alarm/cancel', methods=['POST'])
def cancel_alarm():
    """
    取消闹钟
    
    请求体:
    {
        "device_id": "设备ID",
        "id": "闹钟ID"
    }
    """
    try:
        data = request.get_json()
        
        if 'device_id' not in data or 'id' not in data:
            return jsonify({
                'success': False,
                'message': '缺少device_id或id字段'
            }), 400
        
        device_id = data['device_id']
        alarm_id = data['id']
        
        # 构建CancelAlarm命令
        command = {
            "name": "Alarm",
            "method": "CancelAlarm",
            "parameters": {
                "id": alarm_id
            }
        }
        
        # 发送命令
        result = middleware.send_alarm_command(device_id, command)
        
        return jsonify({
            'success': result['success'],
            'message': result['message'],
            'data': {
                'alarm_id': alarm_id
            }
        }), 200 if result['success'] else 500
        
    except Exception as e:
        return jsonify({
            'success': False,
            'message': f'服务器错误: {str(e)}'
        }), 500

@app.route('/api/v1/alarm/quick', methods=['POST'])
def quick_alarm():
    """
    快捷设置闹钟（简化接口）
    
    请求体:
    {
        "device_id": "设备ID",
        "id": "闹钟ID",
        "type": "once|daily|weekly|workdays|weekends",
        
        // type=once 时使用:
        "seconds": 60,
        
        // type=daily/weekly/workdays/weekends 时使用:
        "hour": 7,
        "minute": 30,
        
        // type=weekly 时额外使用:
        "weekdays": [1, 3, 5]  // 周一、三、五
    }
    """
    try:
        data = request.get_json()
        
        if 'device_id' not in data or 'id' not in data or 'type' not in data:
            return jsonify({
                'success': False,
                'message': '缺少必需字段: device_id, id, type'
            }), 400
        
        device_id = data['device_id']
        alarm_id = data['id']
        alarm_type = data['type'].lower()
        
        # 根据type构建参数
        params = {
            "id": alarm_id,
            "repeat_type": 0,
            "seconds": 0,
            "hour": 0,
            "minute": 0,
            "repeat_days": 0
        }
        
        if alarm_type == 'once':
            params['repeat_type'] = 0
            params['seconds'] = int(data.get('seconds', 60))
            
        elif alarm_type == 'daily':
            params['repeat_type'] = 1
            params['hour'] = int(data.get('hour', 0))
            params['minute'] = int(data.get('minute', 0))
            
        elif alarm_type == 'weekly':
            params['repeat_type'] = 2
            params['hour'] = int(data.get('hour', 0))
            params['minute'] = int(data.get('minute', 0))
            
            # 处理weekdays数组，转换为位掩码
            weekdays = data.get('weekdays', [])
            repeat_days = 0
            for day in weekdays:
                if 0 <= day <= 6:
                    repeat_days |= (1 << day)
            params['repeat_days'] = repeat_days
            
        elif alarm_type == 'workdays':
            params['repeat_type'] = 2
            params['hour'] = int(data.get('hour', 0))
            params['minute'] = int(data.get('minute', 0))
            params['repeat_days'] = 62  # 0b00111110 周一到周五
            
        elif alarm_type == 'weekends':
            params['repeat_type'] = 2
            params['hour'] = int(data.get('hour', 0))
            params['minute'] = int(data.get('minute', 0))
            params['repeat_days'] = 65  # 0b01000001 周六周日
            
        else:
            return jsonify({
                'success': False,
                'message': f'不支持的类型: {alarm_type}，支持: once, daily, weekly, workdays, weekends'
            }), 400
        
        # 构建命令
        command = {
            "name": "Alarm",
            "method": "SetAlarm",
            "parameters": params
        }
        
        # 发送命令
        result = middleware.send_alarm_command(device_id, command)
        
        return jsonify({
            'success': result['success'],
            'message': result['message'],
            'data': {
                'alarm_id': alarm_id,
                'alarm_type': alarm_type,
                'parameters': params
            }
        }), 200 if result['success'] else 500
        
    except Exception as e:
        return jsonify({
            'success': False,
            'message': f'服务器错误: {str(e)}'
        }), 500

# ============ 启动服务 ============
if __name__ == '__main__':
    print("=" * 60)
    print("闹钟MQTT中间件服务")
    print("=" * 60)
    
    # 连接MQTT
    if not middleware.connect():
        print("❌ 无法连接到MQTT Broker，退出")
        exit(1)
    
    print(f"\n🚀 启动HTTP服务器 http://0.0.0.0:{HTTP_PORT}")
    print("\nAPI端点:")
    print(f"  - POST /api/v1/alarm/set       设置闹钟（完整参数）")
    print(f"  - POST /api/v1/alarm/cancel    取消闹钟")
    print(f"  - POST /api/v1/alarm/quick     快速设置闹钟（简化接口）")
    print(f"  - GET  /api/v1/device/status   检查设备在线状态")
    print(f"  - GET  /health                 健康检查")
    print("\n" + "=" * 60 + "\n")
    
    try:
        app.run(host='0.0.0.0', port=HTTP_PORT, debug=False)
    except KeyboardInterrupt:
        print("\n\n正在关闭...")
    finally:
        middleware.disconnect()
        print("服务已停止")
