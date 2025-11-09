#include <ProjectConfig.hpp>
#include <memory>
#include <string>
#include <optional>
#include "CommandResult.hpp"
#include "CommandSchema.hpp"
#include "DependencyRegistry.hpp"
#include <nlohmann-json.hpp>

CommandResult setMDNSCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);
CommandResult getMDNSNameCommand(std::shared_ptr<DependencyRegistry> registry);