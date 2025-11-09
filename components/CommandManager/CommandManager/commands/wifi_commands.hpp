#include <ProjectConfig.hpp>
#include <wifiManager.hpp>
#include <StateManager.hpp>
#include <memory>
#include <string>
#include <optional>
#include "CommandResult.hpp"
#include "CommandSchema.hpp"
#include "DependencyRegistry.hpp"
#include <nlohmann-json.hpp>

CommandResult setWiFiCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult deleteWiFiCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult updateWiFiCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult updateAPWiFiCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

CommandResult getWiFiStatusCommand(std::shared_ptr<DependencyRegistry> registry);
CommandResult connectWiFiCommand(std::shared_ptr<DependencyRegistry> registry);
