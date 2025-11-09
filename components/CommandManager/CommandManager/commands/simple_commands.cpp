#include "simple_commands.hpp"

static const char *TAG = "SimpleCommands";

CommandResult PingCommand()
{
  return CommandResult::getSuccessResult("pong");
};

CommandResult PauseCommand(const nlohmann::json &json)
{
  auto pause = true;

  if (json.contains("pause") && json["pause"].is_boolean())
  {
    pause = json["pause"].get<bool>();
  }

  ESP_LOGI(TAG, "Pause command received: %s", pause ? "true" : "false");

  setStartupPaused(pause);
  if (pause)
  {
    ESP_LOGI(TAG, "Startup paused - device will remain in configuration mode");
    return CommandResult::getSuccessResult("Startup paused");
  }
  else
  {
    ESP_LOGI(TAG, "Startup resumed");
    return CommandResult::getSuccessResult("Startup resumed");
  }
};
