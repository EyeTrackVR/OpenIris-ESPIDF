#include "wifiManager.hpp"

static auto WIFI_MANAGER_TAG = "[WIFI_MANAGER]";

int s_retry_num = 0;
EventGroupHandle_t s_wifi_event_group;

void WiFiManagerHelpers::event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
  ESP_LOGI(WIFI_MANAGER_TAG, "Trying to connect, got event: %d", (int)event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    if (const auto err = esp_wifi_connect(); err != ESP_OK)
    {
      ESP_LOGI(WIFI_MANAGER_TAG, "esp_wifi_connect() failed: %s", esp_err_to_name(err));
    }
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    const auto *disconnected = static_cast<wifi_event_sta_disconnected_t *>(event_data);
    ESP_LOGI(WIFI_MANAGER_TAG, "Disconnect reason: %d", disconnected->reason);

    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(WIFI_MANAGER_TAG, "retry to connect to the AP");
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(WIFI_MANAGER_TAG, "connect to the AP fail");
  }

  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    const auto *event = static_cast<ip_event_got_ip_t *>(event_data);
    ESP_LOGI(WIFI_MANAGER_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

WiFiManager::WiFiManager(std::shared_ptr<ProjectConfig> deviceConfig, QueueHandle_t eventQueue, StateManager *stateManager)
    : deviceConfig(deviceConfig), eventQueue(eventQueue), stateManager(stateManager), wifiScanner(std::make_unique<WiFiScanner>()) {}

void WiFiManager::SetCredentials(const char *ssid, const char *password)
{
  // Clear the config first
  memset(&_wifi_cfg, 0, sizeof(_wifi_cfg));

  // Copy SSID with null termination
  size_t ssid_len = std::min(strlen(ssid), sizeof(_wifi_cfg.sta.ssid) - 1);
  memcpy(_wifi_cfg.sta.ssid, ssid, ssid_len);
  _wifi_cfg.sta.ssid[ssid_len] = '\0';

  // Copy password with null termination
  size_t pass_len = std::min(strlen(password), sizeof(_wifi_cfg.sta.password) - 1);
  memcpy(_wifi_cfg.sta.password, password, pass_len);
  _wifi_cfg.sta.password[pass_len] = '\0';

  // Set other required fields
  // Use open auth if no password, otherwise allow any WPA variant
  if (strlen(password) == 0)
  {
    _wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
  }
  else
  {
    // IMPORTANT: Set threshold to WEP to accept ANY security mode >= WEP
    // This allows WPA, WPA2, WPA3, etc. The driver will negotiate the highest common mode
    _wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WEP;
  }

  // CRITICAL: Disable PMF completely - this often causes handshake timeouts
  _wifi_cfg.sta.pmf_cfg.capable = false;
  _wifi_cfg.sta.pmf_cfg.required = false;

  // OPTIMIZATION: Use fast scan instead of all channel scan for quicker connection
  _wifi_cfg.sta.scan_method = WIFI_FAST_SCAN;
  _wifi_cfg.sta.bssid_set = 0; // Don't use specific BSSID
  _wifi_cfg.sta.channel = 0;   // Auto channel detection

  // Additional settings that might help with compatibility
  _wifi_cfg.sta.listen_interval = 0;                     // Default listen interval
  _wifi_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL; // Connect to strongest signal

  // IMPORTANT: For WPA/WPA2 Personal networks
  _wifi_cfg.sta.threshold.rssi = -127;                  // Accept any signal strength
  _wifi_cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED; // Let driver decide SAE mode

  // Log what we're trying to connect to with detailed debugging
  ESP_LOGI(WIFI_MANAGER_TAG, "Setting credentials for SSID: '%s' (length: %d)", ssid, (int)strlen(ssid));
  ESP_LOGI(WIFI_MANAGER_TAG, "Password: '%s' (length: %d)", password, (int)strlen(password));
  ESP_LOGI(WIFI_MANAGER_TAG, "Auth mode: %d, PMF capable: %d",
           _wifi_cfg.sta.threshold.authmode, _wifi_cfg.sta.pmf_cfg.capable);
}

void WiFiManager::ConnectWithHardcodedCredentials()
{
  SystemEvent event = {EventSource::WIFI, WiFiState_e::WiFiState_ReadyToConnect};
  this->SetCredentials(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

  wifi_mode_t mode;
  if (esp_wifi_get_mode(&mode) == ESP_OK) {
    esp_wifi_stop();
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &_wifi_cfg));

  xQueueSend(this->eventQueue, &event, 10);
  esp_wifi_start();

  event.value = WiFiState_e::WiFiState_Connecting;
  xQueueSend(this->eventQueue, &event, 10);

  // Use shorter timeout for faster startup - 8 seconds should be enough for most networks
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         pdMS_TO_TICKS(8000));

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "connected to ap SSID:%p password:%p",
             _wifi_cfg.sta.ssid, _wifi_cfg.sta.password);

    event.value = WiFiState_e::WiFiState_Connected;
    xQueueSend(this->eventQueue, &event, 10);
  }

  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGE(WIFI_MANAGER_TAG, "Failed to connect to SSID:%p, password:%p",
             _wifi_cfg.sta.ssid, _wifi_cfg.sta.password);

    event.value = WiFiState_e::WiFiState_Error;
    xQueueSend(this->eventQueue, &event, 10);
  }
  else
  {
    ESP_LOGE(WIFI_MANAGER_TAG, "UNEXPECTED EVENT");
  }
}

