#include <cstdio>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "nvs_flash.h"

#include <openiris_logo.hpp>
#include <wifiManager.hpp>
#include <ProjectConfig.hpp>
#include <StateManager.hpp>
#include <LEDManager.hpp>
#include <MDNSManager.hpp>
#include <CameraManager.hpp>
#include <WebSocketLogger.hpp>
#include <StreamServer.hpp>
#include <CommandManager.hpp>
#include <SerialManager.hpp>
#include <RestAPI.hpp>
#include <main_globals.hpp>
#include <MonitoringManager.hpp>

#ifdef CONFIG_GENERAL_INCLUDE_UVC_MODE
#include <UVCStream.hpp>
#endif

// defines to configure nlohmann-json for esp32
#define JSON_NO_IO 1
#define JSON_NOEXCEPTION 1

#ifdef CONFIG_LED_DEBUG_ENABLE
#define BLINK_GPIO (gpio_num_t) CONFIG_LED_DEBUG_GPIO
#else
// Use an invalid / unused GPIO when debug LED disabled to avoid accidental toggles
#define BLINK_GPIO (gpio_num_t) - 1
#endif
#define CONFIG_LED_C_PIN_GPIO (gpio_num_t) CONFIG_LED_EXTERNAL_GPIO

TaskHandle_t serialManagerHandle;

esp_timer_handle_t timerHandle = nullptr;
QueueHandle_t eventQueue = xQueueCreate(10, sizeof(SystemEvent));
QueueHandle_t ledStateQueue = xQueueCreate(10, sizeof(uint32_t));
QueueHandle_t cdcMessageQueue = xQueueCreate(3, sizeof(cdc_command_packet_t));

auto *stateManager = new StateManager(eventQueue, ledStateQueue);
auto dependencyRegistry = std::make_shared<DependencyRegistry>();
auto commandManager = std::make_shared<CommandManager>(dependencyRegistry);

WebSocketLogger webSocketLogger;
Preferences preferences;

std::shared_ptr<ProjectConfig> deviceConfig = std::make_shared<ProjectConfig>(&preferences);
auto wifiManager = std::make_shared<WiFiManager>(deviceConfig, eventQueue, stateManager);
MDNSManager mdnsManager(deviceConfig, eventQueue);

std::shared_ptr<CameraManager> cameraHandler = std::make_shared<CameraManager>(deviceConfig, eventQueue);
StreamServer streamServer(80, stateManager);

auto *restAPI = new RestAPI("http://0.0.0.0:81", commandManager);

#ifdef CONFIG_GENERAL_INCLUDE_UVC_MODE
UVCStreamManager uvcStream;
#endif

auto ledManager = std::make_shared<LEDManager>(BLINK_GPIO, CONFIG_LED_C_PIN_GPIO, ledStateQueue, deviceConfig);
std::shared_ptr<MonitoringManager> monitoringManager = std::make_shared<MonitoringManager>();
auto *serialManager = new SerialManager(commandManager, &timerHandle);

void startWiFiMode();
void startWiredMode(bool shouldCloseSerialManager);

static void initNVSStorage()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

int websocket_logger(const char *format, va_list args)
{
    webSocketLogger.log_message(format, args);
    return vprintf(format, args);
}

void launch_streaming()
{
    // Note, when switching and later right away activating UVC mode when we were previously in WiFi or Setup mode, the WiFi
    // utilities will still be running since we've launched them with startAutoMode() -> startWiFiMode()
    // we could add detection of this case, but it's probably not worth it since the next start of the device literally won't launch them
    // and we're telling folks to just reboot the device anyway
    // same case goes for when switching from UVC to WiFi

    StreamingMode deviceMode = deviceConfig->getDeviceMode();
    // if we've changed the mode from setup to something else, we can clean up serial manager
    // either the API endpoints or CDC will take care of further configuration
    if (deviceMode == StreamingMode::WIFI)
    {
        startWiFiMode();
    }
    else if (deviceMode == StreamingMode::UVC)
    {
        startWiredMode(true);
    }
    else if (deviceMode == StreamingMode::SETUP)
    {
        // we're still in setup, the user didn't select anything yet, let's give a bit of time for them to make a choice
        ESP_LOGI("[MAIN]", "No mode was selected, staying in SETUP mode. WiFi streaming will be enabled still. \nPlease select another mode if you'd like.");
    }
    else
    {
        ESP_LOGI("[MAIN]", "Unknown device mode: %d", (int)deviceMode);
    }
}

