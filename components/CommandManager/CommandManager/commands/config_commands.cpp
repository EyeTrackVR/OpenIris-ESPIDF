#include "config_commands.hpp"

CommandResult saveConfigCommand(std::shared_ptr<DependencyRegistry> registry)
{
  std::shared_ptr<ProjectConfig> projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);

  projectConfig->save();
  return CommandResult::getSuccessResult("Config saved");
}

CommandResult getConfigCommand(std::shared_ptr<DependencyRegistry> registry)
{
  std::shared_ptr<ProjectConfig> projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);

  auto configRepresentation = projectConfig->getTrackerConfig().toRepresentation();
  return CommandResult::getSuccessResult(configRepresentation);
}

CommandResult resetConfigCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{
  std::array<std::string, 4> supported_sections = {
      "all",
  };

  if (!json.contains("section"))
  {
    return CommandResult::getErrorResult("Invalid payload - missing section");
  }

  auto section = json["section"].get<std::string>();

  if (std::find(supported_sections.begin(), supported_sections.end(), section) == supported_sections.end())
  {
    return CommandResult::getErrorResult("Selected section is unsupported");
  }

  // we cannot match on string, and making a map would be overkill right now, sooo
  // todo, add more granular control for other sections, like only reset camera, or only reset wifi
  if (section == "all")
  {
    std::shared_ptr<ProjectConfig> projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
    projectConfig->reset();
  }

  return CommandResult::getSuccessResult("Config reset");
}