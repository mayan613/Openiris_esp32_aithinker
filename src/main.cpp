/**
 * @file main.cpp
 * @brief Galeros OpenIris ESP32 主程序
 *
 * 本文件是项目的核心入口，负责：
 *   1. 系统初始化（CPU频率、串口、LED、PSRAM检测、摄像头、配置加载）
 *   2. WiFi 智能配网（优先连接已保存网络，失败则进入 AP 模式）
 *   3. 独立 Web 日志服务器（端口 1234，基于 SSE 实时推送）
 *   4. 配置管理页面（STA 模式:8080 / AP 模式:80）
 *   5. 主循环（LED 状态指示、DNS 处理、定期输出系统状态）
 *
 * 用户可在此文件顶部的「用户自定义配置修改区域」修改 AP 名称、
 * 端口号、mDNS 主机名等参数。
 *
 * 依赖：
 *   - PlatformIO / Arduino 框架
 *   - OpenIris 库（lib/src/openiris.hpp）
 *   - ESPAsyncWebServer（配网页 + API + 日志页）
 *   - DNSServer（Captive Portal 自动弹出配网页）
 */

#include <DNSServer.h>
#include <WiFi.h>
#include <openiris.hpp>
#include "html_content.h" // 包含 HTML 页面内容（配网页、管理页、日志页）

// ======================================================================
//  用户自定义配置修改区域
//  下面所有带 "← 这里修改" 注释的变量都可以按需调整
// ======================================================================

// 全局最低日志级别——低于此级别的 GLOG 调用不会输出
// DEBUG = 输出所有日志（调试/信息/警告/错误），适合开发调试
// INFO  = 只输出信息/警告/错误，适合正式部署
// WARN  = 只输出警告/错误
// ERROR = 只输出错误
const GLogLevel LOG_LEVEL = GLogLevel::DEBUG; // ← 这里修改

// AP 热点的 SSID 和密码——仅在设备无法连接已保存 WiFi 时才会启用
// 用户连接此热点后浏览器会自动弹出配网页面
const char *AP_SSID = "Galeros_ESP32";
const char *AP_PASSWORD = "Galeros_ESP32";

// 配置管理页面端口——STA 模式下访问 http://设备IP:此端口 可修改 WiFi 设置
const uint16_t CONFIG_PORT = 8080; // ← 这里修改

// Web 日志页面端口——访问 http://设备IP:此端口 可实时查看系统日志
const uint16_t LOG_PORT = 1234; // ← 这里修改

// mDNS 主机名——设备会以此名称注册到局域网，可通过 http://此名称.local 访问
const char *MDNS_HOSTNAME = "Galeros-ESP32";

// WiFi 信道——指定 STA 连接时使用的信道（1-13），一般保持默认即可
const uint8_t WIFI_CHANNEL = 1; // ← 这里修改

// 是否启用官方 Adhoc 模式（AP+STA 混合）
// 注意：本项目的网页配网功能已覆盖 Adhoc 的场景，
//       开启此项可能导致两者冲突，强烈建议保持 false
const bool ENABLE_ADHOC = false; // ← 不建议修改

// ---- 以下为官方 Adhoc 功能的参数，不建议修改 ----
extern const char *ADHOC_AP_SSID;
extern const char *ADHOC_AP_PASSWORD;
extern const uint8_t ADHOC_AP_CHANNEL;
const char *ADHOC_AP_SSID = "Galeros_ESP32";
const char *ADHOC_AP_PASSWORD = "Galeros_ESP32";
const uint8_t ADHOC_AP_CHANNEL = 1;

// ======================================================================
//  用户自定义配置修改区域结束
// ======================================================================

// ======================================================================
//  全局对象声明
// ======================================================================

// DNS 服务器——用于 Captive Portal 功能，将任意域名请求劫持到配网页面 IP
DNSServer dnsServer;
const byte DNS_PORT = 53; // DNS 标准端口，不可修改

// AP 模式下的网关 IP（也是配网页面的访问地址 192.168.4.1）
IPAddress apIP(192, 168, 4, 1);

// 指向各个 Web 服务器的指针（延迟初始化，等 WiFi 就绪后再 new）
AsyncWebServer *configServer = nullptr; // 配置管理服务器（AP:80 / STA:8080）
AsyncWebServer *logServer = nullptr;    // 日志服务器（端口 1234）
AsyncEventSource *events = nullptr;     // SSE 对象——浏览器通过 /logs 长连接接收实时日志

