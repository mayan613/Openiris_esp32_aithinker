#include <DNSServer.h>
#include <WiFi.h>
#include <openiris.hpp>
#include "html_content.h" // 包含HTML内容的头文件，用户可以在这个文件中修改HTML内容来定制配网页面
#include <esp_log.h>      // 添加ESP日志重定向支持

// =============用户自定义配置修改区域=====================
// 是否启用日志回调功能（即将串口的日志转发到Web服务器）
// TOD0 目前有bug，设置成true也没用 (doge)
const bool enableCallbackLog = true; // ← 这里修改，默认启用日志回调功能，如果用户不想启用日志回调功能，可以将这个参数设置为false，这样就不会将ESP_LOG的日志发送到网页上了，只会在串口输出

// 是否启用日志详细输出（包括web界面）（即在日志中包含更多的调试信息，如内存使用情况、回调调用次数等）(多是循环输出的日志，可能会比较频繁，建议在调试阶段启用，正式使用时可以关闭以减少性能影响)
const bool enableDetailedLog = true; // ← 这里修改，默认启用日志详细输出，推荐开启

// 在我们的功能下的AP的SSID和密码配置，之所以说在我们下是因为官方项目有类似于AP配网的功能，会和我们冲突，到时候他们的那些参数我会在下面定义并使用，但用户不需要修改那些参数，用户只需要修改下面的AP_SSID和AP_PASSWORD即可（除非你使用官方的AP功能，也就是adhoc）
const char *AP_SSID = "Galeros_ESP32";     // 这个是当设备进入AP模式时的WiFi名称，用户可以修改为自己喜欢的名字
const char *AP_PASSWORD = "Galeros_ESP32"; // 这个是当设备进入AP模式时的WiFi密码，建议设置一个比较复杂的密码以防止他人连接你的设备，如果你不想设置密码，可以将这个参数设置为一个空字符串""，但不建议这样做，因为这会让任何人都能连接你的设备，存在安全风险

// 配置管理端口
const uint16_t CONFIG_PORT = 8080; // ← 这里修改
// 日志网页端口配置
const uint16_t LOG_PORT = 1234; // ← 这里修改
// msdn的名字配置
const char *MDNS_HOSTNAME = "Galeros-ESP32"; // 这个名字会出现在局域网设备列表中，建议修改为独一无二的名字以便识别
// WiFi的channel配置
const uint8_t WIFI_CHANNEL = 1; // ← 这里修改
// 是否启用Adhoc模式（即AP+STA模式）(官方的配网功能，注意：如果你启用了这个功能，请确保理解它的工作原理，并且做好调试准备，因为这个功能可能会和我们的配网功能起冲突喵~)
const bool ENABLE_ADHOC = false; // ← 不建议修改，请看注意事项
// 注意：本自定义项目已经和这个功能类似，不建议用户将其设置为True，以免与我们的配网功能冲突，如果你确实需要这个功能，请确保理解它的工作原理，并且做好调试准备
// 官方adhoc功能相关参数（不建议用户修改，除非你需要使用官方的配网功能，并且理解它的工作原理）
// 这些代码是给adhoc声明变量,这些变量会在wifihandler.cpp中被使用到，以确保当用户启用adhoc模式时能够正确设置AP的参数
extern const char *ADHOC_AP_SSID;
extern const char *ADHOC_AP_PASSWORD;
extern const uint8_t ADHOC_AP_CHANNEL;
// adhoc功能相关参数定义（不建议用户修改，除非你需要使用官方的配网功能，并且理解它的工作原理）
const char *ADHOC_AP_SSID = "Galeros_ESP32";     // adhoc模式下的AP SSID
const char *ADHOC_AP_PASSWORD = "Galeros_ESP32"; // adhoc模式下的AP密码
const uint8_t ADHOC_AP_CHANNEL = 1;              // adhoc模式下的AP频道
// =============用户自定义配置修改区域结束=====================

// 自定义代码区
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

AsyncWebServer *configServer = nullptr; // 配置专用服务器
AsyncWebServer *logServer = nullptr;    // 日志专用服务器
AsyncEventSource *events = nullptr;     // SSE 响应对象（用于实时推送日志）
bool isInAPMode = false;

