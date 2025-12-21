import BluFi from '../../utils/blufi';

Page({
  data: {
    statusBarHeight: 20,
    devices: [],
    scanning: false,
    connecting: false,
    ssid: '',
    password: '',
    status: '',
    selectedDevice: null,
    showWifiModal: false,
    wifiList: [],
    wifiScanning: false,
    showWifiPicker: false,
    isIOS: false
  },

  blufi: null,

  onLoad() {
    const sysInfo = wx.getSystemInfoSync();
    this.setData({
      statusBarHeight: sysInfo.statusBarHeight,
      isIOS: sysInfo.platform === 'ios'
    });

    this.blufi = new BluFi();
    this.blufi.onStatusChange = (status) => {
      this.setData({ status });
    };
  },

  onUnload() {
    if (this.blufi) {
      this.blufi.disconnect();
    }
  },

  // 扫描设备
  async onScan() {
    if (this.data.scanning) return;
    
    this.setData({ 
      scanning: true, 
      status: '正在初始化蓝牙...', 
      devices: [] 
    });
    
    try {
      await this.blufi.init();
      this.setData({ status: '正在扫描设备...' });
      
      const devices = await this.blufi.scan(5000);
      this.setData({ 
        devices, 
        status: devices.length > 0 
          ? `发现 ${devices.length} 个设备` 
          : '未发现设备，请确认设备已进入配网模式'
      });
    } catch (e) {
      console.error('扫描失败:', e);
      let errMsg = e.message || '扫描失败';
      
      // 处理常见错误
      if (errMsg.includes('not available') || errMsg.includes('not turned on')) {
        errMsg = '请打开手机蓝牙';
      } else if (errMsg.includes('不支持') || errMsg.includes('Mac')) {
        errMsg = '请使用真机调试（开发工具仅 Mac 支持蓝牙）';
      } else if (errMsg.includes('authorize')) {
        errMsg = '请授权蓝牙权限';
      }
      
      this.setData({ status: errMsg });
      wx.showToast({ title: errMsg, icon: 'none', duration: 3000 });
    } finally {
      this.setData({ scanning: false });
    }
  },

  // 选择设备 - 先连接再显示WiFi配置
  async onSelectDevice(e) {
    const deviceId = e.currentTarget.dataset.id;
    const device = this.data.devices.find(d => d.deviceId === deviceId);
    this.setData({ 
      selectedDevice: device,
      connecting: true,
      status: '正在连接设备...'
    });
    
    try {
      // 先连接设备并完成安全协商
      await this.blufi.connect(deviceId);
      await this.blufi.negotiate();
      
      this.setData({ 
        connecting: false,
        showWifiModal: true,
        status: '已连接，请配置WiFi'
      });
    } catch (e) {
      console.error('连接失败:', e);
      this.setData({ 
        connecting: false,
        status: '连接失败: ' + e.message
      });
      wx.showToast({ title: '连接失败', icon: 'none' });
      await this.blufi.disconnect();
    }
  },

  // 关闭 WiFi 配置弹窗
  onCloseModal() {
    this.setData({ showWifiModal: false });
  },

  // 阻止点击穿透
  preventTap() {},

  // 发送WiFi配置（设备已在onSelectDevice中连接）
  async onConnect() {
    const { ssid, password } = this.data;
    
    if (!ssid) {
      wx.showToast({ title: '请输入 WiFi 名称', icon: 'none' });
      return;
    }
    
    this.setData({ 
      connecting: true, 
      showWifiModal: false,
      status: '正在配网...' 
    });
    
    try {
      // 发送 WiFi 配置（连接和协商已在选择设备时完成）
      const result = await this.blufi.configWifi(ssid, password);
      
      if (result.success) {
        this.setData({ status: '配网成功！设备即将重启连接 WiFi' });
        wx.showToast({ title: '配网成功', icon: 'success' });
      } else {
        this.setData({ status: '配网失败' });
        wx.showToast({ title: '配网失败', icon: 'none' });
      }
    } catch (e) {
      console.error('配网错误:', e);
      this.setData({ status: '错误: ' + e.message });
      wx.showToast({ title: e.message, icon: 'none' });
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
  },

  // 通过设备扫描周围WiFi
  async scanWifi() {
    // 检查是否已连接设备
    if (!this.blufi || !this.data.selectedDevice) {
      wx.showToast({ title: '请先连接设备', icon: 'none' });
      return;
    }

    this.setData({ wifiScanning: true, wifiList: [] });
    
    try {
      // 通过蓝牙请求设备扫描WiFi
      const wifiList = await this.blufi.getWifiList(10000);
      
      this.setData({ 
        wifiList,
        wifiScanning: false,
        showWifiPicker: wifiList.length > 0
      });
      
      if (wifiList.length === 0) {
        wx.showToast({ title: '未扫描到WiFi', icon: 'none' });
      }
    } catch (err) {
      console.error('WiFi扫描失败:', err);
      this.setData({ wifiScanning: false });
      wx.showToast({ title: '扫描失败: ' + err.message, icon: 'none' });
    }
  },

  // 选择WiFi
  onSelectWifi(e) {
    const wifi = e.currentTarget.dataset.wifi;
    this.setData({ 
      ssid: wifi.SSID,
      showWifiPicker: false
    });
  },

  // 关闭WiFi选择器
  onCloseWifiPicker() {
    this.setData({ showWifiPicker: false });
  },

  // 获取信号强度图标
  getSignalIcon(strength) {
    if (strength >= -50) return '📶';
    if (strength >= -70) return '📶';
    return '📶';
  }
});