// Callback for automatic startup after delay
void startup_timer_callback(void *arg)
{
    ESP_LOGI("[MAIN]", "Startup timer fired, startupCommandReceived=%s, startupPaused=%s",
             getStartupCommandReceived() ? "true" : "false",
             getStartupPaused() ? "true" : "false");

    if (!getStartupCommandReceived() && !getStartupPaused())
    {
        launch_streaming();
    }
    else
    {
        if (getStartupPaused())
        {
            ESP_LOGI("[MAIN]", "Startup paused, staying in heartbeat mode");
        }
        else
        {
            ESP_LOGI("[MAIN]", "Command received during startup, staying in heartbeat mode");
        }
    }

    // Delete the timer after it fires
    esp_timer_delete(timerHandle);
    timerHandle = nullptr;
}

// Manual streaming activation
// We'll clean up the timer and handle streaming setup if called by a command
void force_activate_streaming()
{
    // Delete the timer before it fires
    // since we've got called manually
    esp_timer_delete(timerHandle);
    timerHandle = nullptr;
    launch_streaming();
}

void startWiredMode(bool shouldCloseSerialManager)
{
#ifndef CONFIG_GENERAL_INCLUDE_UVC_MODE
    ESP_LOGE("[MAIN]", "UVC mode selected but the board likely does not support it.");
    ESP_LOGI("[MAIN]", "Falling back to WiFi mode if credentials available");
    deviceMode = StreamingMode::WIFI;
    startWiFiMode();
#else
    ESP_LOGI("[MAIN]", "Starting UVC streaming mode.");
    if (shouldCloseSerialManager)
    {
        ESP_LOGI("[MAIN]", "Closing serial manager task.");
        vTaskDelete(serialManagerHandle);
    }

    ESP_LOGI("[MAIN]", "Shutting down serial manager, CDC will take over in a bit.");
    serialManager->shutdown();

    ESP_LOGI("[MAIN]", "Serial driver uninstalled");
    // Leaving a small gap for the host to see COM disappear
    vTaskDelay(pdMS_TO_TICKS(200));
    setUsbHandoverDone(true);

    ESP_LOGI("[MAIN]", "Setting up UVC Streamer");

    esp_err_t ret = uvcStream.setup();
    if (ret != ESP_OK)
    {
        ESP_LOGE("[MAIN]", "Failed to initialize UVC: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI("[MAIN]", "Starting CDC Serial Manager Task");
    xTaskCreate(
        HandleCDCSerialManagerTask,
        "HandleCDCSerialManagerTask",
        1024 * 6,
        commandManager.get(),
        1,
        nullptr);

    ESP_LOGI("[MAIN]", "Starting UVC streaming");

    uvcStream.start();
    ESP_LOGI("[MAIN]", "UVC streaming started");
#endif
}

void startWiFiMode()
{
    ESP_LOGI("[MAIN]", "Starting WiFi streaming mode.");
#ifdef CONFIG_GENERAL_ENABLE_WIRELESS
    wifiManager->Begin();
    mdnsManager.start();
    restAPI->begin();
    StreamingMode mode = deviceConfig->getDeviceMode();
    if (mode == StreamingMode::WIFI)
    {
        streamServer.startStreamServer();
    }
    xTaskCreate(
        HandleRestAPIPollTask,
        "HandleRestAPIPollTask",
        1024 * 2,
        restAPI,
        1, // it's the rest API, we only serve commands over it so we don't really need a higher priority
        nullptr);
#else
    ESP_LOGW("[MAIN]", "Wireless is disabled by configuration; skipping WiFi/mDNS/REST startup.");
#endif
}

void startSetupMode()
{
    // If we're in SETUP mode - Device starts with a 20-second delay before deciding on what to do
    // during this time we await any commands
    const uint64_t startup_delay_s = CONFIG_GENERAL_STARTUP_DELAY;
    ESP_LOGI("[MAIN]", "=====================================");
    ESP_LOGI("[MAIN]", "STARTUP: %llu-SECOND DELAY MODE ACTIVE", (unsigned long long)startup_delay_s);
    ESP_LOGI("[MAIN]", "=====================================");
    ESP_LOGI("[MAIN]", "Device will wait %llu seconds for commands...", (unsigned long long)startup_delay_s);

    // Create a one-shot timer for 20 seconds
    const esp_timer_create_args_t startup_timer_args = {
        .callback = &startup_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "startup_timer",
        .skip_unhandled_events = false};

    ESP_ERROR_CHECK(esp_timer_create(&startup_timer_args, &timerHandle));
    ESP_ERROR_CHECK(esp_timer_start_once(timerHandle, startup_delay_s * 1000000));
    ESP_LOGI("[MAIN]", "Started %llu-second startup timer", (unsigned long long)startup_delay_s);
    ESP_LOGI("[MAIN]", "Send any command within %llu seconds to enter heartbeat mode", (unsigned long long)startup_delay_s);
}

extern "C" void app_main(void)
{
    dependencyRegistry->registerService<ProjectConfig>(DependencyType::project_config, deviceConfig);
    dependencyRegistry->registerService<CameraManager>(DependencyType::camera_manager, cameraHandler);
    // Register WiFiManager only when wireless is enabled to avoid exposing WiFi commands in no-wireless builds
#ifdef CONFIG_GENERAL_ENABLE_WIRELESS
    dependencyRegistry->registerService<WiFiManager>(DependencyType::wifi_manager, wifiManager);
#endif
    dependencyRegistry->registerService<LEDManager>(DependencyType::led_manager, ledManager);
    dependencyRegistry->registerService<MonitoringManager>(DependencyType::monitoring_manager, monitoringManager);

    // add endpoint to check firmware version
    // setup CI and building for other boards

    // esp_log_set_vprintf(&websocket_logger);
    Logo::printASCII();
    initNVSStorage();
    deviceConfig->load();
    ledManager->setup();
    monitoringManager->setup();
    monitoringManager->start();

    xTaskCreate(
        HandleStateManagerTask,
        "HandleStateManagerTask",
        1024 * 2,
        stateManager,
        3,
        nullptr // it's fine for us not get a handle back, we don't need it
    );

    xTaskCreate(
        HandleLEDDisplayTask,
        "HandleLEDDisplayTask",
        1024 * 2,
        ledManager.get(),
        3,
        nullptr);

    cameraHandler->setupCamera();

    // let's keep the serial manager running for the duration of the setup
    // we'll clean it up later if need be
    serialManager->setup();
    xTaskCreate(
        HandleSerialManagerTask,
        "HandleSerialManagerTask",
        1024 * 6,
        serialManager,
        1,
        &serialManagerHandle);

    StreamingMode mode = deviceConfig->getDeviceMode();
    if (mode == StreamingMode::UVC)
    {
        // in UVC mode we only need to start the bare essentials for UVC
        // we don't need any wireless communication, we can shut it down

        // todo this would be the perfect place to introduce random delays
        // to workaround windows usb bug
        startWiredMode(true);
    }
    else if (mode == StreamingMode::WIFI)
    {
        // in Wifi mode we only need the wireless communication stuff, but the serial manager has to remain alive
        startWiFiMode();
    }
    else
    {
        // since we're in setup mode, we have to have wireless functionality on,
        // so we can do wifi scanning, test connection etc
        startWiFiMode();
        startSetupMode();
    }
}