#include "CommandResult.hpp"
#include "ProjectConfig.hpp"
#include "OpenIrisTasks.hpp"
#include "DependencyRegistry.hpp"
#include "esp_timer.h"
#include "main_globals.hpp"

#include <format>
#include <string>
#include <nlohmann-json.hpp>

CommandResult updateOTACredentialsCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult updateLEDDutyCycleCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);
CommandResult getLEDDutyCycleCommand(std::shared_ptr<DependencyRegistry> registry);

CommandResult restartDeviceCommand();

CommandResult startStreamingCommand();

CommandResult switchModeCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult getDeviceModeCommand(std::shared_ptr<DependencyRegistry> registry);

CommandResult getSerialNumberCommand(std::shared_ptr<DependencyRegistry> registry);

// Monitoring
CommandResult getLEDCurrentCommand(std::shared_ptr<DependencyRegistry> registry);
CommandResult getBatteryStatusCommand(std::shared_ptr<DependencyRegistry> registry);

// General info
CommandResult getInfoCommand(std::shared_ptr<DependencyRegistry> registry);