// 标记当前是否处于 AP 模式（用于 loop 中判断是否需要处理 DNS 请求）
bool isInAPMode = false;

// ======================================================================
//  GLogManager Web SSE 回调
//  将统一日志系统（GLOG）的输出桥接到 Web 日志页面的 SSE 通道
// ======================================================================

/**
 * @brief 建立 GLogManager → Web SSE 的桥接
 *
 * GLogManager 是 lib/ 层的统一日志管理器，它本身不知道 Web 层的存在。
 * 本函数通过 setWebCallback 注册一个 lambda，当 GLOG 宏在任何地方被调用时，
 * 日志消息会自动通过 SSE 推送到浏览器。
 *
 * 调用时机：setupWebLogServer() 中，SSE 对象创建完成后。
 */
void setupGLogWebCallback()
{
  auto logCallback = [](const String &msg)
  {
    // 安全检查：SSE 对象存在且消息非空才发送
    if (events != nullptr && msg.length() > 0)
    {
      events->send(msg.c_str(), nullptr, millis());
    }
  };
  GLogManager::instance().setWebCallback(logCallback);
}

// ======================================================================
//  Captive Portal 请求处理器
//  在 AP 模式下，将所有 HTTP 请求重定向到配网页面
// ======================================================================

/**
 * @brief Captive Portal 实现
 *
 * 当设备处于 AP 模式时，用户连接热点后：
 *   1. 手机/电脑检测到需要登录 → 自动弹出浏览器
 *   2. 所有 HTTP 请求被此类拦截 → 重定向到 192.168.4.1
 *   3. 用户看到配网页面，输入 WiFi 凭证
 */
class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  // 匹配所有请求（Captive Portal 需要拦截一切）
  bool canHandle(AsyncWebServerRequest *request) { return true; }

  // 将请求重定向到配网页面
  void handleRequest(AsyncWebServerRequest *request)
  {
    request->redirect("http://" + apIP.toString());
  }
};

// ======================================================================
//  核心模块实例化
//  这些对象在整个程序生命周期内存在，管理设备的所有功能
// ======================================================================

/**
 * deviceConfig — NVS 持久化配置管理器
 * 保存/读取 WiFi 凭证、摄像头参数、OTA 账号、mDNS 名称等
 * 数据存储在 ESP32 的 NVS (Non-Volatile Storage) 分区，断电不丢失
 */
ProjectConfig deviceConfig("openiris", MDNS_HOSTNAME);

/**
 * commandManager — 串口 / API JSON 命令解析器
 * 支持的命令：ping、set_wifi、set_mdns
 */
CommandManager commandManager(&deviceConfig);

/**
 * serialManager — 串口通信管理器
 * 在 USB 模式下负责摄像头帧传输；在 Web 模式下负责串口数据转发
 */
SerialManager serialManager(&commandManager);

// ---- LED 管理器：根据不同的开发板使用不同的内置 LED 引脚 ----
#ifdef CONFIG_CAMERA_MODULE_ESP32S3_XIAO_SENSE
LEDManager ledManager(LED_BUILTIN); // XIAO ESP32S3: 使用内置 LED
#elif CONFIG_CAMERA_MODULE_SWROOM_BABBLE_S3
LEDManager ledManager(38); // Babble S3: GPIO 38
#else
LEDManager ledManager(33); // 默认（AI Thinker 等）: GPIO 33
#endif

// ---- 摄像头处理器：仅在非模拟模式下编译 ----
#ifndef SIM_ENABLED
CameraHandler cameraHandler(deviceConfig);
#endif

// ---- 以下模块仅在 Web 配网模式下编译（非 USB 眼球追踪模式）----
#ifndef ETVR_EYE_TRACKER_USB_API

/**
 * wifiHandler — WiFi 连接管理器
 * 参数说明：
 *   - 前两个空字符串：不再使用硬编码 WiFi，由网页配网动态设置
 *   - WIFI_CHANNEL：STA 连接信道
 *   - ENABLE_ADHOC：是否启用官方 Adhoc 模式
 */
WiFiHandler wifiHandler(deviceConfig,
                        "",            // SSID（留空，由配网页设置）
                        "",            // 密码（留空，由配网页设置）
                        WIFI_CHANNEL,  // WiFi 信道
                        ENABLE_ADHOC); // Adhoc 开关

