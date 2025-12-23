#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include "CurrentMonitor.hpp"
#include "BatteryMonitor.hpp"

class MonitoringManager
{
public:
    void setup();
    void start();
    void stop();

    // Latest filtered current in mA
    float getCurrentMilliAmps() const;
    // Latest battery voltage in mV
    float getBatteryVoltageMilliVolts() const;

private:
    static void taskEntry(void *arg);
    void run();

    TaskHandle_t task_{nullptr};
    std::atomic<float> last_current_ma_{0.0f};
    std::atomic<int> last_battery_mv_{0};
    CurrentMonitor cm_;
    BatteryMonitor bm_;
};
