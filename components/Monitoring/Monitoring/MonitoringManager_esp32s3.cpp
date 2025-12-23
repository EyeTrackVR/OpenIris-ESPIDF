#include "MonitoringManager.hpp"
#include <cmath>
#include <esp_log.h>
#include "sdkconfig.h"

static const char *TAG_MM = "[MonitoringManager]";
void MonitoringManager::setup()
{
#if CONFIG_MONITORING_LED_CURRENT
    cm_.setup();
    ESP_LOGI(TAG_MM, "LED current monitoring enabled. Interval=%dms, Samples=%d, Gain=%d, R=%dmΩ",
             CONFIG_MONITORING_LED_INTERVAL_MS,
             CONFIG_MONITORING_LED_SAMPLES,
             CONFIG_MONITORING_LED_GAIN,
             CONFIG_MONITORING_LED_SHUNT_MILLIOHM);
#else
    ESP_LOGI(TAG_MM, "LED current monitoring disabled by Kconfig");
#endif

#if CONFIG_MONITORING_BATTERY_ENABLE
    bm_.setup();
    ESP_LOGI(TAG_MM, "Battery monitoring enabled. Interval=%dms, Samples=%d, R-Top=%dΩ, R-Bottom=%dΩ",
             CONFIG_MONITORING_BATTERY_INTERVAL_MS,
             CONFIG_MONITORING_BATTERY_SAMPLES,
             CONFIG_MONITORING_BATTERY_DIVIDER_R_TOP_OHM,
             CONFIG_MONITORING_BATTERY_DIVIDER_R_BOTTOM_OHM);
#else
    ESP_LOGI(TAG_MM, "Battery monitoring disabled by Kconfig");
#endif
}

void MonitoringManager::start()
{
#if CONFIG_MONITORING_LED_CURRENT || CONFIG_MONITORING_BATTERY_ENABLE
    if (task_ == nullptr)
    {
        xTaskCreate(&MonitoringManager::taskEntry, "MonitoringTask", 2048, this, 1, &task_);
    }
#endif
}

void MonitoringManager::stop()
{
    if (task_)
    {
        TaskHandle_t toDelete = task_;
        task_ = nullptr;
        vTaskDelete(toDelete);
    }
}

void MonitoringManager::taskEntry(void *arg)
{
    static_cast<MonitoringManager *>(arg)->run();
}

void MonitoringManager::run()
{
#if CONFIG_MONITORING_LED_CURRENT || CONFIG_MONITORING_BATTERY_ENABLE
    TickType_t now_tick = xTaskGetTickCount();
#if CONFIG_MONITORING_LED_CURRENT
    TickType_t next_tick_led = now_tick;
    const TickType_t led_period = pdMS_TO_TICKS(CONFIG_MONITORING_LED_INTERVAL_MS);
#endif
#if CONFIG_MONITORING_BATTERY_ENABLE
    TickType_t next_tick_bat = now_tick;
    const TickType_t batt_period = pdMS_TO_TICKS(CONFIG_MONITORING_BATTERY_INTERVAL_MS);
#endif
    

    while (true)
    {
        now_tick = xTaskGetTickCount();
        TickType_t wait_ticks = pdMS_TO_TICKS(50);

#if CONFIG_MONITORING_LED_CURRENT
        if (now_tick >= next_tick_led)
        {
            float ma = cm_.getCurrentMilliAmps();
            last_current_ma_.store(ma);
            next_tick_led = now_tick + led_period;
        }
        TickType_t to_led = (next_tick_led > now_tick) ? (next_tick_led - now_tick) : 1;
        if (to_led < wait_ticks)
        {
            wait_ticks = to_led;
        }
#endif

#if CONFIG_MONITORING_BATTERY_ENABLE
        if (now_tick >= next_tick_bat)
        {
            const int mv = bm_.getBatteryMilliVolts();
            if (mv > 0)
            {
                last_battery_mv_.store(mv);
            }
            next_tick_bat = now_tick + batt_period;
        }
        TickType_t to_batt = (next_tick_bat > now_tick) ? (next_tick_bat - now_tick) : 1;
        if (to_batt < wait_ticks)
        {
            wait_ticks = to_batt;
        }
#endif

        if (wait_ticks == 0)
        {
            wait_ticks = 1;
        }
        vTaskDelay(wait_ticks);
    }
#else
    vTaskDelete(nullptr);
#endif
}

float MonitoringManager::getCurrentMilliAmps() const
{
#if CONFIG_MONITORING_LED_CURRENT
    return last_current_ma_.load();
#else
    return 0.0f;
#endif
}

float MonitoringManager::getBatteryVoltageMilliVolts() const
{
#if CONFIG_MONITORING_BATTERY_ENABLE
    return static_cast<float>(last_battery_mv_.load());
#else
    return 0.0f;
#endif
}
