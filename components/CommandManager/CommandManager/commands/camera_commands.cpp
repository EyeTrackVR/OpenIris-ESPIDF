#include "camera_commands.hpp"

CommandResult updateCameraCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{
  auto payload = json.get<UpdateCameraConfigPayload>();

  std::shared_ptr<ProjectConfig> projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
  auto oldConfig = projectConfig->getCameraConfig();
  projectConfig->setCameraConfig(
      payload.vflip.has_value() ? payload.vflip.value() : oldConfig.vflip,
      payload.framesize.has_value() ? payload.framesize.value() : oldConfig.framesize,
      payload.href.has_value() ? payload.href.value() : oldConfig.href,
      payload.quality.has_value() ? payload.quality.value() : oldConfig.quality,
      payload.brightness.has_value() ? payload.brightness.value() : oldConfig.brightness);

  return CommandResult::getSuccessResult("Config updated");
}