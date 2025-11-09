#include "CommandSchema.hpp"

void to_json(nlohmann::json &j, const UpdateWifiPayload &payload)
{
  j = nlohmann::json{{"name", payload.name}, {"ssid", payload.ssid}, {"password", payload.password}, {"channel", payload.channel}, {"power", payload.power}};
}

void from_json(const nlohmann::json &j, UpdateWifiPayload &payload)
{
  payload.name = j.at("name").get<std::string>();
  if (j.contains("ssid"))
  {
    payload.ssid = j.at("ssid").get<std::string>();
  }

  if (j.contains("password"))
  {
    payload.password = j.at("password").get<std::string>();
  }

  if (j.contains("channel"))
  {
    payload.channel = j.at("channel").get<uint8_t>();
  }

  if (j.contains("power"))
  {
    payload.power = j.at("power").get<uint8_t>();
  }
}

void to_json(nlohmann::json &j, const UpdateAPWiFiPayload &payload)
{
  j = nlohmann::json{{"ssid", payload.ssid}, {"password", payload.password}, {"channel", payload.channel}};
}

void from_json(const nlohmann::json &j, UpdateAPWiFiPayload &payload)
{
  if (j.contains("ssid"))
  {
    payload.ssid = j.at("ssid").get<std::string>();
  }

  if (j.contains("password"))
  {
    payload.password = j.at("password").get<std::string>();
  }
  if (j.contains("channel"))
  {
    payload.channel = j.at("channel").get<uint8_t>();
  }
}

void to_json(nlohmann::json &j, const UpdateCameraConfigPayload &payload)
{
  j = nlohmann::json{{"vflip", payload.vflip}, {"href", payload.href}, {"framesize", payload.framesize}, {"quality", payload.quality}, {"brightness", payload.brightness}};
}

void from_json(const nlohmann::json &j, UpdateCameraConfigPayload &payload)
{
  if (j.contains("vflip"))
  {
    payload.vflip = j.at("vflip").get<uint8_t>();
  }
  if (j.contains("href"))
  {
    payload.href = j.at("href").get<uint8_t>();
  }
  if (j.contains("framesize"))
  {
    payload.framesize = j.at("framesize").get<uint8_t>();
  }
  if (j.contains("quality"))
  {
    payload.quality = j.at("quality").get<uint8_t>();
  }
  if (j.contains("brightness"))
  {
    payload.brightness = j.at("brightness").get<uint8_t>();
  }
}