// mDNS 处理器——让设备可以通过 http://主机名.local 访问
MDNSHandler mdnsHandler(deviceConfig);

// API 服务器——RESTful 接口，端口 81，提供 WiFi/Camera/OTA 等配置接口
#ifdef SIM_ENABLED
APIServer apiServer(deviceConfig, wifiStateManager, "/control");
#else
APIServer apiServer(deviceConfig, cameraHandler, "/control");
StreamServer streamServer; // MJPEG 视频流服务器，端口 80
#endif

// ======================================================================
//  配网页面设置
//  根据设备当前状态（AP 模式 / STA 模式）显示不同的页面
// ======================================================================

/**
 * @brief 配置 Web 服务器的路由
 *
 * @param isAP  true  = AP 模式（显示配网页面 + Captive Portal）
 *              false = STA 模式（显示当前网络信息 + 修改页面）
 *
 * 路由说明：
 *   GET  /      → 根据模式显示配网页或管理页
 *   POST /save  → 接收用户提交的 WiFi 凭证 → 保存到 NVS → 重启设备
 *
 * AP 模式下额外启用 CaptiveRequestHandler 实现自动弹出配网页
 */
void setupConfigServer(bool isAP)
{
  if (!configServer)
    return; // 防御性检查：configServer 尚未创建

  if (isAP)
  {
    // ========== AP 模式：显示配网页面 ==========
    // 用户连接热点后访问任意地址都会看到此页面
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                     { request->send(200, "text/html, charset=utf-8", ap_config_html); });
  }
  else
  {
    // ========== STA 模式：显示管理页面 ==========
    // 页面会显示当前连接的 WiFi 名称，并允许用户修改
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                     {
      String html = sta_config_html;
      String currentSSID = WiFi.SSID();
      if (currentSSID.length() == 0)
        currentSSID = "未知"; // 未获取到 SSID 时的占位文本
      // 将 HTML 模板中的占位符替换为实际 SSID
      html.replace("%CURRENT_SSID%", currentSSID);
      request->send(200, "text/html, charset=utf-8", html); });
  }

  // ========== 保存 WiFi 配置（两种模式共用）==========
  // 用户在网页上填写 SSID 和密码后提交到此路由
  configServer->on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
                   {
    // 解析 POST 表单数据
    if (request->hasParam("ssid", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->hasParam("password", true)
                        ? request->getParam("password", true)->value()
                        : "";

      if (ssid.length() == 0) {
        request->send(400, "text/plain", "SSID不能为空！");
        return;
      }

      // 将 WiFi 凭证保存到 NVS 分区
      // 参数：网络名 "main"、SSID、密码、信道=0(自动)、功率=52(11dBm)、非Adhoc、通知观察者
      deviceConfig.setWifiConfig("main", ssid.c_str(), pass.c_str(), 0, 52,
                                 false, true);

      // 通知用户配置已保存，2.5 秒后自动重启使新配置生效
      request->send(200, "text/plain, charset=utf-8", "配置已保存！设备即将重启...");
      delay(2500);
      ESP.restart();
    }
    request->send(400, "text/plain", "参数错误"); });

  // AP 模式下启用 Captive Portal——将所有 DNS 请求劫持到配网页
  if (isAP)
  {
    configServer->addHandler(new CaptiveRequestHandler())
        .setFilter(ON_AP_FILTER); // 仅在 AP 模式下生效
  }
}

// ======================================================================
//  Web 日志服务器
//  独立的 Web 服务器，通过 SSE (Server-Sent Events) 实时推送日志到浏览器
// ======================================================================

/**
 * @brief 创建并启动独立的 Web 日志服务器
 *
 * 功能：
 *   1. 在 LOG_PORT（默认 1234）上启动 AsyncWebServer
 *   2. 注册 "/" 路由 → 返回日志查看页面 HTML
 *   3. 注册 "/logs" SSE 端点 → 浏览器通过 EventSource 长连接接收实时日志
 *   4. 将 GLogManager 的 Web 回调桥接到 SSE 通道
 *   5. 转发早期日志缓冲（Web 服务器启动前缓存的日志）
 *
 * 调用时机：WiFi 连接成功后或进入 AP 模式后，由 etvr_eye_tracker_web_init() 调用
 *
 * 注意：本函数只应被调用一次（通过 webLogServerStarted 标志控制）
 */
