#ifndef CURRENT_MONITOR_HPP
#define CURRENT_MONITOR_HPP
#pragma once
#include <cstdint>
#include <memory>
#include "sdkconfig.h"
#include "AdcSampler.hpp"

class CurrentMonitor
{
public:
    CurrentMonitor() = default;
    ~CurrentMonitor() = default;

    void setup();

    // convenience: combined sampling and compute; returns mA
    float getCurrentMilliAmps() const;

    // Whether monitoring is enabled by Kconfig
    static constexpr bool isEnabled()
    {
#ifdef CONFIG_MONITORING_LED_CURRENT
        return true;
#else
        return false;
#endif
    }

private:
    mutable AdcSampler adc_;
};

#endif
