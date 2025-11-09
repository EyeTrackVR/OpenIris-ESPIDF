#include "mdns_commands.hpp"

CommandResult setMDNSCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json)
{
  const auto payload = json.get<MDNSPayload>();
  if (payload.hostname.empty())
    return CommandResult::getErrorResult("Invalid payload - empty hostname");

  std::shared_ptr<ProjectConfig> projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
  projectConfig->setMDNSConfig(payload.hostname);

  return CommandResult::getSuccessResult("Config updated");
}

CommandResult getMDNSNameCommand(std::shared_ptr<DependencyRegistry> registry)
{
  const auto projectConfig = registry->resolve<ProjectConfig>(DependencyType::project_config);
  auto mdnsConfig = projectConfig->getMDNSConfig();

  const auto json = nlohmann::json{{"hostname", mdnsConfig.hostname}};
  return CommandResult::getSuccessResult(json);
}