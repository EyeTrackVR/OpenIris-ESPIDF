#include "BatteryMonitor.hpp"

#include <esp_log.h>

static const char *TAG_BAT = "[BatteryMonitor]";

bool BatteryMonitor::setup()
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (CONFIG_MONITORING_BATTERY_DIVIDER_R_TOP_OHM <= 0)
    {
        ESP_LOGE(TAG_BAT, "Invalid divider bottom resistor: %d", CONFIG_MONITORING_BATTERY_DIVIDER_R_TOP_OHM);
        return false;
    }
    scale_ = 1.0f + static_cast<float>(CONFIG_MONITORING_BATTERY_DIVIDER_R_TOP_OHM) / static_cast<float>(CONFIG_MONITORING_BATTERY_DIVIDER_R_BOTTOM_OHM);
    if (!adc_.init(CONFIG_MONITORING_BATTERY_ADC_GPIO, ADC_ATTEN_DB_12, ADC_BITWIDTH_DEFAULT, CONFIG_MONITORING_BATTERY_SAMPLES))
    {
        ESP_LOGE(TAG_BAT, "Battery ADC init failed");
        return false;
    }
    ESP_LOGI(TAG_BAT, "Battery monitor enabled (GPIO=%d, scale=%.3f)", CONFIG_MONITORING_BATTERY_ADC_GPIO, scale_);
    return true;
#else
    ESP_LOGI(TAG_BAT, "Battery monitoring not supported on this target");
    return false;
#endif
}

int BatteryMonitor::getBatteryMilliVolts() const
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    if (!adc_.sampleOnce())
        return 0;

    const int mv_at_adc = adc_.getFilteredMilliVolts();
    if (mv_at_adc <= 0)
        return 0;

    const float battery_mv = mv_at_adc * scale_;
    return static_cast<int>(std::lround(battery_mv));
#else
    return 0;
#endif
}
