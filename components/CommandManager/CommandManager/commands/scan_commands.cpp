#include "scan_commands.hpp"
#include "sdkconfig.h"

CommandResult scanNetworksCommand(std::shared_ptr<DependencyRegistry> registry)
{
#if !CONFIG_GENERAL_ENABLE_WIRELESS
    return CommandResult::getErrorResult("Not supported by current firmware");
#endif
    auto wifiManager = registry->resolve<WiFiManager>(DependencyType::wifi_manager);
    if (!wifiManager)
    {
        return CommandResult::getErrorResult("Not supported by current firmware");
    }

    auto networks = wifiManager->ScanNetworks();

    nlohmann::json result;
    std::vector<nlohmann::json> networksJson;

    for (const auto &network : networks)
    {
        nlohmann::json networkItem;
        networkItem["ssid"] = network.ssid;
        networkItem["channel"] = network.channel;
        networkItem["rssi"] = network.rssi;
        char mac_str[18];
        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
                network.mac[0], network.mac[1], network.mac[2],
                network.mac[3], network.mac[4], network.mac[5]);
        networkItem["mac_address"] = mac_str;
        networkItem["auth_mode"] = network.auth_mode;
        networksJson.push_back(networkItem);
    }

    result["networks"] = networksJson;
    return CommandResult::getSuccessResult(result);
}