#include "CommandManager.hpp"
#include <cstdlib>

std::unordered_map<std::string, CommandType> commandTypeMap = {
    {"ping", CommandType::PING},
    {"pause", CommandType::PAUSE},
    {"set_wifi", CommandType::SET_WIFI},
    {"update_wifi", CommandType::UPDATE_WIFI},
    {"update_ota_credentials", CommandType::UPDATE_OTA_CREDENTIALS},
    {"delete_network", CommandType::DELETE_NETWORK},
    {"update_ap_wifi", CommandType::UPDATE_AP_WIFI},
    {"set_mdns", CommandType::SET_MDNS},
    {"get_mdns_name", CommandType::GET_MDNS_NAME},
    {"update_camera", CommandType::UPDATE_CAMERA},
    {"save_config", CommandType::SAVE_CONFIG},
    {"get_config", CommandType::GET_CONFIG},
    {"reset_config", CommandType::RESET_CONFIG},
    {"restart_device", CommandType::RESTART_DEVICE},
    {"scan_networks", CommandType::SCAN_NETWORKS},
    {"start_streaming", CommandType::START_STREAMING},
    {"get_wifi_status", CommandType::GET_WIFI_STATUS},
    {"connect_wifi", CommandType::CONNECT_WIFI},
    {"switch_mode", CommandType::SWITCH_MODE},
    {"get_device_mode", CommandType::GET_DEVICE_MODE},
    {"set_led_duty_cycle", CommandType::SET_LED_DUTY_CYCLE},
    {"get_led_duty_cycle", CommandType::GET_LED_DUTY_CYCLE},
    {"get_serial", CommandType::GET_SERIAL},
    {"get_led_current", CommandType::GET_LED_CURRENT},
    {"get_battery_status", CommandType::GET_BATTERY_STATUS},
    {"get_who_am_i", CommandType::GET_WHO_AM_I},
};

std::function<CommandResult()> CommandManager::createCommand(const CommandType type, const nlohmann::json &json) const
{
  switch (type)
  {
  case CommandType::PING:
    return {PingCommand};
  case CommandType::PAUSE:
    return [json]
    { return PauseCommand(json); };
  case CommandType::UPDATE_OTA_CREDENTIALS:
    return [this, json]
    { return updateOTACredentialsCommand(this->registry, json); };
  case CommandType::SET_WIFI:
    return [this, json]
    { return setWiFiCommand(this->registry, json); };
  case CommandType::UPDATE_WIFI:
    return [this, json]
    { return updateWiFiCommand(this->registry, json); };
  case CommandType::UPDATE_AP_WIFI:
    return [this, json]
    { return updateAPWiFiCommand(this->registry, json); };
  case CommandType::DELETE_NETWORK:
    return [this, json]
    { return deleteWiFiCommand(this->registry, json); };
  case CommandType::SET_MDNS:
    return [this, json]
    { return setMDNSCommand(this->registry, json); };
  case CommandType::GET_MDNS_NAME:
    return [this]
    { return getMDNSNameCommand(this->registry); };
  case CommandType::UPDATE_CAMERA:
    return [this, json]
    { return updateCameraCommand(this->registry, json); };
  case CommandType::GET_CONFIG:
    return [this]
    { return getConfigCommand(this->registry); };
  case CommandType::SAVE_CONFIG:
    return [this]
    { return saveConfigCommand(this->registry); };
  case CommandType::RESET_CONFIG:
    return [this, json]
    { return resetConfigCommand(this->registry, json); };
  case CommandType::RESTART_DEVICE:
    return restartDeviceCommand;
  case CommandType::SCAN_NETWORKS:
    return [this, json]
    { return scanNetworksCommand(this->registry, json); };
  case CommandType::START_STREAMING:
    return startStreamingCommand;
  case CommandType::GET_WIFI_STATUS:
    return [this]
    { return getWiFiStatusCommand(this->registry); };
  case CommandType::CONNECT_WIFI:
    return [this]
    { return connectWiFiCommand(this->registry); };
  case CommandType::SWITCH_MODE:
    return [this, json]
    { return switchModeCommand(this->registry, json); };
  case CommandType::GET_DEVICE_MODE:
    return [this]
    { return getDeviceModeCommand(this->registry); };
  case CommandType::SET_LED_DUTY_CYCLE:
    return [this, json]
    { return updateLEDDutyCycleCommand(this->registry, json); };
  case CommandType::GET_LED_DUTY_CYCLE:
    return [this]
    { return getLEDDutyCycleCommand(this->registry); };
  case CommandType::GET_SERIAL:
    return [this]
    { return getSerialNumberCommand(this->registry); };
  case CommandType::GET_LED_CURRENT:
    return [this]
    { return getLEDCurrentCommand(this->registry); };
  case CommandType::GET_BATTERY_STATUS:
    return [this]
    { return getBatteryStatusCommand(this->registry); };
  case CommandType::GET_WHO_AM_I:
    return [this]
    { return getInfoCommand(this->registry); };
  default:
    return nullptr;
  }
}

CommandManagerResponse CommandManager::executeFromJson(const std::string_view json) const
{
  if (!nlohmann::json::accept(json))
  {
    return CommandManagerResponse(nlohmann::json{{"error", "Initial JSON Parse - Invalid JSON"}});
  }

  nlohmann::json parsedJson = nlohmann::json::parse(json);
  if (!parsedJson.contains("commands") || !parsedJson["commands"].is_array() || parsedJson["commands"].empty())
  {
    return CommandManagerResponse(CommandResult::getErrorResult("Commands missing"));
  }

  nlohmann::json results = nlohmann::json::array();

  for (auto &commandObject : parsedJson["commands"].items())
  {
    auto commandData = commandObject.value();
    if (!commandData.contains("command"))
    {
      return CommandManagerResponse({{"command", "Unknown command"}, {"error", "Missing command type"}});
    }

    const auto commandName = commandData["command"].get<std::string>();
    if (!commandTypeMap.contains(commandName))
    {
      return CommandManagerResponse({{"command", commandName}, {"error", "Unknown command"}});
    }

    const auto commandType = commandTypeMap.at(commandName);
    const auto commandPayload = commandData.contains("data") ? commandData["data"] : nlohmann::json::object();

    auto command = createCommand(commandType, commandPayload);
    results.push_back({
        {"command", commandName},
        {"result", command()},
    });
  }
  auto response = nlohmann::json{{"results", results}};
  return CommandManagerResponse(response);
}

CommandManagerResponse CommandManager::executeFromType(const CommandType type, const std::string_view json) const
{
  const auto command = createCommand(type, json);

  if (command == nullptr)
  {
    return CommandManagerResponse({{"command", type}, {"error", "Unknown command"}});
  }

  return CommandManagerResponse(nlohmann::json{{"result", command()}});
}