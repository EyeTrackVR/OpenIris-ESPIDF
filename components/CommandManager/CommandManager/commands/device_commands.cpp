#include "device_commands.hpp"
#include "LEDManager.hpp"
#include "MonitoringManager.hpp"
#include "esp_mac.h"
#include <cstdio>

CommandResult setDeviceModeCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{
    if (!json.contains("mode") || !json["mode"].is_number_integer())
    {
        return CommandResult::getErrorResult("Invalid payload - missing or unsupported mode");
    }

    const auto mode = json["mode"].get<int>();
    if (mode < 0 || mode > 2)
    {
        return CommandResult::getErrorResult("Invalid payload - unsupported mode");
    }

    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    projectConfig->setDeviceMode(static_cast<StreamingMode>(mode));

    return CommandResult::getSuccessResult("Device mode set");
}

CommandResult updateOTACredentialsCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{

    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    const auto oldDeviceConfig = projectConfig->getDeviceConfig();
    auto OTALogin = oldDeviceConfig.OTALogin;
    auto OTAPassword = oldDeviceConfig.OTAPassword;
    auto OTAPort = oldDeviceConfig.OTAPort;

    if (json.contains("login") && json["login"].is_string())
    {
        if (const auto newLogin = json["login"].get<std::string>(); strcmp(newLogin.c_str(), "") != 0)
        {
            OTALogin = newLogin;
        }
    }

    if (json.contains("password") && json["password"].is_string())
    {
        OTAPassword = json["password"].get<std::string>();
    }

    if (json.contains("port") && json["port"].is_number_integer())
    {
        if (const auto newPort = json["port"].get<int>(); newPort >= 82)
        {
            OTAPort = newPort;
        }
    }

    projectConfig->setOTAConfig(OTALogin, OTAPassword, OTAPort);
    return CommandResult::getSuccessResult("OTA Config set");
}

CommandResult updateLEDDutyCycleCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{
    if (!json.contains("dutyCycle") || !json["dutyCycle"].is_number_integer())
    {
        return CommandResult::getErrorResult("Invalid payload - missing dutyCycle");
    }

    const auto dutyCycle = json["dutyCycle"].get<int>();

    if (dutyCycle < 0 || dutyCycle > 100)
    {
        return CommandResult::getErrorResult("Invalid payload - unsupported dutyCycle");
    }

    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    projectConfig->setLEDDUtyCycleConfig(dutyCycle);

    // Try to apply the change live via LEDManager if available
    auto ledMgr = registry->resolve<LEDManager>(DependencyType::led_manager);
    if (ledMgr)
    {
        ledMgr->setExternalLEDDutyCycle(static_cast<uint8_t>(dutyCycle));
    }

    return CommandResult::getSuccessResult("LED duty cycle set");
}

CommandResult restartDeviceCommand()
{
    OpenIrisTasks::ScheduleRestart(2000);
    return CommandResult::getSuccessResult("Device restarted");
}

CommandResult getLEDDutyCycleCommand(std::shared_ptr<DependencyRegistry> registry)
{
    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    const auto deviceCfg = projectConfig->getDeviceConfig();
    int duty = deviceCfg.led_external_pwm_duty_cycle;
    const auto json = nlohmann::json{{"led_external_pwm_duty_cycle", duty}};
    return CommandResult::getSuccessResult(json);
}

CommandResult startStreamingCommand()
{
    // since we're trying to kill the serial handler
    // from *inside* the serial handler, we'd deadlock.
    // we can just pass nullptr to the vtaskdelete(),
    // but then we won't get any response, so we schedule a timer instead
    esp_timer_create_args_t args{
        .callback = activateStreaming,
        .arg = nullptr,
        .name = "activateStreaming"};

    esp_timer_handle_t activateStreamingTimer;
    esp_timer_create(&args, &activateStreamingTimer);
    esp_timer_start_once(activateStreamingTimer, pdMS_TO_TICKS(150));
    // streamServer.startStreamServer();
    return CommandResult::getSuccessResult("Streaming starting");
}

CommandResult switchModeCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{

    if (!json.contains("mode") || !json["mode"].is_string())
    {
        return CommandResult::getErrorResult("Invalid payload - missing mode");
    }

    auto modeStr = json["mode"].get<std::string>();
    StreamingMode newMode;

    ESP_LOGI("[DEVICE_COMMANDS]", "Switch mode command received with mode: %s", modeStr.c_str());

    if (modeStr == "uvc")
    {
        newMode = StreamingMode::UVC;
    }
    else if (modeStr == "wifi")
    {
        newMode = StreamingMode::WIFI;
    }
    else if (modeStr == "setup" || modeStr == "auto")
    {
        newMode = StreamingMode::SETUP;
    }
    else
    {
        return CommandResult::getErrorResult("Invalid mode - use 'uvc', 'wifi', or 'auto'");
    }

    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    ESP_LOGI("[DEVICE_COMMANDS]", "Setting device mode to: %d", (int)newMode);
    projectConfig->setDeviceMode(newMode);

    return CommandResult::getSuccessResult("Device mode switched, restart to apply");
}

CommandResult getDeviceModeCommand(std::shared_ptr<DependencyRegistry> registry)
{
    const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    StreamingMode currentMode = projectConfig->getDeviceMode();

    const char *modeStr = "unknown";
    switch (currentMode)
    {
    case StreamingMode::UVC:
        modeStr = "UVC";
        break;
    case StreamingMode::WIFI:
        modeStr = "WiFi";
        break;
    case StreamingMode::SETUP:
        modeStr = "Setup";
        break;
    }

    const auto json = nlohmann::json{
        {"mode", modeStr},
        {"value", static_cast<int>(currentMode)},
    };
    return CommandResult::getSuccessResult(json);
}

CommandResult getSerialNumberCommand(std::shared_ptr<DependencyRegistry> /*registry*/)
{
    // Read MAC for STA interface
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    char serial_no_sep[13];
    // Serial without separators (12 hex chars)
    std::snprintf(serial_no_sep, sizeof(serial_no_sep), "%02X%02X%02X%02X%02X%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    char mac_colon[18];
    // MAC with colons
    std::snprintf(mac_colon, sizeof(mac_colon), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    const auto json = nlohmann::json{
        {"serial", serial_no_sep},
        {"mac", mac_colon},
    };
    return CommandResult::getSuccessResult(json);
}

CommandResult getLEDCurrentCommand(std::shared_ptr<DependencyRegistry> registry)
{
#if CONFIG_MONITORING_LED_CURRENT
    auto mon = registry->resolve<MonitoringManager>(DependencyType::monitoring_manager);
    if (!mon)
    {
        return CommandResult::getErrorResult("MonitoringManager unavailable");
    }
    float ma = mon->getCurrentMilliAmps();
    const auto json = nlohmann::json{{"led_current_ma", std::format("{:.3f}", static_cast<double>(ma))}};
    return CommandResult::getSuccessResult(json);
#else
    return CommandResult::getErrorResult("Monitoring disabled");
#endif
}

CommandResult getInfoCommand(std::shared_ptr<DependencyRegistry> /*registry*/)
{
    const char *who = CONFIG_GENERAL_BOARD;
    const char *ver = CONFIG_GENERAL_VERSION;
    // Ensure non-null strings
    if (!who)
        who = "";
    if (!ver)
        ver = "";

    const auto json = nlohmann::json{
        {"who_am_i", who},
        {"version", ver},
    };
    return CommandResult::getSuccessResult(json);
}
