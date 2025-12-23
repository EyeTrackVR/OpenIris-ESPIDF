#include <esp_log.h>
#include <cmath>
#include "CurrentMonitor.hpp"

static const char *TAG_CM = "[CurrentMonitor]";

void CurrentMonitor::setup()
{
#ifdef CONFIG_MONITORING_LED_CURRENT
    const bool ok = adc_.init(CONFIG_MONITORING_LED_ADC_GPIO, ADC_ATTEN_DB_12, ADC_BITWIDTH_DEFAULT, CONFIG_MONITORING_LED_SAMPLES);
    if (!ok)
    {
        ESP_LOGE(TAG_CM, "ADC init failed for LED current monitor");
    }
#else
    ESP_LOGI(TAG_CM, "LED current monitoring disabled");
#endif
}

float CurrentMonitor::getCurrentMilliAmps() const
{
#ifdef CONFIG_MONITORING_LED_CURRENT
    const int shunt_milliohm = CONFIG_MONITORING_LED_SHUNT_MILLIOHM; // mΩ
    if (shunt_milliohm <= 0)
        return 0.0f;

    if (!adc_.sampleOnce())
        return 0.0f;

    int filtered_mv = adc_.getFilteredMilliVolts();
    if (CONFIG_MONITORING_LED_GAIN > 0)
        filtered_mv = filtered_mv / CONFIG_MONITORING_LED_GAIN;  // convert back to shunt voltage

    // Physically correct scaling:
    // I[mA] = 1000 * Vshunt[mV] / R[mΩ]
    return (1000.0f * static_cast<float>(filtered_mv)) / static_cast<float>(shunt_milliohm);
#else
    return 0.0f;
#endif
}