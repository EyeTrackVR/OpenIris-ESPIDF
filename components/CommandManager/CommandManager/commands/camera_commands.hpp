#ifndef CAMERA_COMMANDS_HPP
#define CAMERA_COMMANDS_HPP
#include <ProjectConfig.hpp>
#include <memory>
#include <string>
#include <optional>
#include "CommandResult.hpp"
#include "CommandSchema.hpp"
#include "DependencyRegistry.hpp"
#include <CameraManager.hpp>
#include <nlohmann-json.hpp>

CommandResult updateCameraCommand(std::shared_ptr<DependencyRegistry> registry, const nlohmann::json &json);

#endif

// add cropping command