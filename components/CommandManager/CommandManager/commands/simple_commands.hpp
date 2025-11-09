#ifndef SIMPLE_COMMANDS
#define SIMPLE_COMMANDS

#include <string>
#include "CommandResult.hpp"
#include "main_globals.hpp"
#include "esp_log.h"
#include <nlohmann-json.hpp>

CommandResult PingCommand();
CommandResult PauseCommand(const nlohmann::json &json);

#endif