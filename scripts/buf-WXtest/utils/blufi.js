const FRAME_CTRL_ENCRYPTED = 0x01;
const FRAME_CTRL_FRAGMENT = 0x10;

const DH_P = 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74' +
             '020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437' +
             '4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED' +
             'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF';

// AES S-Box
const SBOX = [
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
];

// 大整数模幂运算
function modPow(base, exponent, modulus) {
  let result = 1n;
  base = base % modulus;

  while (exponent > 0n) {
    if (exponent % 2n === 1n) {
      result = (result * base) % modulus;
    }
    exponent = exponent / 2n;
    base = (base * base) % modulus;
  }

  return result;
}

function hexToBytes(hexString) {
  const hexPairs = hexString.match(/.{2}/g);
  return new Uint8Array(hexPairs.map(pair => parseInt(pair, 16)));
}

function bytesToBigInt(bytes) {
  const hexString = Array.from(bytes)
    .map(byte => byte.toString(16).padStart(2, '0'))
    .join('');
  return BigInt('0x' + hexString || '0');
}

function bigIntToBytes(number, length) {
  let hexString = number.toString(16);
  if (hexString.length % 2) {
    hexString = '0' + hexString;
  }

  const bytes = new Uint8Array(length);
  const hexPairs = hexString.match(/.{2}/g) || [];
  const offset = length - hexPairs.length;

  for (let i = 0; i < hexPairs.length && offset + i < length; i++) {
    bytes[offset + i] = parseInt(hexPairs[i], 16);
  }

  return bytes;
}

// MD5
function md5(bytes) {
  const SHIFT_AMOUNTS = [
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
  ];

  const CONSTANTS = [
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
  ];

  const add = (x, y) => ((x >>> 0) + (y >>> 0)) >>> 0;
  const rotate = (x, n) => ((x << n) | (x >>> (32 - n))) >>> 0;

  const messageLength = bytes.length;
  const paddingLength = ((56 - (messageLength + 1) % 64) + 64) % 64;
  const padded = new Uint8Array(messageLength + 1 + paddingLength + 8);

  padded.set(bytes);
  padded[messageLength] = 0x80;
  new DataView(padded.buffer).setUint32(padded.length - 8, (messageLength * 8) >>> 0, true);

  let hash0 = 0x67452301;
  let hash1 = 0xefcdab89;
  let hash2 = 0x98badcfe;
  let hash3 = 0x10325476;

  for (let chunkStart = 0; chunkStart < padded.length; chunkStart += 64) {
    const words = new Uint32Array(16);
    for (let wordIndex = 0; wordIndex < 16; wordIndex++) {
      words[wordIndex] = new DataView(padded.buffer).getUint32(chunkStart + wordIndex * 4, true);
    }

    let a = hash0;
    let b = hash1;
    let c = hash2;
    let d = hash3;

    for (let round = 0; round < 64; round++) {
      let f, wordIdx;

      if (round < 16) {
        f = (b & c) | ((~b >>> 0) & d);
        wordIdx = round;
      } else if (round < 32) {
        f = (d & b) | ((~d >>> 0) & c);
        wordIdx = (5 * round + 1) % 16;
      } else if (round < 48) {
        f = b ^ c ^ d;
        wordIdx = (3 * round + 5) % 16;
      } else {
        f = c ^ (b | (~d >>> 0));
        wordIdx = (7 * round) % 16;
      }

      f = add(add(add(f >>> 0, a), CONSTANTS[round]), words[wordIdx]);
      a = d;
      d = c;
      c = b;
      b = add(b, rotate(f, SHIFT_AMOUNTS[round]));
    }

    hash0 = add(hash0, a);
    hash1 = add(hash1, b);
    hash2 = add(hash2, c);
    hash3 = add(hash3, d);
  }

  const result = new Uint8Array(16);
  const view = new DataView(result.buffer);
  view.setUint32(0, hash0, true);
  view.setUint32(4, hash1, true);
  view.setUint32(8, hash2, true);
  view.setUint32(12, hash3, true);

  return Array.from(result)
    .map(byte => byte.toString(16).padStart(2, '0'))
    .join('');
}