// ====================== 全局日志系统 ======================
// 早期日志缓冲（在 Web 服务器启动前收集）
static const size_t LOG_BUFFER_SIZE = 20; // 最多缓存 20 条日志
static String earlyLogBuffer[LOG_BUFFER_SIZE];
static size_t logBufferIndex = 0;
static bool logBufferFull = false;

// 日志回调和统计
std::function<void(const String &)> globalLogCallback = nullptr;
static unsigned long logCallbackCount = 0;
static bool logSystemReady = false; // Web 服务器是否就绪

// 自定义ESP_LOG输出函数，重定向到Web日志
int custom_log_vprintf(const char *format, va_list args)
{
  Serial.printf("[custom_log_vprintf] 进入本函数，准备处理日志...\n");
  char buffer[512];
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  if (len > 0)
  {
    String msg = String(buffer);

    // 如果 Web 服务器已就绪，直接转发
    if (logSystemReady && globalLogCallback)
    {
      Serial.printf("[custom_log_vprintf] Web服务器就绪，直接转发日志: %s", msg.c_str());
      globalLogCallback(msg);
      logCallbackCount++;
    }
    // 否则存入早期缓冲
    else if (!logBufferFull && logBufferIndex < LOG_BUFFER_SIZE)
    {
      Serial.printf("[custom_log_vprintf] Web服务器未就绪，日志存入缓冲: %s", msg.c_str());
      earlyLogBuffer[logBufferIndex] = msg;
      logBufferIndex++;
    }
  }
  else
  {
    Serial.println("[custom_log_vprintf] 日志内容为空！");
  }
  return len;
}

// 自定义代码区
class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request) { return true; }

  void handleRequest(AsyncWebServerRequest *request)
  {
    request->redirect("http://" + apIP.toString());
  }
};

/**
 * @brief ProjectConfig object
 * @brief This is the main configuration object for the project
 * @param name The name of the project config partition
 * @param mdnsName The mDNS hostname to use
 */
ProjectConfig deviceConfig("openiris", MDNS_HOSTNAME);
CommandManager commandManager(&deviceConfig);
SerialManager serialManager(&commandManager);

#ifdef CONFIG_CAMERA_MODULE_ESP32S3_XIAO_SENSE
LEDManager ledManager(LED_BUILTIN);

#elif CONFIG_CAMERA_MODULE_SWROOM_BABBLE_S3
LEDManager ledManager(38);

#else
LEDManager ledManager(33);
#endif // ESP32S3_XIAO_SENSE

#ifndef SIM_ENABLED
CameraHandler cameraHandler(deviceConfig);
#endif // SIM_ENABLED

#ifndef ETVR_EYE_TRACKER_USB_API
WiFiHandler wifiHandler(deviceConfig,
                        "",            // 这个参数在我们新的配网方案中已经不再使用，用户需要通过网页配网界面来设置WiFi信息，这里我们保留这个参数但不使用它，以防止与配网功能冲突
                        "",            // 同上
                        WIFI_CHANNEL,  // 在本文件最上方由用户配置，后续考虑加入网页配置界面
                        ENABLE_ADHOC); // 在本文件最上方由用户配置，请看注意事项
MDNSHandler mdnsHandler(deviceConfig);
#ifdef SIM_ENABLED
APIServer apiServer(deviceConfig, wifiStateManager, "/control");
#else
APIServer apiServer(deviceConfig, cameraHandler, "/control");
StreamServer streamServer;
#endif // SIM_ENABLED

// 自定义核心处理逻辑
void setupConfigServer(bool isAP)
{
  if (!configServer)
    return;

  if (isAP)
  {
    // AP模式：显示配网页面
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                     { request->send(200, "text/html, charset=utf-8", ap_config_html); });
  }
  else
  {
    // STA模式（已连接）：显示当前网络 + 修改页面
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                     {
      String html = sta_config_html;
      String currentSSID = WiFi.SSID();
      if (currentSSID.length() == 0)
        currentSSID = "未知";
      html.replace("%CURRENT_SSID%", currentSSID);
      request->send(200, "text/html, charset=utf-8", html); });
  }

  // 保存逻辑（两种模式共用）
  configServer->on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
                   {
    if (request->hasParam("ssid", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->hasParam("password", true)
                        ? request->getParam("password", true)->value()
                        : "";

      if (ssid.length() == 0) {
        request->send(400, "text/plain", "SSID不能为空！");
        return;
      }

      // 保存到 ProjectConfig（使用第一个网络槽）
      deviceConfig.setWifiConfig("main", ssid.c_str(), pass.c_str(), 0, 52,
                                 false, true);

      request->send(200, "text/plain, charset=utf-8", "配置已保存！设备即将重启...");
      delay(2500);
      ESP.restart();
    }
    request->send(400, "text/plain", "参数错误"); });

  // AP模式下启用 Captive Portal
  if (isAP)
  {
    configServer->addHandler(new CaptiveRequestHandler())
        .setFilter(ON_AP_FILTER);
  }
}

