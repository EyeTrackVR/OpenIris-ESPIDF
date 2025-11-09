#ifndef COMMAND_SCHEMA_HPP
#define COMMAND_SCHEMA_HPP
#include <nlohmann-json.hpp>

struct BasePayload
{
};

struct WifiPayload : BasePayload
{
  std::string name;
  std::string ssid;
  std::string password;
  uint8_t channel;
  uint8_t power;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WifiPayload, name, ssid, password, channel, power)

struct UpdateWifiPayload : BasePayload
{
  std::string name;
  std::optional<std::string> ssid;
  std::optional<std::string> password;
  std::optional<uint8_t> channel;
  std::optional<uint8_t> power;
};

void to_json(nlohmann::json &j, const UpdateWifiPayload &payload);
void from_json(const nlohmann::json &j, UpdateWifiPayload &payload);
struct deleteNetworkPayload : BasePayload
{
  std::string name;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(deleteNetworkPayload, name)

struct UpdateAPWiFiPayload : BasePayload
{
  std::optional<std::string> ssid;
  std::optional<std::string> password;
  std::optional<uint8_t> channel;
};

void to_json(nlohmann::json &j, const UpdateAPWiFiPayload &payload);
void from_json(const nlohmann::json &j, UpdateAPWiFiPayload &payload);
struct MDNSPayload : BasePayload
{
  std::string hostname;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MDNSPayload, hostname)

struct UpdateCameraConfigPayload : BasePayload
{
  std::optional<uint8_t> vflip;
  std::optional<uint8_t> href;
  std::optional<uint8_t> framesize;
  std::optional<uint8_t> quality;
  std::optional<uint8_t> brightness;
  // TODO add more options here
};

void to_json(nlohmann::json &j, const UpdateCameraConfigPayload &payload);
void from_json(const nlohmann::json &j, UpdateCameraConfigPayload &payload);
#endif