void WiFiManager::ConnectWithStoredCredentials()
{
  SystemEvent event = {EventSource::WIFI, WiFiState_e::WiFiState_ReadyToConnect};

  auto const networks = this->deviceConfig->getWifiConfigs();

  if (networks.empty())
  {
    event.value = WiFiState_e::WiFiState_Disconnected;
    xQueueSend(this->eventQueue, &event, 10);
    ESP_LOGE(WIFI_MANAGER_TAG, "No networks stored, cannot connect");
    return;
  }

  // Stop WiFi once before the loop
  esp_wifi_stop();
  vTaskDelay(pdMS_TO_TICKS(100));

  // Ensure we're in STA mode
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  for (const auto &network : networks)
  {
    // Reset retry counter for each network attempt
    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT | WIFI_CONNECTED_BIT);
    this->SetCredentials(network.ssid.c_str(), network.password.c_str());

    // Update config without stopping WiFi again
    ESP_LOGI(WIFI_MANAGER_TAG, "Attempting to connect to SSID: '%s'", network.ssid.c_str());

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &_wifi_cfg));
    xQueueSend(this->eventQueue, &event, 10);

    // Start WiFi if not already started
    esp_err_t start_err = esp_wifi_start();
    if (start_err != ESP_OK && start_err != ESP_ERR_WIFI_STATE)
    {
      ESP_LOGE(WIFI_MANAGER_TAG, "Failed to start WiFi: %s", esp_err_to_name(start_err));
      continue;
    }

    event.value = WiFiState_e::WiFiState_Connecting;
    xQueueSend(this->eventQueue, &event, 10);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(10000)); // 10 second timeout for faster failover
    if (bits & WIFI_CONNECTED_BIT)
    {
      ESP_LOGI(WIFI_MANAGER_TAG, "connected to ap SSID:%s",
               network.ssid.c_str());

      event.value = WiFiState_e::WiFiState_Connected;
      xQueueSend(this->eventQueue, &event, 10);

      return;
    }
    ESP_LOGE(WIFI_MANAGER_TAG, "Failed to connect to SSID:%s, trying next stored network",
             network.ssid.c_str());

    // Disconnect before trying next network
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  event.value = WiFiState_e::WiFiState_Error;
  xQueueSend(this->eventQueue, &event, 10);
  ESP_LOGE(WIFI_MANAGER_TAG, "Failed to connect to all saved networks");
}

void WiFiManager::SetupAccessPoint()
{
  ESP_LOGI(WIFI_MANAGER_TAG, "Connection to stored credentials failed, starting AP");

  esp_netif_create_default_wifi_ap();
  wifi_init_config_t esp_wifi_ap_init_config = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&esp_wifi_ap_init_config));

  wifi_config_t ap_wifi_config = {
      .ap = {
          .ssid = CONFIG_WIFI_AP_SSID,
          .password = CONFIG_WIFI_AP_PASSWORD,
          .max_connection = 1,

      },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(WIFI_MANAGER_TAG, "AP started.");
}