// AES-128-CFB
class AesCfb {
  constructor(key) {
    this.key = typeof key === 'string' ? hexToBytes(key) : new Uint8Array(key);

    const KEY_WORDS = 4;
    const BLOCK_WORDS = 4;
    const ROUNDS = 10;
    const RCON = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36];

    this.roundKeys = new Uint32Array(BLOCK_WORDS * (ROUNDS + 1));

    for (let i = 0; i < KEY_WORDS; i++) {
      this.roundKeys[i] = (this.key[4 * i] << 24) |
                          (this.key[4 * i + 1] << 16) |
                          (this.key[4 * i + 2] << 8) |
                          this.key[4 * i + 3];
    }

    for (let i = KEY_WORDS; i < BLOCK_WORDS * (ROUNDS + 1); i++) {
      let temp = this.roundKeys[i - 1];

      if (i % KEY_WORDS === 0) {
        temp = ((temp << 8) | (temp >>> 24)) >>> 0;
        temp = (SBOX[(temp >>> 24) & 0xff] << 24) |
               (SBOX[(temp >>> 16) & 0xff] << 16) |
               (SBOX[(temp >>> 8) & 0xff] << 8) |
               SBOX[temp & 0xff];
        temp ^= RCON[i / KEY_WORDS - 1] << 24;
      }

      this.roundKeys[i] = (this.roundKeys[i - KEY_WORDS] ^ temp) >>> 0;
    }
  }

  _encryptBlock(block) {
    const state = new Uint8Array(block);
    const xtime = x => ((x << 1) ^ (((x >> 7) & 1) * 0x1b)) & 0xff;

    for (let col = 0; col < 4; col++) {
      const idx = col * 4;
      state[idx] ^= (this.roundKeys[col] >>> 24) & 0xff;
      state[idx + 1] ^= (this.roundKeys[col] >>> 16) & 0xff;
      state[idx + 2] ^= (this.roundKeys[col] >>> 8) & 0xff;
      state[idx + 3] ^= this.roundKeys[col] & 0xff;
    }

    for (let round = 1; round <= 10; round++) {
      for (let i = 0; i < 16; i++) {
        state[i] = SBOX[state[i]];
      }

      let temp = state[1];
      state[1] = state[5];
      state[5] = state[9];
      state[9] = state[13];
      state[13] = temp;

      temp = state[2];
      state[2] = state[10];
      state[10] = temp;
      temp = state[6];
      state[6] = state[14];
      state[14] = temp;

      temp = state[15];
      state[15] = state[11];
      state[11] = state[7];
      state[7] = state[3];
      state[3] = temp;

      if (round < 10) {
        for (let col = 0; col < 4; col++) {
          const idx = col * 4;
          const s0 = state[idx];
          const s1 = state[idx + 1];
          const s2 = state[idx + 2];
          const s3 = state[idx + 3];

          state[idx] = xtime(s0) ^ xtime(s1) ^ s1 ^ s2 ^ s3;
          state[idx + 1] = s0 ^ xtime(s1) ^ xtime(s2) ^ s2 ^ s3;
          state[idx + 2] = s0 ^ s1 ^ xtime(s2) ^ xtime(s3) ^ s3;
          state[idx + 3] = xtime(s0) ^ s0 ^ s1 ^ s2 ^ xtime(s3);
        }
      }

      for (let col = 0; col < 4; col++) {
        const idx = col * 4;
        const roundKey = this.roundKeys[round * 4 + col];
        state[idx] ^= (roundKey >>> 24) & 0xff;
        state[idx + 1] ^= (roundKey >>> 16) & 0xff;
        state[idx + 2] ^= (roundKey >>> 8) & 0xff;
        state[idx + 3] ^= roundKey & 0xff;
      }
    }

    return state;
  }

  encrypt(data, iv) {
    const result = new Uint8Array(data.length);
    let feedback = new Uint8Array(iv);

    for (let offset = 0; offset < data.length; offset += 16) {
      const encrypted = this._encryptBlock(feedback);
      const chunkSize = Math.min(16, data.length - offset);

      for (let j = 0; j < chunkSize; j++) {
        result[offset + j] = data[offset + j] ^ encrypted[j];
      }

      feedback = result.slice(offset, offset + 16);
      if (feedback.length < 16) {
        const padded = new Uint8Array(16);
        padded.set(feedback);
        feedback = padded;
      }
    }

    return result;
  }
}

