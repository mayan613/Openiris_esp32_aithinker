# CLAUDE.md — Galeros OpenIris ESP32

> 基于 [OpenIris](https://github.com/lorow/OpenIris) 二次开发的 ESP32 摄像头固件，专注**网页配网** + **实时 Web 日志监控**。
> 由彩咖工作室 (Galeros Studio) 维护。

---

## 项目概览

- **目标硬件**: ESP32-CAM (AI Thinker) 为主，支持多种 ESP32/S3 开发板
- **框架**: Arduino (C++17) + PlatformIO
- **默认编译环境**: `esp32AIThinker` (debug)
- **核心功能**: 摄像头视频流 + 网页配网 + 实时日志推送
- **上游**: 原版 OpenIris 是眼球追踪固件（USB/串口传输摄像头帧），本项目剥离了眼球追踪部分，替换为 Web 配网和日志系统

---

## 编译与烧录

```bash
# 编译 (默认 esp32AIThinker 环境)
pio run

# 编译并烧录
pio run -t upload

# 指定其他环境
pio run -e esp32Cam -t upload
pio run -e xiaosenses3 -t upload

# 串口监视
pio device monitor
```

编译脚本 (`tools/`):
- `pre:customname.py` — 自定义固件命名
- `post:createzip.py` — 打包固件

---

## 项目结构

```
├── src/
│   ├── main.cpp              # ★ 主入口 + 自定义配网/日志逻辑（核心修改文件）
│   └── html_content.h        # 配网页面的 HTML/CSS（AP配网页+STA管理页+Web日志页）
├── lib/src/                   # 原 OpenIris 库代码
│   ├── openiris.hpp           # 总头文件
│   ├── data/
│   │   ├── config/project_config.hpp/.cpp  # NVS 持久化配置（WiFi/Camera/OTA/mDNS）
│   │   ├── StateManager/StateManager.hpp   # 全局状态机（WiFi/Camera/WebServer等）
│   │   ├── CommandManager/                 # JSON 命令解析（串口/API通用）
│   │   └── utilities/                      # Observer模式、网络工具、STL兼容
│   ├── io/
│   │   ├── Serial/SerialManager.hpp/.cpp   # ★ 串口命令+日志转发（有bug）
│   │   ├── camera/cameraHandler.hpp/.cpp   # 摄像头初始化和配置
│   │   └── LEDManager/                     # LED 状态指示
│   └── network/
│       ├── wifihandler/                    # WiFi 连接/ADHOC 模式管理
│       ├── api/webserverHandler.hpp/.cpp   # REST API (端口81) — 路由注册+OTA
│       ├── api/baseAPI/                    # API 基类（认证、OTA、命令处理）
│       ├── stream/streamServer.hpp/.cpp    # ★ MJPEG 视频流 (端口80) — 集成日志
│       └── mDNS/MDNSManager                # mDNS 服务
├── ini/
│   ├── boards.ini              # 所有支持的开发板环境定义
│   ├── pinouts.ini             # 各板子摄像头引脚映射
│   ├── user_config.ini         # 用户配置（OTA账号密码等）
│   └── dev_config.ini          # ★ 全局编译标志、依赖库、PlatformIO设置
└── platformio.ini              # 主配置（引用上述ini文件）
```

---

## 关键架构

### 端口分配
| 端口 | 服务 |
|------|------|
| 80 | 配网页面（AP模式） / 视频流（STA模式） |
| 81 | REST API 服务器 |
| 1234 | Web 日志监控（SSE推送） |
| 8080 | 配置管理页面（STA模式） |

### 编译宏（关键分支）
- `ETVR_EYE_TRACKER_USB_API` — 定义时走 USB/串口眼球追踪模式，**不定义时走 Web 配网模式**（本项目默认不定义）
- `ETVR_EYE_TRACKER_WEB_API` — 代码中引用此宏的 #ifndef 块是 Web 配网功能
- `SIM_ENABLED` — 模拟模式（本项目未使用）
- `CONFIG_CAMERA_MODULE_*` — 根据板子自动定义，用于摄像头引脚配置
- `SERIAL_MANAGER_USE_HIGHER_FREQUENCY` — S3 板子使用 3M 波特率

### 启动流程 (main.cpp)
1. `setup()`: CPU 240MHz → Serial 115200 → Logo → LED → PSRAM 检查 → Camera → 加载NVS配置 → SerialManager → `etvr_eye_tracker_web_init()`
2. `etvr_eye_tracker_web_init()`: 断开WiFi → `wifiHandler.begin()` → 尝试连接已保存网络 → 成功则启动正常服务 / 失败则进入AP模式
3. `loop()`: LED处理 → SerialManager轮询 → DNS(AP模式) → 每10s输出调试信息

### 配网流程
1. WiFi STA连接失败 → 启动AP (`Galeros_ESP32`) + Captive Portal DNS
2. 用户连接AP → 自动弹出配网页 → 输入WiFi凭证
3. 保存到 NVS (`deviceConfig.setWifiConfig()`) → 2.5秒后 `ESP.restart()`
4. 重启后通过 NVS 中的配置连接 WiFi

### 日志系统
- **SSE 推送**: AsyncEventSource 在端口 1234，浏览器建立长连接接收日志
- **早期缓冲**: 最多20条日志在 Web 服务器启动前缓存
- **双通道**: `esp_log_set_vprintf(custom_log_vprintf)` 拦截 ESP_LOG + `SendLogToWeb()` 手动发送

---

## 已知 Bug 和待办事项

### 🔴 核心 Bug
1. **`Serial.available()` 始终返回 0** (`SerialManager.cpp` L98)
   - 导致串口日志转发永远不执行
   - 原因不明，开发者在注释中标注"真没招了 QAQ"
   
2. **`esp_log_set_vprintf` 日志回调不工作** (`main.cpp` L64-93)
   - `custom_log_vprintf` 设置了但 ESP_LOG 日志似乎不会触发回调
   - `enableCallbackLog` 设为 true 也无效（开发者注释："其实设置成TRUE也不会输出 DOGE"）
   
3. **`esp_log_set_vprintf` 重定向可能引发递归** (main.cpp L242)
   - 回调里调用 `Serial.printf` → 如果 Serial.printf 底层也走 ESP_LOG 就会死循环

### 🟡 待实现功能（来自 README TODO）
- [ ] 日志级别颜色区分（DEBUG/INFO/ERROR）
- [ ] 网页端扫描附近 WiFi 列表
- [ ] OTA 更新时暂停日志推送避免冲突
- [ ] 暗黑模式支持
- [ ] 优化内存占用

### 🟠 设计问题
- `SendLogToWeb` 被声明为 `extern "C"` 但实际是 C++ 代码，不需要 C linkage
- `WiFiHandler::begin()` 使用的 ssid/password 为空字符串时，会走 "No networks found" 分支 → 尝试连接空SSID → 超时 → 进入 ADHOC 模式（效率低）
- `streamServer.cpp` 中 `setupWebLogServer()` 在 `setup()` 中会报错（因为网络未初始化），所以被延迟到 `etvr_eye_tracker_web_init()` 中调用

---

## 编码约定

- 日志用 `Serial.printf()` + `SendLogToWeb()` 双输出
- 中文注释为主，英文日志输出
- `// ======` 分隔符标记代码区块
- 用户可配置参数集中在 `main.cpp` 顶部的 "用户自定义配置修改区域"
- ★ 标记的注释（如 `// ★ 重要`）表示关键点
- 编码：UTF-8，换行：CRLF (Windows)

---

## 相关资源

- 原版 OpenIris: https://github.com/lorow/OpenIris
- 本项目教程: https://www.galeros.xyz/2026/05/01/openiris-esp32/
- 父项目博客文章: `docs/blog-article.md`
