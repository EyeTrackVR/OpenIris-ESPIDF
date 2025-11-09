#ifndef SCAN_COMMANDS_HPP
#define SCAN_COMMANDS_HPP

#include "CommandResult.hpp"
#include "DependencyRegistry.hpp"
#include "esp_log.h"
#include <wifiManager.hpp>
#include <string>
#include <nlohmann-json.hpp>

CommandResult scanNetworksCommand(std::shared_ptr<DependencyRegistry> registry);

#endif