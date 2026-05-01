# Galeros OpenIris - 自定义配网 + Web 日志版

[![PlatformIO](https://img.shields.io/badge/PlatformIO-3.0.0-blue?style=for-the-badge&logo=platformio)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-Arduino-ff69b4?style=for-the-badge&logo=espressif)](https://www.espressif.com/)


一个基于 OpenIris 修改的 ESP32 固件，专注于**网页配网**和**实时 Web 日志监控**，让调试和使用更加便捷。

**项目目标**：让 ESP32 设备开箱即用，通过浏览器即可完成 WiFi 配置，并实时监控设备运行日志。

![](https://www.galeros.xyz/images/ap-config-page.png)  

---

## 主要功能

- **智能网页配网系统**
  - 开机自动尝试连接已保存 WiFi
  - 连接失败或首次使用时自动开启 AP 热点（`Galeros_ESP32`）
  - 支持 Captive Portal 自动弹出配网页面

- **实时 Web 日志监控**
  - 独立日志服务器（端口 1234）
  - 支持 `Serial.printf`、`log_d`、`log_i` 等日志实时显示
  - 支持自定义日志发送 `sendLogToWeb()`

- **配置管理页面**
  - 已连接 WiFi 后可通过 `http://设备IP:8080` 修改网络设置

- **优化体验**
  - 美化后的配网和管理界面
  - 更好的错误处理和日志系统
  - 支持 OTA 更新

---

## 使用方法

### 1. 开发环境准备

- 安装 **Visual Studio Code**
- 在 VS Code 中安装 **PlatformIO** 插件
- 克隆本仓库并用 VS Code 打开项目

### 2. 编译与烧录

1. 在 PlatformIO 左侧栏点击 **Build**（或按 `Ctrl+Alt+B`）
2. 编译成功后点击 **Upload**（或按 `Ctrl+Alt+U`）烧录到开发板
3. 烧录完成后设备会自动重启

### 3. 首次配网流程

1. 设备上电后，若未保存 WiFi，会自动开启热点 `Galeros_ESP32`
2. 用手机或电脑连接该热点（密码：`Galeros_ESP32`）
3. 浏览器会自动弹出配网页面（或手动访问 `192.168.4.1`）
4. 输入你的家用 WiFi 名称和密码
5. 点击“保存并重启”
6. 设备重启后会自动尝试连接你设置的 WiFi

### 4. 查看实时日志

设备连上 WiFi 后，在浏览器输入：
```
http://设备IP:1234
```
即可实时查看系统运行日志。

### 5. 配置管理页面

已连接 WiFi 后访问：
```
http://设备IP:8080
```
可在此页面修改 WiFi 设置。

---

## 详细使用教程

📖 **完整详细教程请访问：**  
**[Galeros OpenIris 详细使用教程](https://www.galeros.xyz/2026/05/01/Galeros-OpenIris-ESP32-固件开发实战/)**

教程包含：
- 详细配网步骤图文说明
- 日志系统使用方法
- 自定义日志发送示例
- 常见问题排查
- 高级配置说明

---

## 注意事项

- 首次使用必须通过 AP 配网方式配置 WiFi
- 修改 WiFi 后设备会自动重启，请耐心等待
- 配置管理页面端口：**8080**
- Web 日志监控页面端口：**1234**
- AP 热点名称和密码可在 `main.cpp` 顶部修改
- 目前日志回调功能仍在优化中，推荐使用 `sendLogToWeb()` 发送自定义调试日志

---

## 待办事项（TODO）

- 完善日志回调系统（解决 `Serial.available()` 一直为 0 的问题）
- 添加日志级别颜色区分（DEBUG / INFO / ERROR）
- 支持网页端扫描附近 WiFi 列表
- OTA 更新时暂停日志推送，避免冲突
- 添加暗黑模式支持
- 优化内存占用

---

## 项目说明

- 基于 OpenIris 项目二次开发
- 主要修改文件：`main.cpp`、`SerialManager.cpp`、`html_content.h`、`streamServer.cpp`
- 配置信息保存在 NVS 中，重启后依然有效

---

**欢迎 Star 本项目！**

如有问题或建议，欢迎在 Issues 中提出。

