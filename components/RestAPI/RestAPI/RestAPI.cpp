#include "RestAPI.hpp"

#include <utility>

#define POST_METHOD "POST"

bool getIsSuccess(const nlohmann::json &response)
{
  // since the commandManager will be returning CommandManagerResponse to simplify parsing on the clients end
  // we can slightly its json representation, and extract the status from there
  // note: This will only work for commands executed with CommandManager::executeFromType().
  if (!response.contains("result"))
  {
    return false;
  }

  return response.at("result").at("status").get<std::string>() == "success";
}

RestAPI::RestAPI(std::string url, std::shared_ptr<CommandManager> commandManager) : command_manager(commandManager)
{
  this->url = std::move(url);
  // updates
  routes.emplace("/api/update/wifi/", &RestAPI::handle_update_wifi);
  routes.emplace("/api/update/device/", &RestAPI::handle_update_device);
  routes.emplace("/api/update/camera/", &RestAPI::handle_update_camera);

  // post will reset it
  // resets
  routes.emplace("/api/reset/config/", &RestAPI::handle_reset_config);
  // gets
  routes.emplace("/api/get/config/", &RestAPI::handle_get_config);

  // reboots
  routes.emplace("/api/reboot/device/", &RestAPI::handle_reboot);
  routes.emplace("/api/reboot/camera/", &RestAPI::handle_camera_reboot);

  // heartbeat
  routes.emplace("/api/ping/", &RestAPI::pong);

  // special
  routes.emplace("/api/save/", &RestAPI::handle_save);
}

void RestAPI::begin()
{
  mg_log_set(MG_LL_DEBUG);
  mg_mgr_init(&mgr);
  // every route is handled through this class, with commands themselves by a command manager
  // hence we pass a pointer to this in mg_http_listen
  mg_http_listen(&mgr, this->url.c_str(), (mg_event_handler_t)RestAPIHelpers::event_handler, this);
}

void RestAPI::handle_request(struct mg_connection *connection, int event, void *event_data)
{
  if (event == MG_EV_HTTP_MSG)
  {
    auto const *message = static_cast<struct mg_http_message *>(event_data);
    auto const uri = std::string(message->uri.buf, message->uri.len);

    if (auto const handler = this->routes[uri])
    {
      auto *context = new RequestContext{
          .connection = connection,
          .method = std::string(message->method.buf, message->method.len),
          .body = std::string(message->body.buf, message->body.len),
      };
      (*this.*handler)(context);
    }
    else
    {
      mg_http_reply(connection, 404, "", "Wrong URL");
    }
  }
}

void RestAPIHelpers::event_handler(struct mg_connection *connection, int event, void *event_data)
{
  auto *rest_api_handler = static_cast<RestAPI *>(connection->fn_data);
  rest_api_handler->handle_request(connection, event, event_data);
}

void RestAPI::poll()
{
  mg_mgr_poll(&mgr, 100);
}

void HandleRestAPIPollTask(void *pvParameter)
{
  auto *rest_api_handler = static_cast<RestAPI *>(pvParameter);
  while (true)
  {
    rest_api_handler->poll();
    vTaskDelay(1000);
  }
}

// COMMANDS
// updates
void RestAPI::handle_update_wifi(RequestContext *context)
{
  if (context->method != POST_METHOD)
  {
    mg_http_reply(context->connection, 401, JSON_RESPONSE, "{%m:%m}", MG_ESC("error"), "Method not allowed");
    return;
  }

  const nlohmann::json result = command_manager->executeFromType(CommandType::UPDATE_WIFI, context->body);
  const auto code = getIsSuccess(result) ? 200 : 400;
  mg_http_reply(context->connection, code, JSON_RESPONSE, result.dump().c_str());
}

void RestAPI::handle_update_device(RequestContext *context)
{
  if (context->method != POST_METHOD)
  {
    mg_http_reply(context->connection, 401, JSON_RESPONSE, "{%m:%m}", MG_ESC("error"), "Method not allowed");
    return;
  }

  const nlohmann::json result = command_manager->executeFromType(CommandType::UPDATE_OTA_CREDENTIALS, context->body);
  const auto code = getIsSuccess(result) ? 200 : 500;
  mg_http_reply(context->connection, code, JSON_RESPONSE, result.dump().c_str());
}

void RestAPI::handle_update_camera(RequestContext *context)
{
  if (context->method != POST_METHOD)
  {
    mg_http_reply(context->connection, 401, JSON_RESPONSE, "{%m:%m}", MG_ESC("error"), "Method not allowed");
    return;
  }

  const nlohmann::json result = command_manager->executeFromType(CommandType::UPDATE_CAMERA, context->body);
  const auto code = getIsSuccess(result) ? 200 : 500;
  mg_http_reply(context->connection, code, JSON_RESPONSE, result.dump().c_str());
}

// gets

void RestAPI::handle_get_config(RequestContext *context)
{
  auto const result = this->command_manager->executeFromType(CommandType::GET_CONFIG, "");
  const nlohmann::json jsonResult = result;
  mg_http_reply(context->connection, 200, JSON_RESPONSE, "{%m:%m}", MG_ESC("result"), jsonResult.dump().c_str());
}

// resets

void RestAPI::handle_reset_config(RequestContext *context)
{
  if (context->method != POST_METHOD)
  {
    mg_http_reply(context->connection, 401, JSON_RESPONSE, "{%m:%m}", MG_ESC("error"), "Method not allowed");
    return;
  }

  const nlohmann::json result = this->command_manager->executeFromType(CommandType::RESET_CONFIG, "{\"section\": \"all\"}");
  const auto code = getIsSuccess(result) ? 200 : 500;
  mg_http_reply(context->connection, code, JSON_RESPONSE, "{%m:%m}", MG_ESC("result"), result.dump().c_str());
}

// reboots
void RestAPI::handle_reboot(RequestContext *context)
{
  const auto result = this->command_manager->executeFromType(CommandType::RESTART_DEVICE, "");
  mg_http_reply(context->connection, 200, JSON_RESPONSE, "{%m:%m}", MG_ESC("result"), "Ok");
}

void RestAPI::handle_camera_reboot(RequestContext *context)
{
  mg_http_reply(context->connection, 200, JSON_RESPONSE, "{%m:%m}", MG_ESC("result"), "Ok");
}

// heartbeat

void RestAPI::pong(RequestContext *context)
{
  const nlohmann::json result = this->command_manager->executeFromType(CommandType::PING, "");
  const auto code = getIsSuccess(result) ? 200 : 500;
  mg_http_reply(context->connection, code, JSON_RESPONSE, result.dump().c_str());
}

// special

void RestAPI::handle_save(RequestContext *context)
{
  const nlohmann::json result = this->command_manager->executeFromType(CommandType::SAVE_CONFIG, "");
  const auto code = getIsSuccess(result) ? 200 : 500;
  mg_http_reply(context->connection, code, JSON_RESPONSE, result.dump().c_str());
}
