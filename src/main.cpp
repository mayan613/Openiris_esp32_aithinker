#include <DNSServer.h>
#include <WiFi.h>
#include <openiris.hpp>
#include <vector>

// 自定义代码区
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

AsyncWebServer* configServer = nullptr;  // 配置专用服务器
bool isInAPMode = false;

// 新增：配置管理端口
const uint16_t CONFIG_PORT = 8080;  // ← 这里修改

const char ap_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>OpenIris 配网</title><meta name="viewport" content="width=device-width,initial-scale=1"></head>
<body style="font-family:Arial;text-align:center;padding:40px;">
  <h1>OpenIris 首次配网</h1>
  <p>请输入你的家用 WiFi</p>
  <p>由彩咖工作室(Galeros Studio)提供支持</p>
  <form action="/save" method="POST">
    SSID: <input type="text" name="ssid" style="width:280px" required><br><br>
    Password: <input type="password" name="password" style="width:280px"><br><br>
    <input type="submit" value="保存并重启" style="padding:12px 30px;font-size:18px;">
  </form>
</body>
</html>
)rawliteral";

// 已连接WiFi时的管理页面（显示当前网络 + 修改按钮）
const char sta_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>OpenIris 配置</title><meta name="viewport" content="width=device-width,initial-scale=1"></head>
<body style="font-family:Arial;text-align:center;padding:40px;">
  <h1>OpenIris WiFi 配置</h1>
  <p><b>当前已连接：</b> %CURRENT_SSID%</p>
  <p>由彩咖工作室(Galeros Studio)提供支持</p>
  <hr>
  <h2>修改 WiFi 网络</h2>
  <form action="/save" method="POST">
    新 SSID: <input type="text" name="ssid" style="width:280px" required><br><br>
    新 Password: <input type="password" name="password" style="width:280px"><br><br>
    <input type="submit" value="修改并重启" style="padding:12px 30px;font-size:18px;">
  </form>
  <p>修改后设备将重启并尝试连接新网络</p>
</body>
</html>
)rawliteral";

// 自定义代码区
class CaptiveRequestHandler : public AsyncWebHandler {
 public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest* request) { return true; }

  void handleRequest(AsyncWebServerRequest* request) {
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
#endif  // ESP32S3_XIAO_SENSE

#ifndef SIM_ENABLED
CameraHandler cameraHandler(deviceConfig);
#endif  // SIM_ENABLED

#ifndef ETVR_EYE_TRACKER_USB_API
WiFiHandler wifiHandler(deviceConfig,
                        WIFI_SSID,
                        WIFI_PASSWORD,
                        WIFI_CHANNEL,
                        ENABLE_ADHOC);
MDNSHandler mdnsHandler(deviceConfig);
#ifdef SIM_ENABLED
APIServer apiServer(deviceConfig, wifiStateManager, "/control");
#else
APIServer apiServer(deviceConfig, cameraHandler, "/control");
StreamServer streamServer;
#endif  // SIM_ENABLED

// 自定义核心处理逻辑
void setupConfigServer(bool isAP) {
  if (!configServer)
    return;

  if (isAP) {
    // AP模式：显示配网页面
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(200, "text/html, charset=utf-8", ap_config_html);
    });
  } else {
    // STA模式（已连接）：显示当前网络 + 修改页面
    configServer->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      String html = sta_config_html;
      String currentSSID = WiFi.SSID();
      if (currentSSID.length() == 0)
        currentSSID = "未知";
      html.replace("%CURRENT_SSID%", currentSSID);
      request->send(200, "text/html, charset=utf-8", html);
    });
  }

  // 保存逻辑（两种模式共用）
  configServer->on("/save", HTTP_POST, [](AsyncWebServerRequest* request) {
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
    request->send(400, "text/plain", "参数错误");
  });

  // AP模式下启用 Captive Portal
  if (isAP) {
    configServer->addHandler(new CaptiveRequestHandler())
        .setFilter(ON_AP_FILTER);
  }
}

void etvr_eye_tracker_web_init() {
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
  if (WiFi.isConnected()) {
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

    return;
  }

  // WIFI未连接时

  log_d("[SETUP]: WiFi not connected, starting AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Galeros_ESP32", "Galeros_ESP32");  // ssid,password
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(DNS_PORT, "*", apIP);

  configServer = new AsyncWebServer(80);
  setupConfigServer(true);  // ap模式
  configServer->begin();

  isInAPMode = true;
  log_d("AP Mode + Config Portal started! Connect to 'Galeros_ESP32'");

  //   log_d("[SETUP]: Starting MDNS Handler");
  //   mdnsHandler.startMDNS();

  //   switch (wifiStateManager.getCurrentState()) {
  //     case WiFiState_e::WiFiState_Disconnected: {
  //       //! TODO: Implement
  //       break;
  //     }
  //     case WiFiState_e::WiFiState_ADHOC: {
  // #ifndef SIM_ENABLED
  //       log_d("[SETUP]: Starting Stream Server");
  //       streamServer.startStreamServer();
  // #endif  // SIM_ENABLED
  //       log_d("[SETUP]: Starting API Server");
  //       apiServer.setup();
  //       break;
  //     }
  //     case WiFiState_e::WiFiState_Connected: {
  // #ifndef SIM_ENABLED
  //       log_d("[SETUP]: Starting Stream Server");
  //       streamServer.startStreamServer();
  // #endif  // SIM_ENABLED
  //       log_d("[SETUP]: Starting API Server");
  //       apiServer.setup();
  //       break;
  //     }
  //     case WiFiState_e::WiFiState_Connecting: {
  //       //! TODO: Implement
  //       break;
  //     }
  //     case WiFiState_e::WiFiState_Error: {
  //       //! TODO: Implement
  //       break;
  //     }
}
#endif  // ETVR_EYE_TRACKER_WEB_API

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  Logo::printASCII();
  ledManager.begin();

#ifdef CONFIG_CAMERA_MODULE_SWROOM_BABBLE_S3  // Set IR emitter strength to
                                              // 100%.
  const int ledPin = 1;  // Replace this with a command endpoint eventually.
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
#endif  // SIM_ENABLED
  deviceConfig.load();

  serialManager.init();

#ifndef ETVR_EYE_TRACKER_USB_API
  etvr_eye_tracker_web_init();
#else   // ETVR_EYE_TRACKER_WEB_API
  WiFi.disconnect(true);
#endif  // ETVR_EYE_TRACKER_WEB_API
}

void loop() {
  ledManager.handleLED();
  serialManager.run();

  if (isInAPMode) {
    dnsServer.processNextRequest();
  }
}