// BluFi 主类
export default class BluFi {
  constructor() {
    this.deviceId = null;
    this.serviceId = null;
    this.charP2E = null;
    this.charE2P = null;
    this.sequenceNumber = 0;
    this.aes = null;
    this.pendingResolve = null;
    this.dhPrime = BigInt('0x' + DH_P);
    this.dhGenerator = 2n;
    this.onStatus = null;
  }

  _log(...args) {
    console.log('[BluFi]', ...args);
  }

  _status(message) {
    this._log(message);
    if (this.onStatus) {
      this.onStatus(message);
    }
  }

  _delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  async init() {
    return new Promise((resolve, reject) => {
      wx.openBluetoothAdapter({
        success: resolve,
        fail: error => reject(new Error(error.errMsg))
      });
    });
  }

  async scan(timeout = 5000) {
    return new Promise((resolve, reject) => {
      const devices = [];

      wx.onBluetoothDeviceFound(result => {
        result.devices.forEach(device => {
          const displayName = device.localName || device.name || '';
          const isTargetDevice = displayName.includes('BLUFI') || displayName.includes('Voxia');
          const isNewDevice = !devices.find(d => d.deviceId === device.deviceId);

          if (isTargetDevice && isNewDevice) {
            devices.push({
              deviceId: device.deviceId,
              name: displayName,
              RSSI: device.RSSI
            });
          }
        });
      });

      wx.startBluetoothDevicesDiscovery({
        allowDuplicatesKey: false,
        success: () => {
          setTimeout(() => {
            wx.stopBluetoothDevicesDiscovery();
            resolve(devices);
          }, timeout);
        },
        fail: error => reject(new Error(error.errMsg))
      });
    });
  }

  async connect(deviceId) {
    this.deviceId = deviceId;
    this.sequenceNumber = 0;
    this._status('连接中...');

    await new Promise((resolve, reject) => {
      wx.createBLEConnection({
        deviceId,
        timeout: 10000,
        success: resolve,
        fail: error => reject(new Error(error.errMsg))
      });
    });

    await new Promise(resolve => {
      wx.setBLEMTU({ deviceId, mtu: 512, complete: resolve });
    });

    await this._delay(500);

    const servicesResult = await new Promise((resolve, reject) => {
      wx.getBLEDeviceServices({ deviceId, success: resolve, fail: reject });
    });

    this.serviceId = servicesResult.services.find(
      service => service.uuid.toLowerCase().includes('ffff')
    )?.uuid;

    const charsResult = await new Promise((resolve, reject) => {
      wx.getBLEDeviceCharacteristics({
        deviceId,
        serviceId: this.serviceId,
        success: resolve,
        fail: reject
      });
    });

    charsResult.characteristics.forEach(char => {
      const uuid = char.uuid.toLowerCase();
      if (uuid.includes('ff01')) {
        this.charP2E = char.uuid;
      }
      if (uuid.includes('ff02')) {
        this.charE2P = char.uuid;
      }
    });

    wx.onBLECharacteristicValueChange(result => {
      this._onData(new Uint8Array(result.value));
    });

    await new Promise((resolve, reject) => {
      wx.notifyBLECharacteristicValueChange({
        deviceId,
        serviceId: this.serviceId,
        characteristicId: this.charE2P,
        state: true,
        success: resolve,
        fail: reject
      });
    });

    await this._delay(2000);
    this._status('已连接');
  }

