#include "network_utilities.hpp"
#include "data/utilities/log_manager.hpp"

void Network_Utilities::setupWifiScan()
{
    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Disconnect from the access point if connected before
    my_delay(0.1);
    GLOG_I("NET", "Setup done");
}

bool Network_Utilities::loopWifiScan()
{
    // WiFi.scanNetworks will return the number of networks found
    GLOG_I("NET", "[INFO]: Beginning WiFi Scanner");
    int networks = WiFi.scanNetworks(true, true);
    GLOG_I("NET", "[INFO]: scan done");
    GLOG_I("NET", "%d networks found", networks);
    for (int i = networks; i--;)
    {
        // Print SSID and RSSI for each network found
        //! TODO: Add method here to interface with the API and forward the scanned networks to the API
        GLOG_I("NET", "%d: %s (%d) %s", i - 1, WiFi.SSID(i), WiFi.RSSI(i), (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
        my_delay(0.02L); // delay 20ms
    }
    return (networks > 0);
}

// Take measurements of the Wi-Fi strength and return the average result.
int Network_Utilities::getStrength(int points) // TODO: add to JSON doc
{
    int32_t rssi = 0, averageRSSI = 0;
    for (int i = 0; i < points; i++)
    {
        rssi += WiFi.RSSI();
        my_delay(0.02L);
    }
    averageRSSI = rssi / points;
    return averageRSSI;
}

void Network_Utilities::my_delay(volatile long delay_time)
{
    delay_time = delay_time * 1e6L;
    for (volatile long count = delay_time; count > 0; count--)
        ;
}

// a function to generate the device ID
std::string Network_Utilities::generateDeviceID() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  GLOG_I("NET", "ESP Chip model = %s Rev %d", ESP.getChipModel(),
        ESP.getChipRevision());
  GLOG_I("NET", "This chip has %d cores", ESP.getChipCores());
  GLOG_I("NET", "Chip ID: %d", chipId);
  std::string deviceID = (const char*)chipId;
  return deviceID;
}

/**
 * @brief Function to map the WiFi status to the WiFiState_e enum
 *
 * @brief Call this function in the loop() function
 */
void Network_Utilities::checkWiFiState()
{
    if (wifiStateManager.getCurrentState() == WiFiState_e::WiFiState_ADHOC)
    {
        return;
    }

    switch (WiFi.status())
    {
        case wl_status_t::WL_IDLE_STATUS:
            wifiStateManager.setState(WiFiState_e::WiFiState_Idle);
            break;
        case wl_status_t::WL_NO_SSID_AVAIL:
            wifiStateManager.setState(WiFiState_e::WiFiState_Error);
            break;
        case wl_status_t::WL_SCAN_COMPLETED:
            wifiStateManager.setState(WiFiState_e::WiFiState_None);
            break;
        case wl_status_t::WL_CONNECTED:
            wifiStateManager.setState(WiFiState_e::WiFiState_Connected);
            break;
        case wl_status_t::WL_CONNECT_FAILED:
            wifiStateManager.setState(WiFiState_e::WiFiState_Error);
            break;
        case wl_status_t::WL_CONNECTION_LOST:
            wifiStateManager.setState(WiFiState_e::WiFiState_Disconnected);
            break;
        case wl_status_t::WL_DISCONNECTED:
            wifiStateManager.setState(WiFiState_e::WiFiState_Disconnected);
            break;
        default:
            wifiStateManager.setState(WiFiState_e::WiFiState_Disconnected);
    }
}