// ====================== 独立 Web 日志服务器函数 ======================
// 目前功能以自定义日志发送为主，日志回调功能有问题，后续修复
void setupWebLogServer() // 这个函数专门用来设置Web日志服务器的,只会被调用一次，但在setup中调用会报错，因为此时网络没有初始化完毕，所以放在etvr_eye_tracker_web_init()中调用了
{
  delay(1000); // 确保WiFi连接稳定后再启动日志服务器
  if (logServer != nullptr)
  {
    Serial.printf("[setupWebLogServer] 日志服务器已存在，跳过创建\n");
    return;
  }

  Serial.printf("\n[setupWebLogServer] 开始创建日志服务器...\n");
  logServer = new AsyncWebServer(LOG_PORT); // 端口号在上面定义，用户可以修改

  // 日志主页
  logServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/html", web_log_html); });

  // SSE 实时日志推送
  events = new AsyncEventSource("/logs");
  Serial.printf("[setupWebLogServer] 已创建AsyncEventSource，events=%p\n", events);
  logServer->addHandler(events);

  logServer->begin();

  Serial.printf("[LOG] Web 日志服务器已启动 → http://设备IP:%d\n", LOG_PORT);
  Serial.printf("[LOG] Web Log Server initialized on port %d\n", LOG_PORT);

  // ====================== 连接日志回调到 Web 日志服务器 ======================
  // TDO0：有问题，后续修复
  // 根据前面的bool参数决定是否启用日志回调功能，默认启用，如果用户不想启用日志回调功能，可以将这个参数设置为false，这样就不会将ESP_LOG的日志发送到网页上了，只会在串口输出
  // 其实设置成 TRUE也不会输出 DOGE
  if (enableCallbackLog)
  {
    Serial.printf("[setupWebLogServer] 日志回调状态参数为 %d, 设置日志回调...\n", enableCallbackLog);
    auto logCallback = [](const String &msg)
    {
      // 移除调试输出以避免递归
      if (events != nullptr && msg.length() > 0)
      {
        String cleanMsg = msg;
        cleanMsg.trim();
        if (cleanMsg.length() > 0)
        {
          events->send(cleanMsg.c_str()); // 直接发送日志内容，不区分事件类型
          Serial.printf("[LOG CALLBACK] 已发送到网页: %s\n", cleanMsg.c_str());
        }
      }
    };

    // 设置全局回调
    globalLogCallback = logCallback;
    Serial.printf("[setupWebLogServer] 日志回调设置完成\n");

    // ====================== 转发早期日志缓冲 ======================
    Serial.printf("[setupWebLogServer] 转发 %d 条早期日志...\n", logBufferIndex);
    for (size_t i = 0; i < logBufferIndex; i++)
    {
      logCallback(earlyLogBuffer[i]);
    }
    logBufferFull = true; // 标记缓冲已满，后续日志直接转发
    Serial.printf("[setupWebLogServer] 早期日志转发完毕\n");

    // 标记日志系统就绪
    logSystemReady = true;

    serialManager.setLogCallback(logCallback);
    Serial.printf("[setupWebLogServer] 日志系统第一部分初始化完毕\n");
  }
  else
  {
    Serial.printf("[setupWebLogServer] 日志回调状态参数为 %d, 日志回调已禁用\n", enableCallbackLog);
  }

  // 适用于log_d的日志回调设置
  Serial.printf("[setupWebLogServer] 初始化日志系统第二部分...\n");
  Serial.printf("[setupWebLogServer] 检测日志回调状态参数...\n");
  Serial.printf("[setupWebLogServer] enableCallbackLog状态为 %d\n", enableCallbackLog);
  if (enableCallbackLog)
  {
    Serial.printf("[setupWebLogServer] 日志回调已启用，串口日志将被转发到Web服务器\n");
    Serial.printf("[setupWebLogServer] 设置日志回调函数...(适用于ESP_LOG)\n");
    esp_log_set_vprintf(custom_log_vprintf);
    Serial.printf("[setupWebLogServer] 日志重定向已启用\n");
  }
  else
  {
    Serial.printf("[setupWebLogServer] 日志回调已禁用，只有自定义日志会被转发\n");
  }
}