std::vector<WiFiNetwork> WiFiManager::ScanNetworks(int timeout_ms)
{
  wifi_mode_t current_mode;
  esp_err_t err = esp_wifi_get_mode(&current_mode);

  if (err == ESP_ERR_WIFI_NOT_INIT)
  {
    ESP_LOGE(WIFI_MANAGER_TAG, "WiFi not initialized for scanning");
    return std::vector<WiFiNetwork>();
  }

  // If we're in AP-only mode, we need STA interface for scanning
  if (current_mode == WIFI_MODE_AP)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "AP mode detected, checking for STA interface");

    // Check if STA netif already exists
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    bool sta_netif_exists = (sta_netif != nullptr);

    if (!sta_netif_exists)
    {
      ESP_LOGI(WIFI_MANAGER_TAG, "Creating STA interface for scanning");
      sta_netif = esp_netif_create_default_wifi_sta();
    }

    ESP_LOGI(WIFI_MANAGER_TAG, "Switching to APSTA mode for scanning");
    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK)
    {
      ESP_LOGE(WIFI_MANAGER_TAG, "Failed to set APSTA mode: %s", esp_err_to_name(err));
      if (!sta_netif_exists && sta_netif)
      {
        esp_netif_destroy(sta_netif);
      }
      return std::vector<WiFiNetwork>();
    }

    // Configure STA with empty config to prevent auto-connect
    wifi_config_t empty_config = {};
    esp_wifi_set_config(WIFI_IF_STA, &empty_config);

    // Ensure STA is disconnected and not trying to connect
    esp_wifi_disconnect();
    // Longer delay for mode to stabilize and enable all channels
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Perform scan
    auto networks = wifiScanner->scanNetworks(timeout_ms);

    // Restore AP-only mode
    ESP_LOGI(WIFI_MANAGER_TAG, "Restoring AP-only mode");
    esp_wifi_set_mode(WIFI_MODE_AP);

    // Clean up STA interface if we created it
    if (!sta_netif_exists && sta_netif)
    {
      esp_netif_destroy(sta_netif);
    }

    return networks;
  }

  // If already in STA or APSTA mode, scan directly
  return wifiScanner->scanNetworks(timeout_ms);
}

WiFiState_e WiFiManager::GetCurrentWiFiState()
{
  return this->stateManager->GetWifiState();
}

void WiFiManager::TryConnectToStoredNetworks()
{
  ESP_LOGI(WIFI_MANAGER_TAG, "Manual WiFi connection attempt requested");

  // Check current WiFi mode
  wifi_mode_t current_mode;
  esp_err_t err = esp_wifi_get_mode(&current_mode);
  if (err != ESP_OK)
  {
    ESP_LOGE(WIFI_MANAGER_TAG, "Failed to get WiFi mode: %s", esp_err_to_name(err));
    return;
  }

  // If in AP mode, we need to properly transition to STA mode
  if (current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "Currently in AP mode, transitioning to STA mode");

    // Stop WiFi first
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Check if STA interface exists, create if needed
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr)
    {
      ESP_LOGI(WIFI_MANAGER_TAG, "Creating STA interface");
      sta_netif = esp_netif_create_default_wifi_sta();
    }

    // Set to STA mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Reset retry counter and clear all event bits
  s_retry_num = 0;
  xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT | WIFI_CONNECTED_BIT);
  this->ConnectWithStoredCredentials();
}

void WiFiManager::Begin()
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  auto netif = esp_netif_create_default_wifi_sta();

  wifi_init_config_t esp_wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&esp_wifi_init_config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &WiFiManagerHelpers::event_handler,
                                                      nullptr,
                                                      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                      IP_EVENT_STA_GOT_IP,
                                                      &WiFiManagerHelpers::event_handler,
                                                      nullptr,
                                                      &instance_got_ip));

  _wifi_cfg = {};
  _wifi_cfg.sta.threshold.authmode = WIFI_AUTH_OPEN; // Start with open, will be set properly by SetCredentials
  _wifi_cfg.sta.pmf_cfg.capable = false;             // Disable PMF by default
  _wifi_cfg.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

  ESP_LOGI(WIFI_MANAGER_TAG, "Beginning setup");
  const auto hasHardcodedCredentials = strlen(CONFIG_WIFI_SSID) > 0;
  if (hasHardcodedCredentials)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "Detected hardcoded credentials, trying them out");
    this->ConnectWithHardcodedCredentials();
  }

  if (this->stateManager->GetWifiState() != WiFiState_e::WiFiState_Connected || !hasHardcodedCredentials)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "Hardcoded credentials failed or missing, trying stored credentials");
    xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    this->ConnectWithStoredCredentials();
  }

  if (this->stateManager->GetWifiState() != WiFiState_e::WiFiState_Connected)
  {
    ESP_LOGI(WIFI_MANAGER_TAG, "Stored netoworks failed or hardcoded credentials missing, starting AP");
    xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    esp_netif_destroy(netif);
    this->SetupAccessPoint();
  }
}
