# BluFi 蓝牙配网小程序对接文档

> 版本: 1.0  
> 更新日期: 2024-12  
> 适用固件: (abrobot-1.28tft-wifi)

---

## 目录

1. [概述](#1-概述)
2. [BLE 服务与特征](#2-ble-服务与特征)
3. [BluFi 帧格式](#3-blufi-帧格式)
4. [安全协商流程](#4-安全协商流程)
5. [配网流程](#5-配网流程)
6. [帧类型定义](#6-帧类型定义)
7. [错误码定义](#7-错误码定义)
8. [微信小程序示例代码](#8-微信小程序示例代码)
9. [调试建议](#9-调试建议)

---

## 1. 概述

### 1.1 什么是 BluFi

BluFi 是乐鑫（Espressif）开发的一种基于蓝牙低功耗（BLE）的 WiFi 配网协议。它允许用户通过手机 APP 或小程序，将 WiFi 凭据安全地传输给 ESP32 设备。

### 1.2 安全特性

| 特性 | 说明 |
|------|------|
| 密钥交换 | Diffie-Hellman (DH) 1024-bit |
| 数据加密 | AES-128-CFB |
| 完整性校验 | CRC16-CCITT |
| PSK 派生 | MD5(共享密钥) → 16字节 AES 密钥 |

### 1.3 配网流程概览

```
┌─────────────┐                          ┌─────────────┐
│  小程序端   │                          │  ESP32设备  │
└──────┬──────┘                          └──────┬──────┘
       │                                        │
       │  1. 扫描发现设备 (BLUFI_DEVICE)        │
       │ ────────────────────────────────────>  │
       │                                        │
       │  2. 建立 BLE 连接                      │
       │ <────────────────────────────────────> │
       │                                        │
       │  3. 安全协商 (DH 密钥交换)             │
       │ <────────────────────────────────────> │
       │                                        │
       │  4. 发送 WiFi SSID (AES 加密)          │
       │ ────────────────────────────────────>  │
       │                                        │
       │  5. 发送 WiFi 密码 (AES 加密)          │
       │ ────────────────────────────────────>  │
       │                                        │
       │  6. 发送连接请求                       │
       │ ────────────────────────────────────>  │
       │                                        │
       │  7. 返回连接结果                       │
       │ <────────────────────────────────────  │
       │                                        │
       │  8. 设备重启，配网完成                 │
       │                                        │
```

---

## 2. BLE 服务与特征

### 2.1 设备广播名称

```
BLUFI_DEVICE
```

小程序扫描时，可通过设备名称前缀 `BLUFI` 过滤目标设备。

### 2.2 服务 UUID

| 名称 | UUID |
|------|------|
| BluFi 服务 | `0000FFFF-0000-1000-8000-00805F9B34FB` |

### 2.3 特征 UUID

| 名称 | UUID | 属性 | 说明 |
|------|------|------|------|
| P2E (Phone to ESP32) | `0000FF01-0000-1000-8000-00805F9B34FB` | Write | 小程序写入数据 |
| E2P (ESP32 to Phone) | `0000FF02-0000-1000-8000-00805F9B34FB` | Notify | 设备通知数据 |

### 2.4 连接参数建议

| 参数 | 推荐值 |
|------|--------|
| MTU | 512 (协商后) |
| 连接间隔 | 7.5ms - 30ms |
| 超时时间 | 5000ms |

---

## 3. BluFi 帧格式

### 3.1 帧结构

```
+--------+----------+--------+---------+--------+----------+
| Type   | FrameCtrl| SeqNum | DataLen | Data   | Checksum |
| 1 byte | 1 byte   | 1 byte | 1 byte  | N bytes| 2 bytes  |
+--------+----------+--------+---------+--------+----------+
```

### 3.2 字段说明

#### Type 字段 (1 byte)

```
Bit 7-6: 帧类型 (Frame Type)
  - 00: 控制帧 (Ctrl)
  - 01: 数据帧 (Data)
  
Bit 5-0: 子类型 (Subtype)
```

#### FrameCtrl 字段 (1 byte)

```
Bit 0: 加密标志 (Encrypted)
  - 0: 未加密
  - 1: AES-128-CFB 加密
  
Bit 1: 校验和标志 (Checksum)
  - 0: 无校验和
  - 1: 包含 CRC16 校验和
  
Bit 2: 数据方向 (Direction)
  - 0: 小程序 → 设备
  - 1: 设备 → 小程序
  
Bit 3: 需要 ACK (Require ACK)
  - 0: 不需要
  - 1: 需要
  
Bit 4: 分片标志 (Fragment)
  - 0: 非分片/最后一片
  - 1: 有后续分片
  
Bit 7-5: 保留
```

#### SeqNum 字段 (1 byte)

帧序列号，每发送一帧递增 1，范围 0-255 循环。

#### DataLen 字段 (1 byte)

数据部分长度（不包括校验和），最大 255 字节。

#### Checksum 字段 (2 bytes, 可选)

CRC16-CCITT 校验和（小端序），计算范围：Type + FrameCtrl + SeqNum + DataLen + Data

### 3.3 CRC16-CCITT 算法

```javascript
function crc16(data) {
  let crc = 0xFFFF;
  for (let byte of data) {
    crc ^= byte << 8;
    for (let i = 0; i < 8; i++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
  }
  return crc & 0xFFFF;
}
```

---

## 4. 安全协商流程

### 4.1 DH 密钥交换步骤

```
┌─────────────┐                          ┌─────────────┐
│  小程序端   │                          │  ESP32设备  │
└──────┬──────┘                          └──────┬──────┘
       │                                        │
       │  1. 请求 DH 参数长度                   │
       │     Type=0x01, Subtype=0x00            │
       │     Data=[0x00]                        │
       │ ────────────────────────────────────>  │
       │                                        │
       │  2. 返回 DH 参数长度                   │
       │     Data=[len_high, len_low]           │
       │ <────────────────────────────────────  │
       │                                        │
       │  3. 请求 DH 参数数据                   │
       │     Data=[0x01]                        │
       │ ────────────────────────────────────>  │
       │                                        │
       │  4. 返回 DH 参数 (p, g)                │
       │     Data=[p(128B) + g(128B)]           │
       │ <────────────────────────────────────  │
       │                                        │
       │  【小程序计算】                        │
       │  - 生成私钥 a (128字节随机数)          │
       │  - 计算公钥 A = g^a mod p              │
       │                                        │
       │  5. 发送小程序公钥                     │
       │     Data=[0x01, A(128B)]               │
       │ ────────────────────────────────────>  │
       │                                        │
       │  【设备计算】                          │
       │  - 生成私钥 b                          │
       │  - 计算公钥 B = g^b mod p              │
       │  - 计算共享密钥 S = A^b mod p          │
       │  - AES密钥 = MD5(S)                    │
       │                                        │
       │  6. 返回设备公钥                       │
       │     Data=[B(128B)]                     │
       │ <────────────────────────────────────  │
       │                                        │
       │  【小程序计算】                        │
       │  - 计算共享密钥 S = B^a mod p          │
       │  - AES密钥 = MD5(S)                    │
       │                                        │
       │  ═══════ 安全通道建立完成 ═══════      │
```

### 4.2 DH 参数格式

设备返回的 DH 参数为 256 字节：
- 前 128 字节: 大素数 p (Big-Endian)
- 后 128 字节: 生成元 g (Big-Endian)

### 4.3 AES 密钥派生

```javascript
// 共享密钥 S 为 128 字节
// AES 密钥 = MD5(S) = 16 字节
const aesKey = md5(sharedSecret);
```

### 4.4 AES-128-CFB 加密

```javascript
// IV 构造: [sequence_number, 0x00, 0x00, ..., 0x00] (16 bytes)
function encrypt(data, sequenceNumber) {
  const iv = new Uint8Array(16);
  iv[0] = sequenceNumber;
  return aesCfbEncrypt(data, aesKey, iv);
}
```

---

## 5. 配网流程

### 5.1 发送 SSID

```
帧类型: 数据帧 (0x01)
子类型: STA_SSID (0x02)
加密: 是
数据: WiFi SSID 的 UTF-8 编码
```

### 5.2 发送密码

```
帧类型: 数据帧 (0x01)
子类型: STA_PASSWD (0x03)
加密: 是
数据: WiFi 密码的 UTF-8 编码
```

### 5.3 发送连接请求

```
帧类型: 控制帧 (0x00)
子类型: CONNECT_WIFI (0x03)
加密: 否
数据: 空
```

### 5.4 接收连接结果

```
帧类型: 数据帧 (0x01)
子类型: WIFI_CONN_REPORT (0x0F)
数据格式:
  - Byte 0: WiFi 模式 (0x00=STA)
  - Byte 1: 连接状态
    - 0x00: 连接成功
    - 0x01: 连接中
    - 0x02: 连接失败
  - Byte 2+: 额外信息 (BSSID, SSID 等)
```

---

## 6. 帧类型定义

### 6.1 控制帧 (Ctrl Frame, Type=0x00)

| Subtype | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 0x00 | ACK | 双向 | 确认帧 |
| 0x01 | SET_SEC_MODE | P→E | 设置安全模式 |
| 0x02 | SET_OP_MODE | P→E | 设置 WiFi 模式 |
| 0x03 | CONNECT_WIFI | P→E | 请求连接 WiFi |
| 0x04 | DISCONNECT_WIFI | P→E | 请求断开 WiFi |
| 0x05 | GET_WIFI_STATUS | P→E | 获取 WiFi 状态 |
| 0x06 | DEAUTHENTICATE | E→P | 解除认证 |
| 0x07 | GET_VERSION | P→E | 获取版本 |
| 0x08 | CLOSE_CONNECTION | P→E | 关闭 BLE 连接 |
| 0x09 | GET_WIFI_LIST | P→E | 获取 WiFi 列表 |

### 6.2 数据帧 (Data Frame, Type=0x01)

| Subtype | 名称 | 方向 | 说明 |
|---------|------|------|------|
| 0x00 | NEG_DATA | 双向 | 安全协商数据 |
| 0x01 | STA_BSSID | P→E | 设置 STA BSSID |
| 0x02 | STA_SSID | P→E | 设置 STA SSID |
| 0x03 | STA_PASSWD | P→E | 设置 STA 密码 |
| 0x04 | SOFTAP_SSID | P→E | 设置 SoftAP SSID |
| 0x05 | SOFTAP_PASSWD | P→E | 设置 SoftAP 密码 |
| 0x06 | SOFTAP_MAX_CONN | P→E | SoftAP 最大连接数 |
| 0x07 | SOFTAP_AUTH_MODE | P→E | SoftAP 认证模式 |
| 0x08 | SOFTAP_CHANNEL | P→E | SoftAP 信道 |
| 0x09 | USERNAME | P→E | 企业级 WiFi 用户名 |
| 0x0A | CA_CERT | P→E | CA 证书 |
| 0x0B | CLIENT_CERT | P→E | 客户端证书 |
| 0x0C | SERVER_CERT | P→E | 服务器证书 |
| 0x0D | CLIENT_PRIV_KEY | P→E | 客户端私钥 |
| 0x0E | SERVER_PRIV_KEY | P→E | 服务器私钥 |
| 0x0F | WIFI_CONN_REPORT | E→P | WiFi 连接报告 |
| 0x10 | VERSION | E→P | 版本信息 |
| 0x11 | WIFI_LIST | E→P | WiFi 列表 |
| 0x12 | ERROR | E→P | 错误信息 |
| 0x13 | CUSTOM_DATA | 双向 | 自定义数据 |

---

## 7. 错误码定义

### 7.1 BluFi 错误码

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0x00 | SUCCESS | 成功 |
| 0x01 | INIT_SECURITY_ERROR | 安全初始化失败 |
| 0x02 | DH_MALLOC_ERROR | DH 内存分配失败 |
| 0x03 | DH_PARAM_ERROR | DH 参数错误 |
| 0x04 | READ_PARAM_ERROR | 读取参数失败 |
| 0x05 | MAKE_PUBLIC_ERROR | 生成公钥失败 |
| 0x06 | DATA_FORMAT_ERROR | 数据格式错误 |
| 0x07 | CALC_MD5_ERROR | MD5 计算失败 |
| 0x08 | ENCRYPT_ERROR | 加密失败 |
| 0x09 | DECRYPT_ERROR | 解密失败 |

### 7.2 WiFi 连接状态

| 状态码 | 名称 | 说明 |
|--------|------|------|
| 0x00 | STA_CONN_SUCCESS | 连接成功 |
| 0x01 | STA_CONN_FAIL | 连接失败 |
| 0x02 | STA_CONNECTING | 连接中 |
| 0x03 | STA_NO_IP | 已连接但未获取 IP |

---

## 8. 微信小程序示例代码

### 8.1 常量定义

```javascript
// utils/blufi/constants.js
export const BLUFI = {
  // 服务和特征 UUID
  SERVICE_UUID: '0000FFFF-0000-1000-8000-00805F9B34FB',
  CHAR_P2E_UUID: '0000FF01-0000-1000-8000-00805F9B34FB',
  CHAR_E2P_UUID: '0000FF02-0000-1000-8000-00805F9B34FB',
  
  // 帧类型
  FRAME_TYPE_CTRL: 0x00,
  FRAME_TYPE_DATA: 0x01,
  
  // 控制帧子类型
  CTRL_SUBTYPE_ACK: 0x00,
  CTRL_SUBTYPE_SET_SEC_MODE: 0x01,
  CTRL_SUBTYPE_SET_OP_MODE: 0x02,
  CTRL_SUBTYPE_CONNECT_WIFI: 0x03,
  CTRL_SUBTYPE_DISCONNECT_WIFI: 0x04,
  CTRL_SUBTYPE_GET_WIFI_STATUS: 0x05,
  
  // 数据帧子类型
  DATA_SUBTYPE_NEG: 0x00,
  DATA_SUBTYPE_STA_BSSID: 0x01,
  DATA_SUBTYPE_STA_SSID: 0x02,
  DATA_SUBTYPE_STA_PASSWD: 0x03,
  DATA_SUBTYPE_WIFI_CONN_REPORT: 0x0F,
  
  // FrameCtrl 标志位
  FRAME_CTRL_ENCRYPTED: 0x01,
  FRAME_CTRL_CHECKSUM: 0x02,
  FRAME_CTRL_DATA_DIR: 0x04,
  FRAME_CTRL_REQUIRE_ACK: 0x08,
  FRAME_CTRL_FRAGMENT: 0x10,
  
  // WiFi 连接状态
  WIFI_CONN_SUCCESS: 0x00,
  WIFI_CONN_FAIL: 0x01,
  WIFI_CONNECTING: 0x02
};
```

### 8.2 帧封装类

```javascript
// utils/blufi/frame.js
import CryptoJS from 'crypto-js';
import { BLUFI } from './constants';

export class BlufiFrame {
  constructor() {
    this.sequence = 0;
    this.aesKey = null;
  }

  setAesKey(key) {
    this.aesKey = key;
  }

  // 构建发送帧
  build(type, subtype, data = [], encrypted = false) {
    const typeField = (type << 6) | (subtype & 0x3F);
    
    let frameCtrl = BLUFI.FRAME_CTRL_CHECKSUM; // 始终启用校验和
    
    let payload = new Uint8Array(data);
    
    if (encrypted && this.aesKey) {
      frameCtrl |= BLUFI.FRAME_CTRL_ENCRYPTED;
      payload = this._encrypt(payload, this.sequence);
    }
    
    // 构建帧头
    const header = new Uint8Array([
      typeField,
      frameCtrl,
      this.sequence,
      payload.length
    ]);
    
    // 合并头部和数据
    const frameWithoutChecksum = new Uint8Array(header.length + payload.length);
    frameWithoutChecksum.set(header, 0);
    frameWithoutChecksum.set(payload, header.length);
    
    // 计算校验和
    const checksum = this._crc16(frameWithoutChecksum);
    
    // 完整帧
    const frame = new Uint8Array(frameWithoutChecksum.length + 2);
    frame.set(frameWithoutChecksum, 0);
    frame[frameWithoutChecksum.length] = checksum & 0xFF;
    frame[frameWithoutChecksum.length + 1] = (checksum >> 8) & 0xFF;
    
    this.sequence = (this.sequence + 1) & 0xFF;
    
    return frame;
  }

  // 解析接收帧
  parse(data) {
    if (data.length < 4) {
      throw new Error('帧长度不足');
    }
    
    const type = (data[0] >> 6) & 0x03;
    const subtype = data[0] & 0x3F;
    const frameCtrl = data[1];
    const sequence = data[2];
    const dataLen = data[3];
    
    const encrypted = (frameCtrl & BLUFI.FRAME_CTRL_ENCRYPTED) !== 0;
    const hasChecksum = (frameCtrl & BLUFI.FRAME_CTRL_CHECKSUM) !== 0;
    
    let payload = data.slice(4, 4 + dataLen);
    
    // 解密
    if (encrypted && this.aesKey) {
      payload = this._decrypt(payload, sequence);
    }
    
    return {
      type,
      subtype,
      sequence,
      frameCtrl,
      payload: new Uint8Array(payload)
    };
  }

  // AES-128-CFB 加密
  _encrypt(data, seq) {
    if (!this.aesKey) return data;
    
    const iv = new Uint8Array(16);
    iv[0] = seq;
    
    const ivWordArray = CryptoJS.lib.WordArray.create(iv);
    const keyWordArray = CryptoJS.enc.Hex.parse(this.aesKey);
    const dataWordArray = CryptoJS.lib.WordArray.create(data);
    
    const encrypted = CryptoJS.AES.encrypt(dataWordArray, keyWordArray, {
      iv: ivWordArray,
      mode: CryptoJS.mode.CFB,
      padding: CryptoJS.pad.NoPadding,
      segmentSize: 128
    });
    
    return this._wordArrayToUint8Array(encrypted.ciphertext, data.length);
  }

  // AES-128-CFB 解密
  _decrypt(data, seq) {
    if (!this.aesKey) return data;
    
    const iv = new Uint8Array(16);
    iv[0] = seq;
    
    const ivWordArray = CryptoJS.lib.WordArray.create(iv);
    const keyWordArray = CryptoJS.enc.Hex.parse(this.aesKey);
    const ciphertext = CryptoJS.lib.WordArray.create(data);
    
    const decrypted = CryptoJS.AES.decrypt(
      { ciphertext: ciphertext },
      keyWordArray,
      {
        iv: ivWordArray,
        mode: CryptoJS.mode.CFB,
        padding: CryptoJS.pad.NoPadding,
        segmentSize: 128
      }
    );
    
    return this._wordArrayToUint8Array(decrypted, data.length);
  }

  // CRC16-CCITT
  _crc16(data) {
    let crc = 0xFFFF;
    for (let i = 0; i < data.length; i++) {
      crc ^= data[i] << 8;
      for (let j = 0; j < 8; j++) {
        if (crc & 0x8000) {
          crc = (crc << 1) ^ 0x1021;
        } else {
          crc = crc << 1;
        }
      }
    }
    return crc & 0xFFFF;
  }

  _wordArrayToUint8Array(wordArray, length) {
    const words = wordArray.words;
    const bytes = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      bytes[i] = (words[i >>> 2] >>> (24 - (i % 4) * 8)) & 0xFF;
    }
    return bytes;
  }
}
```

### 8.3 安全协商类

```javascript
// utils/blufi/security.js
import { BigInteger } from 'jsbn';
import CryptoJS from 'crypto-js';

export class BlufiSecurity {
  constructor() {
    this.p = null;
    this.g = null;
    this.privateKey = null;
    this.publicKey = null;
    this.sharedKey = null;
    this.aesKey = null;
  }

  // 解析设备返回的 DH 参数
  parseDHParams(paramData) {
    // 前 128 字节是 p，后 128 字节是 g
    const pBytes = paramData.slice(0, 128);
    const gBytes = paramData.slice(128, 256);
    
    this.p = new BigInteger(this._bytesToHex(pBytes), 16);
    this.g = new BigInteger(this._bytesToHex(gBytes), 16);
    
    console.log('DH 参数已解析, p 长度:', this.p.bitLength(), 'bits');
  }

  // 生成客户端密钥对
  generateKeyPair() {
    // 生成 128 字节随机私钥
    const privateBytes = new Uint8Array(128);
    for (let i = 0; i < 128; i++) {
      privateBytes[i] = Math.floor(Math.random() * 256);
    }
    this.privateKey = new BigInteger(this._bytesToHex(privateBytes), 16);
    
    // 计算公钥: A = g^a mod p
    this.publicKey = this.g.modPow(this.privateKey, this.p);
    
    // 转换为 128 字节数组
    return this._bigIntToBytes(this.publicKey, 128);
  }

  // 计算共享密钥
  computeSharedKey(devicePublicKeyBytes) {
    const devicePublicKey = new BigInteger(this._bytesToHex(devicePublicKeyBytes), 16);
    
    // S = B^a mod p
    this.sharedKey = devicePublicKey.modPow(this.privateKey, this.p);
    
    // MD5 哈希生成 16 字节 AES 密钥
    const sharedKeyHex = this.sharedKey.toString(16).padStart(256, '0');
    this.aesKey = CryptoJS.MD5(CryptoJS.enc.Hex.parse(sharedKeyHex)).toString();
    
    console.log('AES 密钥已生成');
    return this.aesKey;
  }

  _bytesToHex(bytes) {
    return Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join('');
  }

  _bigIntToBytes(bigInt, length) {
    let hex = bigInt.toString(16);
    if (hex.length % 2) hex = '0' + hex;
    
    const bytes = new Uint8Array(length);
    const hexBytes = hex.match(/.{2}/g) || [];
    const offset = length - hexBytes.length;
    
    for (let i = 0; i < hexBytes.length; i++) {
      bytes[offset + i] = parseInt(hexBytes[i], 16);
    }
    return bytes;
  }
}
```

### 8.4 BluFi 主类

```javascript
// utils/blufi/index.js
import { BLUFI } from './constants';
import { BlufiFrame } from './frame';
import { BlufiSecurity } from './security';

export default class BluFi {
  constructor() {
    this.deviceId = null;
    this.serviceId = BLUFI.SERVICE_UUID;
    this.frame = new BlufiFrame();
    this.security = new BlufiSecurity();
    this.connected = false;
    this.responseResolve = null;
    this.responseData = [];
  }

  // 初始化蓝牙适配器
  async init() {
    return new Promise((resolve, reject) => {
      wx.openBluetoothAdapter({
        success: () => {
          console.log('蓝牙适配器初始化成功');
          resolve();
        },
        fail: (err) => {
          console.error('蓝牙初始化失败:', err);
          reject(err);
        }
      });
    });
  }

  // 扫描 BluFi 设备
  async scan(timeout = 5000) {
    return new Promise((resolve, reject) => {
      const devices = [];
      
      wx.onBluetoothDeviceFound((res) => {
        res.devices.forEach(device => {
          if (device.name && device.name.includes('BLUFI')) {
            const exists = devices.find(d => d.deviceId === device.deviceId);
            if (!exists) {
              devices.push({
                deviceId: device.deviceId,
                name: device.name,
                RSSI: device.RSSI
              });
              console.log('发现设备:', device.name, device.RSSI);
            }
          }
        });
      });

      wx.startBluetoothDevicesDiscovery({
        services: [this.serviceId],
        success: () => {
          setTimeout(() => {
            wx.stopBluetoothDevicesDiscovery();
            resolve(devices);
          }, timeout);
        },
        fail: reject
      });
    });
  }

  // 连接设备
  async connect(deviceId) {
    this.deviceId = deviceId;
    
    // 建立连接
    await this._createConnection();
    
    // 获取服务
    await this._getServices();
    
    // 启用通知
    await this._enableNotify();
    
    this.connected = true;
    console.log('设备连接成功');
  }

  // 安全协商
  async negotiate() {
    console.log('开始安全协商...');
    
    // 1. 发送请求获取 DH 参数长度
    let frame = this.frame.build(
      BLUFI.FRAME_TYPE_DATA,
      BLUFI.DATA_SUBTYPE_NEG,
      [0x00]  // 请求参数长度
    );
    await this._write(frame);
    let response = await this._waitResponse(3000);
    
    const paramLen = (response.payload[0] << 8) | response.payload[1];
    console.log('DH 参数长度:', paramLen);
    
    // 2. 发送请求获取 DH 参数数据
    frame = this.frame.build(
      BLUFI.FRAME_TYPE_DATA,
      BLUFI.DATA_SUBTYPE_NEG,
      [0x01]  // 请求参数数据
    );
    await this._write(frame);
    
    // 收集所有 DH 参数数据（可能分片）
    const dhParams = await this._collectFragments(paramLen, 5000);
    console.log('收到 DH 参数, 长度:', dhParams.length);
    
    // 3. 解析参数并生成密钥对
    this.security.parseDHParams(dhParams);
    const clientPublicKey = this.security.generateKeyPair();
    
    // 4. 发送客户端公钥
    const pubKeyData = new Uint8Array(1 + clientPublicKey.length);
    pubKeyData[0] = 0x01;  // 类型：公钥数据
    pubKeyData.set(clientPublicKey, 1);
    
    frame = this.frame.build(
      BLUFI.FRAME_TYPE_DATA,
      BLUFI.DATA_SUBTYPE_NEG,
      pubKeyData
    );
    await this._write(frame);
    
    // 5. 接收设备公钥
    response = await this._waitResponse(5000);
    const devicePublicKey = response.payload;
    console.log('收到设备公钥, 长度:', devicePublicKey.length);
    
    // 6. 计算共享密钥
    const aesKey = this.security.computeSharedKey(devicePublicKey);
    this.frame.setAesKey(aesKey);
    
    console.log('安全协商完成!');
  }

  // WiFi 配网
  async configWifi(ssid, password) {
    console.log('开始配网, SSID:', ssid);
    
    // 发送 SSID（加密）
    const ssidBytes = new TextEncoder().encode(ssid);
    let frame = this.frame.build(
      BLUFI.FRAME_TYPE_DATA,
      BLUFI.DATA_SUBTYPE_STA_SSID,
      ssidBytes,
      true  // 加密
    );
    await this._write(frame);
    await this._delay(100);
    
    // 发送密码（加密）
    const pwdBytes = new TextEncoder().encode(password);
    frame = this.frame.build(
      BLUFI.FRAME_TYPE_DATA,
      BLUFI.DATA_SUBTYPE_STA_PASSWD,
      pwdBytes,
      true  // 加密
    );
    await this._write(frame);
    await this._delay(100);
    
    // 发送连接请求
    frame = this.frame.build(
      BLUFI.FRAME_TYPE_CTRL,
      BLUFI.CTRL_SUBTYPE_CONNECT_WIFI,
      []
    );
    await this._write(frame);
    
    // 等待连接结果（最长 20 秒）
    console.log('等待 WiFi 连接结果...');
    const result = await this._waitWifiResult(20000);
    
    return result;
  }

  // 断开连接
  async disconnect() {
    if (this.deviceId) {
      await new Promise(resolve => {
        wx.closeBLEConnection({
          deviceId: this.deviceId,
          complete: resolve
        });
      });
    }
    this.connected = false;
  }

  // ========== 私有方法 ==========

  async _createConnection() {
    return new Promise((resolve, reject) => {
      wx.createBLEConnection({
        deviceId: this.deviceId,
        timeout: 10000,
        success: resolve,
        fail: reject
      });
    });
  }

  async _getServices() {
    return new Promise((resolve, reject) => {
      setTimeout(() => {
        wx.getBLEDeviceServices({
          deviceId: this.deviceId,
          success: resolve,
          fail: reject
        });
      }, 500);
    });
  }

  async _enableNotify() {
    return new Promise((resolve, reject) => {
      // 监听特征值变化
      wx.onBLECharacteristicValueChange((res) => {
        const data = new Uint8Array(res.value);
        this._onDataReceived(data);
      });

      // 启用通知
      wx.notifyBLECharacteristicValueChange({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: BLUFI.CHAR_E2P_UUID,
        state: true,
        success: resolve,
        fail: reject
      });
    });
  }

  async _write(data) {
    // 分包发送，每包最大 20 字节
    const MTU = 20;
    for (let i = 0; i < data.length; i += MTU) {
      const chunk = data.slice(i, Math.min(i + MTU, data.length));
      await new Promise((resolve, reject) => {
        wx.writeBLECharacteristicValue({
          deviceId: this.deviceId,
          serviceId: this.serviceId,
          characteristicId: BLUFI.CHAR_P2E_UUID,
          value: chunk.buffer,
          success: resolve,
          fail: reject
        });
      });
      await this._delay(20);  // 包间延时
    }
  }

  _onDataReceived(data) {
    try {
      const parsed = this.frame.parse(data);
      console.log('收到帧:', parsed.type, parsed.subtype, parsed.payload.length);
      
      if (this.responseResolve) {
        this.responseResolve(parsed);
        this.responseResolve = null;
      }
    } catch (e) {
      console.error('解析帧失败:', e);
    }
  }

  _waitResponse(timeout = 5000) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.responseResolve = null;
        reject(new Error('响应超时'));
      }, timeout);

      this.responseResolve = (data) => {
        clearTimeout(timer);
        resolve(data);
      };
    });
  }

  async _collectFragments(expectedLen, timeout) {
    const fragments = [];
    let totalLen = 0;
    const startTime = Date.now();
    
    while (totalLen < expectedLen && (Date.now() - startTime) < timeout) {
      const response = await this._waitResponse(3000);
      fragments.push(...response.payload);
      totalLen = fragments.length;
    }
    
    return new Uint8Array(fragments);
  }

  async _waitWifiResult(timeout) {
    const startTime = Date.now();
    
    while ((Date.now() - startTime) < timeout) {
      try {
        const response = await this._waitResponse(2000);
        
        if (response.subtype === BLUFI.DATA_SUBTYPE_WIFI_CONN_REPORT) {
          const status = response.payload[1];
          
          if (status === BLUFI.WIFI_CONN_SUCCESS) {
            return { success: true, message: 'WiFi 连接成功' };
          } else if (status === BLUFI.WIFI_CONN_FAIL) {
            return { success: false, message: 'WiFi 连接失败' };
          }
          // WIFI_CONNECTING 继续等待
        }
      } catch (e) {
        // 超时继续等待
      }
    }
    
    return { success: false, message: '连接超时' };
  }

  _delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}
```

### 8.5 页面使用示例

```javascript
// pages/blufi/index.js
import BluFi from '../../utils/blufi/index';

Page({
  data: {
    devices: [],
    scanning: false,
    connecting: false,
    ssid: '',
    password: '',
    status: ''
  },

  blufi: null,

  onLoad() {
    this.blufi = new BluFi();
  },

  // 扫描设备
  async onScan() {
    this.setData({ scanning: true, status: '正在扫描...' });
    
    try {
      await this.blufi.init();
      const devices = await this.blufi.scan(5000);
      this.setData({ devices, status: `发现 ${devices.length} 个设备` });
    } catch (e) {
      this.setData({ status: '扫描失败: ' + e.message });
    } finally {
      this.setData({ scanning: false });
    }
  },

  // 连接并配网
  async onConnect(e) {
    const deviceId = e.currentTarget.dataset.id;
    const { ssid, password } = this.data;
    
    if (!ssid) {
      wx.showToast({ title: '请输入 WiFi 名称', icon: 'none' });
      return;
    }
    
    this.setData({ connecting: true, status: '正在连接设备...' });
    
    try {
      // 1. 连接设备
      await this.blufi.connect(deviceId);
      this.setData({ status: '正在安全协商...' });
      
      // 2. 安全协商
      await this.blufi.negotiate();
      this.setData({ status: '正在配网...' });
      
      // 3. 发送 WiFi 配置
      const result = await this.blufi.configWifi(ssid, password);
      
      if (result.success) {
        this.setData({ status: '配网成功！设备即将重启' });
        wx.showToast({ title: '配网成功', icon: 'success' });
      } else {
        this.setData({ status: '配网失败: ' + result.message });
        wx.showToast({ title: result.message, icon: 'error' });
      }
    } catch (e) {
      this.setData({ status: '错误: ' + e.message });
      wx.showToast({ title: e.message, icon: 'error' });
    } finally {
      this.setData({ connecting: false });
      await this.blufi.disconnect();
    }
  },

  // 输入处理
  onSsidInput(e) {
    this.setData({ ssid: e.detail.value });
  },

  onPasswordInput(e) {
    this.setData({ password: e.detail.value });
  }
});
```

### 8.6 页面模板

```xml
<!-- pages/blufi/index.wxml -->
<view class="container">
  <view class="section">
    <text class="title">WiFi 配置</text>
    <input placeholder="WiFi 名称" bindinput="onSsidInput" value="{{ssid}}" />
    <input placeholder="WiFi 密码" password bindinput="onPasswordInput" value="{{password}}" />
  </view>

  <view class="section">
    <button type="primary" loading="{{scanning}}" bindtap="onScan">
      {{scanning ? '扫描中...' : '扫描设备'}}
    </button>
  </view>

  <view class="status">{{status}}</view>

  <view class="devices">
    <view class="device" wx:for="{{devices}}" wx:key="deviceId" 
          data-id="{{item.deviceId}}" bindtap="onConnect">
      <text class="name">{{item.name}}</text>
      <text class="rssi">信号: {{item.RSSI}} dBm</text>
      <button size="mini" loading="{{connecting}}" disabled="{{connecting}}">
        {{connecting ? '配网中...' : '配网'}}
      </button>
    </view>
  </view>
</view>
```

---

## 9. 调试建议

### 9.1 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 扫描不到设备 | 设备未进入配网模式 | 确认设备处于配网状态 |
| 连接失败 | 距离太远/干扰 | 靠近设备重试 |
| 安全协商失败 | DH 参数解析错误 | 检查大数运算库 |
| 配网超时 | WiFi 密码错误 | 确认密码正确 |
| 配网后设备不重启 | 固件问题 | 检查固件日志 |

### 9.2 调试日志

在小程序端开启详细日志：

```javascript
// 在 BluFi 类中添加
_log(level, ...args) {
  const timestamp = new Date().toISOString();
  console[level](`[BluFi ${timestamp}]`, ...args);
}
```

### 9.3 固件端日志

通过串口监控固件日志，关键 TAG：`BLUFI_CLASS`

```
I (1234) BLUFI_CLASS: BLUFI init finish
I (2345) BLUFI_CLASS: BLUFI ble connect
I (3456) BLUFI_CLASS: Recv STA SSID: MyWiFi
I (3567) BLUFI_CLASS: Recv STA PASSWORD
I (3678) BLUFI_CLASS: BLUFI request wifi connect to AP
I (5000) BLUFI_CLASS: BluFi: connected to WiFi, restarting device...
```

### 9.4 测试工具

推荐使用乐鑫官方 EspBluFi APP 进行对比测试：
- Android: [EspBluFi](https://github.com/EspressifApp/EspBlufiForAndroid)
- iOS: [EspBluFi](https://github.com/EspressifApp/EspBlufiForiOS)

---

## 10. 联系方式

如有技术问题，请联系固件开发团队。

---

*文档结束*
