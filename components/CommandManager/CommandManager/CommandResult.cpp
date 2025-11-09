#include "CommandResult.hpp"

void to_json(nlohmann::json &j, const CommandResult &result)
{
  j = nlohmann::json{{"status", result.isSuccess() ? "success" : "error"}, {"data", result.getData()}};
}

// defined only for interface compatibility, should not be used directly
void from_json(const nlohmann::json &j, CommandResult &result)
{
  auto message = j.at("message");
  j.at("status") == "success" ? result = CommandResult::getSuccessResult(message) : result = CommandResult::getErrorResult(message);
}

void to_json(nlohmann::json &j, const CommandManagerResponse &result)
{
  j = result.getData();
}

// defined only for interface compatibility, should not be used directly
void from_json(const nlohmann::json &j, CommandManagerResponse &result)
{
  result = CommandManagerResponse(j.at("result"));
}