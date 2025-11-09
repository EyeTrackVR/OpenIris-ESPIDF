#include <ProjectConfig.hpp>
#include <memory>
#include <string>
#include <optional>
#include "CommandResult.hpp"
#include "CommandSchema.hpp"
#include "DependencyRegistry.hpp"
#include <nlohmann-json.hpp>

CommandResult saveConfigCommand(std::shared_ptr<DependencyRegistry> registry);
CommandResult getConfigCommand(std::shared_ptr<DependencyRegistry> registry);

CommandResult resetConfigCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);