// ====================== 自定义日志发送核心函数 ======================
void SendLogToWeb(const char *initialCustomLog = nullptr) // 自定义传入的日志
{

  // 增加自定义日志发送功能，如果用户传入了initialCustomLog参数，就立即发送这条日志到网页上
  // 为什么要这样干？因为我修了很多遍日志回调的功能，他就是不工作，所以我打算通过自定义发送日志的方式，算是曲线救国了。
  if (initialCustomLog != nullptr && strlen(initialCustomLog) > 0)
  {
    if (enableDetailedLog)
    {
      Serial.printf("[SendLogToWeb] 发送初始自定义日志: %s\n", initialCustomLog);
    }
    if (events != nullptr)
    {
      events->send(initialCustomLog, nullptr, millis());
      if (enableDetailedLog)
      {
        Serial.printf("[SendLogToWeb] 初始自定义日志已发送到网页\n");
      }
    }
    else
    {
      Serial.printf("[SendLogToWeb] 无法发送初始自定义日志，events对象未初始化\n");
    }
  }

  if (initialCustomLog == nullptr || strlen(initialCustomLog) <= 0 && enableCallbackLog == false && enableDetailedLog == true)
  {
    Serial.printf("[SendLogToWeb] 没有需要发送的自定义和回调日志，web日志功能已禁用\n");
  }
}

void etvr_eye_tracker_web_init()
{
  Serial.printf("[网络]: 开始自定义WiFi配置...\n");

  // 先断开 WiFi，避免 wifiHandler.begin() 使用默认配置干扰
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  Serial.printf("[SETUP]: Starting Network Handler");
  deviceConfig.attach(mdnsHandler);
  Serial.printf("[设置]: 开始WiFi处理器\n");
  wifiHandler.begin();
  // 自定义代码
  delay(1000);
  if (WiFi.isConnected())
  {
    // ==================== 延迟启动 Web 日志服务器 ====================
    static bool webLogServerStarted = false;

    if (!webLogServerStarted)
    {
      Serial.printf("[SETUP] WiFi已连接，准备启动Web日志服务器...\n");
      setupWebLogServer();
      webLogServerStarted = true;
      Serial.printf("[SETUP] Web日志服务器启动完毕\n");
      SendLogToWeb("[SETUP] 设备已连接到WiFi，Web日志服务器启动成功！"); // 连接成功后发送日志到网页
    }

    Serial.printf("[设置]: WiFi已连接！开始启动主要服务...\n");
    SendLogToWeb("[设置]: WiFi已连接！开始启动主要服务..."); // 连接成功后发送日志到网页
    Serial.printf("[设置]: 开始MDNS处理器\n");
    SendLogToWeb("[设置]: 开始MDNS处理器");
    mdnsHandler.startMDNS();
    Serial.printf("[设置]: 开始流媒体服务器\n");
    SendLogToWeb("[设置]: 开始流媒体服务器");
    streamServer.startStreamServer();
    Serial.printf("[设置]: 开始API服务器\n");
    SendLogToWeb("[设置]: 开始API服务器");
    apiServer.setup();

    // 后端管理窗口
    configServer = new AsyncWebServer(CONFIG_PORT);
    setupConfigServer(false);

    configServer->begin();

    String configUrl = "http://" + WiFi.localIP().toString() + ":" + String(CONFIG_PORT);
    Serial.printf("配置页面可用: %s\n", configUrl.c_str());
    SendLogToWeb(("配置页面可用: " + configUrl).c_str());

    return;
  }

  // WIFI未连接时

  Serial.printf("[设置]: WiFi未连接，开始AP模式...\n");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD); // ssid,password
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  dnsServer.start(DNS_PORT, "*", apIP);

  // 延迟启动日志服务器
  static bool webLogServerStarted = false;
  if (!webLogServerStarted)
  {
    Serial.printf("[SETUP] AP模式，准备启动Web日志服务器...\n");
    setupWebLogServer();
    webLogServerStarted = true;
    Serial.printf("[SETUP] Web日志服务器启动完毕\n");
    SendLogToWeb("[SETUP] 设备已进入AP模式，Web日志服务器启动成功！"); // AP模式启动后发送日志到网页
  }

  configServer = new AsyncWebServer(80);
  setupConfigServer(true); // ap模式
  configServer->begin();

  isInAPMode = true;
  String apSSID = String(AP_SSID);
  Serial.printf("AP模式 + 配置门户已启动！连接到 '%s'\n", apSSID.c_str());
  SendLogToWeb(("[设置]: AP模式 + 配置门户已启动！连接到 '" + apSSID + "'").c_str());
}
#endif // ETVR_EYE_TRACKER_WEB_API

