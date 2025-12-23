#pragma once
#include <cstddef>
#include <cstdint>
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <vector>

class AdcSampler
{
public:
    AdcSampler() = default;
    ~AdcSampler();

    // Initialize the ADC channel on the shared ADC1 oneshot unit.
    // window_size: moving-average window (>=1).
    bool init(int gpio, adc_atten_t atten = ADC_ATTEN_DB_12, adc_bitwidth_t bitwidth = ADC_BITWIDTH_DEFAULT, size_t window_size = 1);

    // Perform one conversion, update filtered value. Returns false on failure.
    bool sampleOnce();

    int getFilteredMilliVolts() const { return filtered_mv_; }

private:
    bool ensure_unit();
    bool configure_channel(int gpio, adc_atten_t atten, adc_bitwidth_t bitwidth);
    bool map_gpio_to_channel(int gpio, adc_unit_t &unit, adc_channel_t &channel);

    // Shared ADC1 oneshot handle and calibration mutex-less state (single-threaded use here).
    static adc_oneshot_unit_handle_t shared_unit_;

    adc_cali_handle_t cali_handle_{nullptr};
    bool cali_inited_{false};
    adc_channel_t channel_{ADC_CHANNEL_0};
    adc_unit_t unit_{ADC_UNIT_1};
    adc_atten_t atten_{ADC_ATTEN_DB_12};
    adc_bitwidth_t bitwidth_{ADC_BITWIDTH_DEFAULT};

    std::vector<int> samples_{};
    int sample_sum_{0};
    size_t sample_idx_{0};
    size_t sample_count_{0};
    int filtered_mv_{0};
};

#else
// Stub for non-ESP32-S3 targets to keep interfaces consistent.
class AdcSampler
{
public:
    bool init(int /*gpio*/, int /*atten*/ = 0, int /*bitwidth*/ = 0, size_t /*window_size*/ = 1) { return false; }
    bool sampleOnce() { return false; }
    int getFilteredMilliVolts() const { return 0; }
};
#endif
