#include "wifihandler.hpp"
#include <WiFi.h>
#include "data/StateManager/StateManager.hpp"
#include "data/utilities/helpers.hpp"
#include "data/utilities/log_manager.hpp"

WiFiHandler::WiFiHandler(ProjectConfig& configManager,
                         const std::string& ssid,
                         const std::string& password,
                         uint8_t channel,
                         bool enable_adhoc)
    : configManager(configManager),
      ssid(std::move(ssid)),
      password(std::move(password)),
      channel(channel),
      power(0),
      _enable_adhoc(enable_adhoc) {}

WiFiHandler::~WiFiHandler() {}

void WiFiHandler::begin() {

  // just to be sure, we reeset everything before we do anything, some boards were having problems otherwise
  WiFi.disconnect();
  // we purposefully set the lowest min required security level, some boards have problems connecting otherwise
  // https://github.com/espressif/arduino-esp32/issues/8770
  WiFi.setMinSecurity(WIFI_AUTH_WEP);

  GLOG_I("WIFI", "Starting WiFi Handler");
  if (this->_enable_adhoc ||
      wifiStateManager.getCurrentState() == WiFiState_e::WiFiState_ADHOC) {
    GLOG_D("WIFI", "ADHOC is enabled, setting up ADHOC network");
    this->setUpADHOC();
    return;
  }

  GLOG_D("WIFI", "ADHOC is disabled, setting up STA network and checking transmission power");
  auto txpower = configManager.getWiFiTxPowerConfig();
  GLOG_D("WIFI", "Setting Wifi Power to: %d", txpower.power);
  GLOG_D("WIFI", "Setting WiFi sleep mode to NONE");
  WiFi.setSleep(false);

  GLOG_I("WIFI", "Initializing connection to wifi");
  wifiStateManager.setState(WiFiState_e::WiFiState_Connecting);

  auto networks = configManager.getWifiConfigs();

  if (networks.empty()) {
    GLOG_I("WIFI", "No networks found in config, trying the default one");
    
    if (this->iniSTA(
          this->ssid,
          this->password,
          this->channel,
          (wifi_power_t)txpower.power
        )
    ) {
      return;
    }

    GLOG_I("WIFI",
        "Could not connect to the hardcoded network, setting up ADHOC "
        "network");
    this->setUpADHOC();
    return;
  }

  for (auto& network : networks) {
    GLOG_I("WIFI", "Trying to connect to network: %s", network.ssid.c_str());
    if (this->iniSTA(network.ssid, network.password, network.channel,
                     (wifi_power_t)network.power)) {
      return;
    }
  }

  // at this point, we've tried every network, let's just setup adhoc
  GLOG_I("WIFI",
      "We've gone through every network, each timed out. Trying to connect "
      "to hardcoded network: %s",
      this->ssid.c_str());
  if (this->iniSTA(this->ssid, this->password, this->channel,
                   (wifi_power_t)txpower.power)) {
    GLOG_I("WIFI", "Successfully connected to the hardcoded network.");
    return;
  }

  GLOG_I("WIFI",
      "Could not connect to the hardcoded network, setting up adhoc. "
      "");
  this->setUpADHOC();
}

void WiFiHandler::adhoc(const std::string& ssid,
                        uint8_t channel,
                        const std::string& password) {
  wifiStateManager.setState(WiFiState_e::WiFiState_ADHOC);

  GLOG_I("WIFI", "Configuring access point...");
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(WIFI_PS_NONE);
  GLOG_I("WIFI", "Starting AP");
  IPAddress IP = WiFi.softAPIP();
  GLOG_I("WIFI", "AP IP address: %s", IP.toString().c_str());
  // You can remove the password parameter if you want the AP to be open.
  ProjectConfig::WiFiTxPower_t txpower = configManager.getWiFiTxPowerConfig();
  WiFi.softAP(ssid.c_str(), password.c_str(),
              channel);  // AP mode with password
  WiFi.setTxPower((wifi_power_t)txpower.power);
}

void WiFiHandler::setUpADHOC() {
  //添加来自main函数的AP设置参数，以确保当用户启用ADHOC模式时能够正确设置AP
  extern const char* ADHOC_AP_SSID;
  extern const char* ADHOC_AP_PASSWORD;
  extern const uint8_t ADHOC_AP_CHANNEL;

  GLOG_I("WIFI", "Setting Up Access Point...");
  size_t ssidLen = configManager.getAPWifiConfig().ssid.length();
  size_t passwordLen = configManager.getAPWifiConfig().password.length();
  if (ssidLen <= 0) {
    GLOG_I("WIFI", "Configuring access point with default values");
    this->adhoc(ADHOC_AP_SSID, ADHOC_AP_CHANNEL, ADHOC_AP_PASSWORD);
    return;
  }

  if (passwordLen <= 0) {
    GLOG_I("WIFI", "Configuring access point without a password");
    this->adhoc(configManager.getAPWifiConfig().ssid,
                configManager.getAPWifiConfig().channel);
    return;
  }

  this->adhoc(configManager.getAPWifiConfig().ssid,
              configManager.getAPWifiConfig().channel,
              configManager.getAPWifiConfig().password);

  GLOG_I("WIFI", "Configuring access point...");
  GLOG_D("WIFI", "ssid: %s", configManager.getAPWifiConfig().ssid.c_str());
  GLOG_D("WIFI", "password: %s",
        configManager.getAPWifiConfig().password.c_str());
  GLOG_D("WIFI", "channel: %d", configManager.getAPWifiConfig().channel);
}

bool WiFiHandler::iniSTA(const std::string& ssid,
                         const std::string& password,
                         uint8_t channel,
                         wifi_power_t power) {
  
  // since networks may not have a password, we only need to check if we have an ssid
  // bail if we don't  
  if (ssid == ""){
    GLOG_D("WIFI", "ssid missing, bailing");
    return false;
  }

  unsigned long currentMillis = millis();
  unsigned long startingMillis = currentMillis;
  int connectionTimeout = 45000;  // 30 seconds
  int progress = 0;

  wifiStateManager.setState(WiFiState_e::WiFiState_Connecting);
  GLOG_I("WIFI", "Trying to connect to: %s", ssid.c_str());
  auto mdnsConfig = configManager.getMDNSConfig();
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE,
              INADDR_NONE);  // need to call before setting hostname
  GLOG_D("WIFI", "Setting hostname %s", mdnsConfig.hostname.c_str());
  GLOG_I("WIFI", "Setting TX power to: %d", (uint8_t)power);
  WiFi.setTxPower(power); // https://github.com/espressif/arduino-esp32/issues/5698
  WiFi.begin(ssid.c_str(), password.c_str(), channel);

  GLOG_D("WIFI", "Waiting for WiFi to connect...");
  while (WiFi.status() != WL_CONNECTED) {
    progress++;
    currentMillis = millis();
    GLOG_I("WIFI", ".");
    GLOG_D("WIFI", "Progress: %d", progress);
    if ((currentMillis - startingMillis) >= connectionTimeout) {
      wifiStateManager.setState(WiFiState_e::WiFiState_Error);
      GLOG_E("WIFI", "Connection to: %s TIMEOUT", ssid.c_str());
      return false;
    }
  }
  wifiStateManager.setState(WiFiState_e::WiFiState_Connected);
  GLOG_I("WIFI", "Successfully connected to %s", ssid.c_str());
  return true;
}

void WiFiHandler::update(ConfigState_e event) {
  switch (event) {
    case ConfigState_e::networksConfigUpdated:
      this->begin();
      break;
    default:
      break;
  }
}

std::string WiFiHandler::getName() {
  return "WiFiHandler";
}