void setup()
{
  setCpuFrequencyMhz(240);
  Serial.begin(115200);

  Logo::printASCII();
  ledManager.begin();
  // 开始时打印psram和ram的使用情况，帮助用户了解内存使用情况和剩余空间，方便调试和优化
  if (psramFound())
  {
    Serial.printf("PSRAM使用情况: 已使用 %d KB, 剩余 %d KB\n", (ESP.getPsramSize() - ESP.getFreePsram()) / 1024, ESP.getFreePsram() / 1024);
  }
  // 输出ram的使用情况，帮助用户了解ram的使用情况和剩余空间，方便调试和优化
  Serial.printf("RAM使用情况: 已使用 %d KB, 剩余 %d KB\n", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getFreeHeap() / 1024);

#ifdef CONFIG_CAMERA_MODULE_SWROOM_BABBLE_S3 // Set IR emitter strength to
                                             // 100%.
  const int ledPin = 1;                      // Replace this with a command endpoint eventually.
  const int freq = 5000;
  const int ledChannel = 0;
  const int resolution = 8;
  const int dutyCycle = 255;
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(1, ledChannel);
  ledcWrite(ledChannel, dutyCycle);
#endif

#ifndef SIM_ENABLED
  deviceConfig.attach(cameraHandler);
#endif // SIM_ENABLED
  deviceConfig.load();

  serialManager.init();

#ifndef ETVR_EYE_TRACKER_USB_API
  etvr_eye_tracker_web_init();
#else  // ETVR_EYE_TRACKER_WEB_API
  WiFi.disconnect(true);
#endif // ETVR_EYE_TRACKER_WEB_API
}

void loop()
{
  ledManager.handleLED();
  serialManager.run();

  if (isInAPMode)
  {
    dnsServer.processNextRequest();
  }

  // ====================== 调试：监控Web日志状态以及内存使用情况 ======================
  if (enableDetailedLog) // 如果启用了日志详细输出功能，就在loop中定期输出Web日志状态和内存使用情况，帮助用户了解系统状态和调试问题
  {
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastDebugTime > 10000) // 每10秒输出一次
    {
      lastDebugTime = currentTime;

      // 准备调试信息
      char debugInfo[256];

      snprintf(debugInfo, sizeof(debugInfo),
               "[DEBUG_LOOP] logServer=%p, events=%p, logCallback已设置\n",
               logServer, events);
      Serial.print(debugInfo);
      SendLogToWeb(debugInfo); // 发送到网页

      snprintf(debugInfo, sizeof(debugInfo),
               "[DEBUG_LOOP] Serial.available()=%d\n",
               Serial.available());
      Serial.print(debugInfo);
      SendLogToWeb(debugInfo);

      if (enableCallbackLog)
      {
        snprintf(debugInfo, sizeof(debugInfo),
                 "[DEBUG_LOOP] logCallback调用次数: %lu\n",
                 logCallbackCount);
        Serial.print(debugInfo);
        SendLogToWeb(debugInfo);
      }

      // PSRAM 使用情况
      if (psramFound())
      {
        snprintf(debugInfo, sizeof(debugInfo),
                 "PSRAM使用情况: 已使用 %d KB, 剩余 %d KB\n",
                 (ESP.getPsramSize() - ESP.getFreePsram()) / 1024,
                 ESP.getFreePsram() / 1024);
        Serial.print(debugInfo);
        SendLogToWeb(debugInfo);
      }

      // RAM 使用情况
      snprintf(debugInfo, sizeof(debugInfo),
               "RAM使用情况: 已使用 %d KB, 剩余 %d KB\n",
               (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024,
               ESP.getFreeHeap() / 1024);
      Serial.print(debugInfo);
      SendLogToWeb(debugInfo);
    }
  }
}