  async negotiate() {
    this._status('安全协商...');

    // 生成私钥和公钥
    const privateKeyBytes = new Uint8Array(128);
    for (let i = 0; i < 128; i++) {
      privateKeyBytes[i] = Math.floor(Math.random() * 256);
    }

    const privateKey = bytesToBigInt(privateKeyBytes);
    const publicKey = modPow(this.dhGenerator, privateKey, this.dhPrime);
    const publicKeyBytes = bigIntToBytes(publicKey, 128);
    const primeBytes = bigIntToBytes(this.dhPrime, 128);

    // mbedtls_dhm_read_params 期望格式: [P_len(2), P, G_len(2), G, PubKey_len(2), PubKey]
    // P_len=128, G_len=1, PubKey_len=128
    const totalLength = 2 + 128 + 2 + 1 + 2 + 128; // = 263

    // 发送长度帧
    await this._writeFrame(1, 0, [0x00, (totalLength >> 8) & 0xFF, totalLength & 0xFF]);

    // 构建DH参数数据
    const dhParamData = new Uint8Array(1 + totalLength);
    dhParamData[0] = 0x01; // DH_PARAM_DATA type

    let offset = 1;

    // P长度 (big-endian)
    dhParamData[offset++] = 0;
    dhParamData[offset++] = 128;

    // P值
    dhParamData.set(primeBytes, offset);
    offset += 128;

    // G长度 (big-endian)
    dhParamData[offset++] = 0;
    dhParamData[offset++] = 1;

    // G值
    dhParamData[offset++] = 2;

    // 公钥长度 (big-endian)
    dhParamData[offset++] = 0;
    dhParamData[offset++] = 128;

    // 公钥值
    dhParamData.set(publicKeyBytes, offset);

    const responsePromise = this._waitResponse(10000);
    await this._writeFragmented(dhParamData);
    const response = await responsePromise;

    const sharedSecret = modPow(bytesToBigInt(response.payload), privateKey, this.dhPrime);
    this.aes = new AesCfb(md5(bigIntToBytes(sharedSecret, 128)));
    this._status('协商完成');
  }

  async configWifi(ssid, password) {
    this._status('配网中...');

    const encodeUtf8 = str => {
      const bytes = [];
      for (let i = 0; i < str.length; i++) {
        const charCode = str.charCodeAt(i);
        if (charCode < 0x80) {
          bytes.push(charCode);
        } else if (charCode < 0x800) {
          bytes.push(0xC0 | (charCode >> 6), 0x80 | (charCode & 0x3F));
        } else {
          bytes.push(
            0xE0 | (charCode >> 12),
            0x80 | ((charCode >> 6) & 0x3F),
            0x80 | (charCode & 0x3F)
          );
        }
      }
      return new Uint8Array(bytes);
    };

    await this._writeFrame(1, 2, encodeUtf8(ssid), true);
    await this._delay(100);
    await this._writeFrame(1, 3, encodeUtf8(password), true);
    await this._delay(100);
    await this._writeFrame(0, 3, []);

    this._status('等待结果...');
    const startTime = Date.now();

    while (Date.now() - startTime < 15000) {
      try {
        const response = await this._waitResponse(2000);
        if (response.subtype === 0x0F && response.payload[1] === 0) {
          return { success: true };
        }
      } catch {
        // 超时继续等待
      }
    }

    return { success: false };
  }

  async disconnect() {
    if (this.deviceId) {
      await new Promise(resolve => {
        wx.closeBLEConnection({ deviceId: this.deviceId, complete: resolve });
      });
    }
  }

  // 请求设备扫描WiFi列表
  async getWifiList(timeout = 10000) {
    this._status('扫描WiFi中...');

    // 发送获取WiFi列表请求: type=0(控制帧), subtype=9(GET_WIFI_LIST)
    await this._writeFrame(0, 9, []);

    // 等待WiFi列表响应
    const wifiList = [];
    const startTime = Date.now();

    while (Date.now() - startTime < timeout) {
      try {
        const response = await this._waitResponse(5000);

        // WiFi列表响应: type=1(数据帧), subtype=0x11 (WIFI_LIST)
        if (response.type === 1 && response.subtype === 0x11) {
          // 解析WiFi列表数据
          // ESP-IDF格式: [len(1), rssi(1), ssid(len-1)] 重复
          let offset = 0;
          const payload = response.payload;

          while (offset < payload.length) {
            const entryLength = payload[offset]; // len = ssid_len + 1 (rssi)
            if (entryLength <= 1 || offset + 1 + entryLength > payload.length) {
              break;
            }

            const rssi = payload[offset + 1] > 127
              ? payload[offset + 1] - 256
              : payload[offset + 1];

            const ssidLength = entryLength - 1;
            const ssidBytes = payload.slice(offset + 2, offset + 2 + ssidLength);
            const ssid = this._decodeUtf8(ssidBytes);

            if (ssid && !wifiList.find(w => w.SSID === ssid)) {
              wifiList.push({ SSID: ssid, signalStrength: rssi });
            }

            offset += 1 + entryLength;
          }

          this._log('收到WiFi列表:', wifiList.length, '个');
          break;
        }
      } catch {
        // 超时继续等待
      }
    }

    // 按信号强度排序
    wifiList.sort((a, b) => b.signalStrength - a.signalStrength);
    this._status(`发现 ${wifiList.length} 个WiFi`);
    return wifiList;
  }