void setupWebLogServer()
{
  delay(1000); // 等待 WiFi 连接完全稳定

  // 防止重复创建
  if (logServer != nullptr)
  {
    GLOG_I("WEB", "Web log server already exists, skipping creation");
    return;
  }

  GLOG_I("WEB", "Creating Web log server on port %d...", LOG_PORT);

  // ---- 1. 创建日志服务器 ----
  logServer = new AsyncWebServer(LOG_PORT);

  // ---- 2. 注册日志查看页面 ----
  // 浏览器访问 http://设备IP:1234 即可看到实时日志界面
  logServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/html", web_log_html); });

  // ---- 3. 注册 SSE 端点 ----
  // 浏览器通过 new EventSource("/logs") 建立长连接
  // 服务器通过 events->send() 推送日志消息
  events = new AsyncEventSource("/logs");
  GLOG_D("WEB", "AsyncEventSource created, events=%p", events);
  logServer->addHandler(events);

  // ---- 4. 启动服务器 ----
  logServer->begin();
  GLOG_I("WEB", "Web Log Server started on port %d", LOG_PORT);

  // ---- 5. 桥接 GLogManager → SSE ----
  // 此后所有 GLOG 宏都会同时输出到串口和 Web 日志页面
  setupGLogWebCallback();
  GLogManager::instance().webReady = true; // 标记 Web 就绪
  GLOG_I("WEB", "GLog → Web SSE callback connected");

  // ---- 6. 设置 SerialManager 的日志回调（保留兼容）----
  // SerialManager 从串口读取到的数据也会通过 GLOG 转发到 Web
  serialManager.setLogCallback([](const String &msg)
                               { GLogManager::instance().log(GLogLevel::INFO, "SERIAL", msg.c_str()); });
}

// ======================================================================
//  Web 模式初始化（替代原 OpenIris 的 USB 眼球追踪模式）
//  这是整个程序的网络初始化入口
// ======================================================================

/**
 * @brief Web 配网模式的网络初始化
 *
 * 执行流程：
 *   ┌─ WiFi.disconnect() + WiFi.mode(STA)
 *   ├─ wifiHandler.begin() → 尝试连接 NVS 中保存的 WiFi
 *   ├─ 等待 1 秒
 *   ├─ 如果 WiFi 已连接：
 *   │   ├─ 启动 Web 日志服务器（首次）
 *   │   ├─ 启动 mDNS 服务
 *   │   ├─ 启动摄像头视频流服务器
 *   │   ├─ 启动 REST API 服务器
 *   │   └─ 启动配置管理页面（端口 CONFIG_PORT）
 *   │
 *   └─ 如果 WiFi 未连接：
 *       ├─ 启动 Web 日志服务器（首次）
 *       ├─ 切换到 AP 模式
 *       ├─ 启动 SoftAP（热点名 = AP_SSID）
 *       ├─ 启动 DNS 劫持（Captive Portal）
 *       └─ 启动配网页面（端口 80）
 *
 * 此函数仅在 ETVR_EYE_TRACKER_USB_API 宏未定义时编译。
 */
