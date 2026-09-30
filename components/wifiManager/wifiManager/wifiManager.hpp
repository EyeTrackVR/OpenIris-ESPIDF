#pragma once
#ifndef WIFIHANDLER_HPP
#define WIFIHANDLER_HPP

#include <ProjectConfig.hpp>
#include <StateManager.hpp>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "WiFiScanner.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY 5
// Spacing between reconnect attempts. Retrying with no gap burns the whole retry budget in
// under a second, which is not enough time for a busy or distant AP to answer.
#define WIFI_RETRY_BACKOFF_MS 300
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

namespace WiFiManagerHelpers
{
void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
}

class WiFiManager
{
   private:
    uint8_t channel;
    std::shared_ptr<ProjectConfig> deviceConfig;
    QueueHandle_t eventQueue;
    StateManager* stateManager;
    wifi_init_config_t _wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t _wifi_cfg = {};
    std::unique_ptr<WiFiScanner> wifiScanner;

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    int8_t power;

    void ApplyTxPower();
    void SetCredentials(const char* ssid, const std::vector<uint8_t> bssid, const char* password, bool use_bssid);
    void ConnectWithHardcodedCredentials();
    void ConnectWithStoredCredentials();
    void SetupAccessPoint();
    std::vector<uint8_t> ParseBSSID(std::string_view bssid_string);

   public:
    WiFiManager(std::shared_ptr<ProjectConfig> deviceConfig, QueueHandle_t eventQueue, StateManager* stateManager);
    void Begin();
    std::vector<WiFiNetwork> ScanNetworks(int timeout_ms = 15000);
    WiFiState_e GetCurrentWiFiState();
    void TryConnectToStoredNetworks();
};

#endif