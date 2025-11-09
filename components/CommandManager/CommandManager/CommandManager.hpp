#ifndef COMMANDMANAGER_HPP
#define COMMANDMANAGER_HPP

#include <ProjectConfig.hpp>
#include <CameraManager.hpp>
#include <memory>
#include <string>
#include <optional>
#include <unordered_map>
#include <functional>
#include "CommandResult.hpp"
#include "CommandSchema.hpp"
#include "DependencyRegistry.hpp"
#include "commands/simple_commands.hpp"
#include "commands/camera_commands.hpp"
#include "commands/config_commands.hpp"
#include "commands/mdns_commands.hpp"
#include "commands/wifi_commands.hpp"
#include "commands/device_commands.hpp"
#include "commands/scan_commands.hpp"
#include <nlohmann-json.hpp>

enum class CommandType
{
  None,
  PING,
  PAUSE,
  SET_WIFI,
  UPDATE_OTA_CREDENTIALS,
  UPDATE_WIFI,
  DELETE_NETWORK,
  UPDATE_AP_WIFI,
  SET_MDNS,
  GET_MDNS_NAME,
  UPDATE_CAMERA,
  SAVE_CONFIG,
  GET_CONFIG,
  RESET_CONFIG,
  RESTART_DEVICE,
  SCAN_NETWORKS,
  START_STREAMING,
  GET_WIFI_STATUS,
  CONNECT_WIFI,
  SWITCH_MODE,
  GET_DEVICE_MODE,
  SET_LED_DUTY_CYCLE,
  GET_LED_DUTY_CYCLE,
  GET_SERIAL,
  GET_LED_CURRENT,
  GET_WHO_AM_I,
};

class CommandManager
{
  std::shared_ptr<DependencyRegistry> registry;

public:
  explicit CommandManager(const std::shared_ptr<DependencyRegistry> &DependencyRegistry) : registry(DependencyRegistry) {};
  std::function<CommandResult()> createCommand(const CommandType type, const nlohmann::json &json) const;

  CommandManagerResponse executeFromJson(std::string_view json) const;
  CommandManagerResponse executeFromType(CommandType type, std::string_view json) const;
};

#endif