void etvr_eye_tracker_web_init()
{
  GLOG_I("SETUP", "Starting custom WiFi configuration...");

  // 先断开所有 WiFi 连接，确保干净的初始状态
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA); // 先设为 STA 模式，尝试连接

  // 注册 mDNS 观察者：当配置变更时自动更新 mDNS
  deviceConfig.attach(mdnsHandler);

  GLOG_I("SETUP", "Starting WiFi handler");
  // wifiHandler.begin() 内部逻辑：
  //   1. 尝试连接 NVS 中保存的所有网络
  //   2. 如果全部失败 → 自动进入 ADHOC/AP 模式
  // 注意：我们的 ENABLE_ADHOC=false，且 NVS 中无网络时 begin() 不会启动 AP
  //       而是走完所有尝试后返回，由下面的代码来启动我们的自定义 AP 模式
  wifiHandler.begin();

  delay(1000); // 等待连接稳定

  // ====================================================================
  //  分支 1：WiFi 已连接（STA 模式）
  // ====================================================================
  if (WiFi.isConnected())
  {
    // 确保 Web 日志服务器只启动一次
    static bool webLogServerStarted = false;
    if (!webLogServerStarted)
    {
      GLOG_I("SETUP", "WiFi connected, starting Web log server...");
      setupWebLogServer();
      webLogServerStarted = true;
      GLOG_I("SETUP", "Device connected to WiFi, Web log server started!");
    }

    // ---- 依次启动各项网络服务 ----

    GLOG_I("SETUP", "WiFi connected! Starting main services...");

    // mDNS：让设备可以通过 http://Galeros-ESP32.local 访问
    GLOG_I("SETUP", "Starting mDNS handler");
    mdnsHandler.startMDNS();

    // MJPEG 视频流：http://设备IP:80
    GLOG_I("SETUP", "Starting stream server");
    streamServer.startStreamServer();

    // REST API：http://设备IP:81/control/xxx/command/xxx
    // 提供 WiFi、Camera、OTA 等配置接口
    GLOG_I("SETUP", "Starting API server");
    apiServer.setup();

    // 配置管理页面：http://设备IP:CONFIG_PORT (默认 8080)
    // 用户可以在此页面查看当前连接和修改 WiFi
    configServer = new AsyncWebServer(CONFIG_PORT);
    setupConfigServer(false); // false = STA 模式，显示管理页
    configServer->begin();

    String configUrl = "http://" + WiFi.localIP().toString() + ":" + String(CONFIG_PORT);
    GLOG_I("SETUP", "Config page available at: %s", configUrl.c_str());

    return; // STA 模式初始化完成，函数返回
  }

  // ====================================================================
  //  分支 2：WiFi 未连接（AP 模式）
  // ====================================================================
  GLOG_I("SETUP", "WiFi not connected, entering AP mode...");

  // 切换到 AP 模式并创建热点
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);                          // 创建热点
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0)); // 设置网关 IP 和子网掩码

  // 启动 DNS 劫持——将所有域名请求解析到 apIP (192.168.4.1)
  // 这是实现 Captive Portal（自动弹出配网页）的关键
  dnsServer.start(DNS_PORT, "*", apIP);

  // 确保 Web 日志服务器只启动一次（AP 模式下同样需要日志）
  static bool webLogServerStarted = false;
  if (!webLogServerStarted)
  {
    GLOG_I("SETUP", "AP mode, starting Web log server...");
    setupWebLogServer();
    webLogServerStarted = true;
    GLOG_I("SETUP", "Device entered AP mode, Web log server started!");
  }

  // 配网页面运行在 80 端口（HTTP 默认端口，方便 Captive Portal 重定向）
  configServer = new AsyncWebServer(80);
  setupConfigServer(true); // true = AP 模式，显示配网页面 + 启用 Captive Portal
  configServer->begin();

  isInAPMode = true; // 标记 AP 模式，loop() 中需要处理 DNS 请求
  GLOG_I("SETUP", "AP mode + Captive Portal started! Connect to '%s'", AP_SSID);
}

#endif // ETVR_EYE_TRACKER_WEB_API

// ======================================================================
//  setup() — Arduino 标准初始化函数，上电后只执行一次
// ======================================================================

/**
 * @brief 系统初始化
 *
 * 执行顺序：
 *   1. CPU 频率设为 240MHz（最大性能）
 *   2. 串口初始化 115200 波特率
 *   3. 打印 ASCII Logo（OpenIris 艺术字）
 *   4. LED 管理器初始化
 *   5. 打印 PSRAM / RAM 使用情况
 *   6. 特定板子的硬件初始化（如 Babble S3 的红外 LED）
 *   7. 加载 NVS 中的持久化配置
 *   8. 串口通信初始化
 *   9. 启动网络（WiFi配网 + 各项 Web 服务）
 */