  // UTF-8解码
  _decodeUtf8(bytes) {
    try {
      let result = '';
      let i = 0;

      while (i < bytes.length) {
        const byte = bytes[i];

        if (byte < 0x80) {
          result += String.fromCharCode(byte);
          i++;
        } else if ((byte & 0xE0) === 0xC0) {
          result += String.fromCharCode(
            ((byte & 0x1F) << 6) | (bytes[i + 1] & 0x3F)
          );
          i += 2;
        } else if ((byte & 0xF0) === 0xE0) {
          result += String.fromCharCode(
            ((byte & 0x0F) << 12) |
            ((bytes[i + 1] & 0x3F) << 6) |
            (bytes[i + 2] & 0x3F)
          );
          i += 3;
        } else {
          i++;
        }
      }

      return result;
    } catch {
      return '';
    }
  }

  async _writeFrame(type, subtype, data, encrypted = false) {
    let payload = data instanceof Uint8Array ? data : new Uint8Array(data);
    let controlFlags = 0;

    if (encrypted && this.aes) {
      controlFlags |= FRAME_CTRL_ENCRYPTED;
      const iv = new Uint8Array(16);
      iv[0] = this.sequenceNumber;
      payload = this.aes.encrypt(payload, iv);
    }

    const frameHeader = (subtype << 2) | (type & 0x03);
    const frame = new Uint8Array([
      frameHeader,
      controlFlags,
      this.sequenceNumber++,
      payload.length,
      ...payload
    ]);

    const buffer = new ArrayBuffer(frame.length);
    new Uint8Array(buffer).set(frame);

    await new Promise((resolve, reject) => {
      wx.writeBLECharacteristicValue({
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.charP2E,
        value: buffer,
        success: resolve,
        fail: reject
      });
    });
  }

  async _writeFragmented(data) {
    let offset = 0;

    while (offset < data.length) {
      const isFirstChunk = offset === 0;
      const maxChunkSize = isFirstChunk ? 14 : 16;
      const chunkSize = Math.min(maxChunkSize, data.length - offset);
      const isLastChunk = offset + chunkSize >= data.length;

      let payload;
      let controlFlags = isLastChunk ? 0 : FRAME_CTRL_FRAGMENT;

      if (!isLastChunk) {
        payload = new Uint8Array(2 + chunkSize);
        payload[0] = data.length & 0xFF;
        payload[1] = (data.length >> 8) & 0xFF;
        payload.set(data.slice(offset, offset + chunkSize), 2);
      } else {
        payload = data.slice(offset, offset + chunkSize);
      }

      const frame = new Uint8Array([
        0x01,
        controlFlags,
        this.sequenceNumber++,
        payload.length,
        ...payload
      ]);

      const buffer = new ArrayBuffer(frame.length);
      new Uint8Array(buffer).set(frame);

      await new Promise((resolve, reject) => {
        wx.writeBLECharacteristicValue({
          deviceId: this.deviceId,
          serviceId: this.serviceId,
          characteristicId: this.charP2E,
          value: buffer,
          success: resolve,
          fail: reject
        });
      });

      await this._delay(100);
      offset += chunkSize;
    }
  }

  _onData(data) {
    try {
      const response = {
        type: data[0] & 0x03,
        subtype: (data[0] >> 2) & 0x3F,
        payload: data.slice(4, 4 + data[3])
      };

      if (this.pendingResolve) {
        this.pendingResolve(response);
        this.pendingResolve = null;
      }
    } catch {
      // 忽略解析错误
    }
  }

  _waitResponse(timeout) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingResolve = null;
        reject(new Error('timeout'));
      }, timeout);

      this.pendingResolve = response => {
        clearTimeout(timer);
        resolve(response);
      };
    });
  }
}
