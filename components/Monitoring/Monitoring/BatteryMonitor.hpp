#pragma once
#include "AdcSampler.hpp"
#include <cmath>

class BatteryMonitor
{
public:
    BatteryMonitor() = default;
    ~BatteryMonitor() = default;

    bool setup();

    // Read once, update filter, and return battery voltage in mV (after divider compensation).
    int getBatteryMilliVolts() const;

    // Whether monitoring is enabled by Kconfig
    static constexpr bool isEnabled()
    {
#ifdef CONFIG_MONITORING_BATTERY_ENABLE
        return true;
#else
        return false;
#endif
    }

private:
    float scale_{1.0f};
    mutable AdcSampler adc_;
};