void setup()
{
  // ---- 1. 性能设置 ----
  setCpuFrequencyMhz(240); // ESP32 最高频率，保证摄像头帧率

  // ---- 2. 串口初始化 ----
  Serial.begin(115200); // 与 platformio.ini 中的 monitor_speed 保持一致

  // ---- 3. 打印启动 Logo ----
  Logo::printASCII(); // ASCII 艺术字（OpenIris Logo）

  // ---- 4. LED 初始化 ----
  ledManager.begin(); // 根据不同状态显示不同的闪烁模式

  // ---- 5. 内存状态报告 ----
  // PSRAM（外部 SPI RAM）——摄像头帧缓冲需要，没有的话图像质量会下降
  if (psramFound())
  {
    GLOG_I("BOOT", "PSRAM usage: used %d KB, free %d KB",
           (ESP.getPsramSize() - ESP.getFreePsram()) / 1024,
           ESP.getFreePsram() / 1024);
  }
  // 内部 DRAM
  GLOG_I("BOOT", "RAM usage: used %d KB, free %d KB",
         (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024,
         ESP.getFreeHeap() / 1024);

  // ---- 6. 板级特定初始化 ----
#ifdef CONFIG_CAMERA_MODULE_SWROOM_BABBLE_S3
  // Babble S3 开发板：配置红外 LED 发射器（用于暗光环境下的眼球追踪）
  // GPIO 1 输出 PWM，频率 5kHz，占空比 100%（最大亮度）
  const int ledPin = 1;
  const int freq = 5000;
  const int ledChannel = 0;
  const int resolution = 8;
  const int dutyCycle = 255;
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(1, ledChannel);
  ledcWrite(ledChannel, dutyCycle);
#endif

  // ---- 7. 加载持久化配置 ----
#ifndef SIM_ENABLED
  deviceConfig.attach(cameraHandler); // 注册摄像头为配置观察者
#endif
  deviceConfig.load(); // 从 NVS 读取所有配置（WiFi/Camera/mDNS等）

  // ---- 8. 串口管理器初始化 ----
  serialManager.init(); // 高波特率板子会切换到 3Mbps

  // ---- 8. 设置日志级别 ----
  // deviceConfig.load() 之后调用，LOG_LEVEL 定义在本文件顶部
  GLogManager::instance().setLevel(LOG_LEVEL);

  // ---- 9. 网络启动 ----
#ifndef ETVR_EYE_TRACKER_USB_API
  etvr_eye_tracker_web_init(); // Web 配网模式（本项目默认）
#else
  WiFi.disconnect(true); // USB 眼球追踪模式（原版 OpenIris）
#endif
}

// ======================================================================
//  loop() — Arduino 标准主循环，setup() 完成后反复执行
// ======================================================================

/**
 * @brief 主循环
 *
 * 每次迭代执行：
 *   1. LED 状态刷新——根据当前系统状态切换闪烁模式
 *   2. 串口轮询——检查是否有串口数据（JSON 命令或日志）
 *   3. DNS 请求处理——仅在 AP 模式下，处理 Captive Portal 的 DNS 劫持
 *   4. 系统状态报告——每 10 秒输出一次内存和日志状态（DEBUG 级别，受 LOG_LEVEL 控制）
 */
void loop()
{
  // ---- 1. LED 状态指示灯 ----
  // 根据系统状态（WiFi连接中/已连接/错误等）自动切换闪烁模式
  ledManager.handleLED();

  // ---- 2. 串口通信处理 ----
  // 检查是否有来自串口的 JSON 命令或日志数据
  // 注意：Web 配网模式下通常没有外部设备发送串口数据，
  //       所以 Serial.available() 一般为 0，这是正常现象
  serialManager.run();

  // ---- 3. DNS 劫持处理（仅 AP 模式）----
  // 将客户端的所有 DNS 请求解析到 192.168.4.1，
  // 配合 CaptiveRequestHandler 实现自动弹出配网页面
  if (isInAPMode)
  {
    dnsServer.processNextRequest();
  }

  // ---- 4. 定期系统状态报告 ----
  // 每 10 秒输出一次调试信息（日志级别低于 LOG_LEVEL 时自动跳过）

  static unsigned long lastDebugTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastDebugTime > 10000) // 10000ms = 10 秒
  {
    lastDebugTime = currentTime;

    // 日志服务器和 SSE 对象的状态
    GLOG_D("SYS", "logServer=%p, events=%p, webReady=%d",
           logServer, events, GLogManager::instance().webReady);

    // PSRAM 使用情况（如果硬件支持）
    if (psramFound())
    {
      GLOG_D("SYS", "PSRAM: used %d KB, free %d KB",
             (ESP.getPsramSize() - ESP.getFreePsram()) / 1024,
             ESP.getFreePsram() / 1024);
    }

    // 内部 RAM 使用情况
    GLOG_D("SYS", "RAM: used %d KB, free %d KB",
           (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024,
           ESP.getFreeHeap() / 1024);
  }
}
