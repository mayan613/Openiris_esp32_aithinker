#include <DNSServer.h>
#include <WiFi.h>
#include <openiris.hpp>
#include "html_content.h" // 包含HTML内容的头文件，用户可以在这个文件中修改HTML内容来定制配网页面

// 自定义代码区
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

AsyncWebServer *configServer = nullptr; // 配置专用服务器
AsyncWebServer *logServer = nullptr;    // 日志专用服务器
// SSE 响应对象（用于实时推送日志）
AsyncEventSource *events = nullptr;
bool isInAPMode = false;

// 用户自定义配置修改区域
// 在我们的功能下的AP的SSID和密码配置，之所以说在我们下是因为官方项目有类似于AP配网的功能，会和我们冲突，到时候他们的那些参数我会在下面定义并使用，但用户不需要修改那些参数，用户只需要修改下面的AP_SSID和AP_PASSWORD即可（除非你使用官方的AP功能，也就是adhoc）
const char *AP_SSID = "Galeros_ESP32";     // 这个是当设备进入AP模式时的WiFi名称，用户可以修改为自己喜欢的名字
const char *AP_PASSWORD = "Galeros_ESP32"; // 这个是当设备进入AP模式时的WiFi密码，建议设置一个比较复杂的密码以防止他人连接你的设备，如果你不想设置密码，可以将这个参数设置为一个空字符串""，但不建议这样做，因为这会让任何人都能连接你的设备，存在安全风险

// 新增：配置管理端口
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
void setupWebLogServer()
{
  delay(1000); // 确保WiFi连接稳定后再启动日志服务器
  if (logServer != nullptr)
    return;
  logServer = new AsyncWebServer(LOG_PORT); // 端口号在上面定义，用户可以修改

  // 日志主页
  logServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/html", web_log_html); });

  // SSE 实时日志推送
  events = new AsyncEventSource("/logs");
  logServer->addHandler(events);

  logServer->begin();

  log_d("[LOG] Web 日志服务器已启动 → http://设备IP:%d", LOG_PORT);
  log_d("[LOG] Web Log Server initialized on port %d", LOG_PORT);

  // ====================== 连接日志回调到 Web 日志服务器 ======================
  serialManager.setLogCallback([](const String &msg) {
      if (events != nullptr && msg.length() > 0) {
          String cleanMsg = msg;
          cleanMsg.trim();
          if (cleanMsg.length() > 0) {
              events->send(cleanMsg.c_str());
          }
      }
  });
}


void etvr_eye_tracker_web_init()
{
  log_d("[NETWORK]: Starting custom WiFi provisioning...");

  // 先断开 WiFi，避免 wifiHandler.begin() 使用默认配置干扰
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  log_d("[SETUP]: Starting Network Handler");
  deviceConfig.attach(mdnsHandler);
  log_d("[SETUP]: Starting WiFi Handler");
  wifiHandler.begin();
  // 自定义代码
  delay(1000);
  if (WiFi.isConnected())
  {
    log_d("[SETUP]: WiFi Connected! Starting main services...");
    log_d("[SETUP]: Starting MDNS Handler");
    mdnsHandler.startMDNS();
    log_d("[SETUP]: Starting Stream Server");
    streamServer.startStreamServer();
    log_d("[SETUP]: Starting Stream Server");
    apiServer.setup();

    // 后端管理窗口
    configServer = new AsyncWebServer(CONFIG_PORT);
    setupConfigServer(false);

    configServer->begin();

    log_d("Config page available at http://%s:%d",
          WiFi.localIP().toString().c_str(), CONFIG_PORT);

    // ==================== 延迟启动 Web 日志服务器 ====================
    static bool webLogServerStarted = false;

    if (!webLogServerStarted)
    {
      setupWebLogServer();
      webLogServerStarted = true;
    }

    return;
  }

  // WIFI未连接时

  log_d("[SETUP]: WiFi not connected, starting AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD); // ssid,password
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(DNS_PORT, "*", apIP);

  configServer = new AsyncWebServer(80);
  setupConfigServer(true); // ap模式
  configServer->begin();

  isInAPMode = true;
  log_d("AP Mode + Config Portal started! Connect to '%s'", AP_SSID);

  // 延迟启动日志服务器
  static bool webLogServerStarted = false;
  if (!webLogServerStarted)
  {
    setupWebLogServer();
    webLogServerStarted = true;
  }
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
    log_d("PSRAM使用情况: 已使用 %d KB, 剩余 %d KB", (ESP.getPsramSize() - ESP.getFreePsram()) / 1024, ESP.getFreePsram() / 1024);
  }
  // 输出ram的使用情况，帮助用户了解ram的使用情况和剩余空间，方便调试和优化
  log_d("RAM使用情况: 已使用 %d KB, 剩余 %d KB", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getFreeHeap() / 1024);

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
  // 移除这里的setLogCallback，移到Web服务器启动后


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
  // psram和ram使用情况日志输出（每5秒输出一次，帮助用户了解内存使用情况，方便调试和优化）
  // 注意：频繁地输出日志可能会增加CPU占用，影响性能，所以我们设置为每5秒输出一次，如果你需要更频繁的日志输出，可以将这个时间间隔调整为更短的时间，但请注意可能会对性能产生影响
  // 仅供调试使用，正式使用时建议注释掉这些日志输出以减少CPU占用和日志干扰，防止delay函数导致正常功能的延迟，如果你需要持续监控内存使用情况，建议使用专业的监控工具或者在特定的调试阶段启用这些日志输出。
  // delay(5000);  // 减少CPU占用，避免过度频繁地处理LED和串口任务
  // //添加函数不断读取psram状态并打印日志，帮助用户了解psram的使用情况和剩余空间，方便调试和优化
  // if(psramFound()) {
  //   log_d("PSRAM使用情况: 已使用 %d KB, 剩余 %d KB", (ESP.getPsramSize() - ESP.getFreePsram()) / 1024, ESP.getFreePsram() / 1024);
  // }
  // //输出ram的使用情况，帮助用户了解ram的使用情况和剩余空间，方便调试和优化
  // log_d("RAM使用情况: 已使用 %d KB, 剩余 %d KB", (ESP.getHeapSize() - ESP.getFreeHeap()) / 1024, ESP.getFreeHeap() / 1024);
}
