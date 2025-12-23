#include "AdcSampler.hpp"

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <esp_log.h>
#include <cmath>

static const char *TAG_ADC = "[AdcSampler]";

adc_oneshot_unit_handle_t AdcSampler::shared_unit_ = nullptr;

AdcSampler::~AdcSampler()
{
    if (cali_handle_)
    {
        adc_cali_delete_scheme_curve_fitting(cali_handle_);
        cali_handle_ = nullptr;
    }
}

bool AdcSampler::init(int gpio, adc_atten_t atten, adc_bitwidth_t bitwidth, size_t window_size)
{
    if (window_size == 0)
    {
        window_size = 1;
    }
    samples_.assign(window_size, 0);
    sample_sum_ = 0;
    sample_idx_ = 0;
    sample_count_ = 0;

    atten_ = atten;
    bitwidth_ = bitwidth;

    if (!map_gpio_to_channel(gpio, unit_, channel_))
    {
        ESP_LOGW(TAG_ADC, "GPIO %d may not be ADC-capable on ESP32-S3", gpio);
    }

    if (!ensure_unit())
    {
        return false;
    }

    if (!configure_channel(gpio, atten, bitwidth))
    {
        return false;
    }

    // Calibration using curve fitting if available
    adc_cali_curve_fitting_config_t cal_cfg = {
        .unit_id = unit_,
        .chan = channel_,
        .atten = atten_,
        .bitwidth = bitwidth_,
    };
    if (adc_cali_create_scheme_curve_fitting(&cal_cfg, &cali_handle_) == ESP_OK)
    {
        cali_inited_ = true;
        ESP_LOGI(TAG_ADC, "ADC calibration initialized (curve fitting)");
    }
    else
    {
        cali_inited_ = false;
        ESP_LOGW(TAG_ADC, "ADC calibration not available; using raw-to-mV approximation");
    }

    return true;
}

bool AdcSampler::sampleOnce()
{
    if (!shared_unit_)
    {
        return false;
    }

    int raw = 0;
    if (adc_oneshot_read(shared_unit_, channel_, &raw) != ESP_OK)
    {
        return false;
    }

    int mv = 0;
    if (cali_inited_)
    {
        if (adc_cali_raw_to_voltage(cali_handle_, raw, &mv) != ESP_OK)
        {
            mv = 0;
        }
    }
    else
    {
        // Approximation for 11dB attenuation
        mv = (raw * 2450) / 4095;
    }

    // Moving average
    sample_sum_ -= samples_[sample_idx_];
    samples_[sample_idx_] = mv;
    sample_sum_ += mv;
    sample_idx_ = (sample_idx_ + 1) % samples_.size();
    if (sample_count_ < samples_.size())
    {
        sample_count_++;
    }
    filtered_mv_ = sample_sum_ / static_cast<int>(sample_count_ > 0 ? sample_count_ : 1);

    return true;
}

bool AdcSampler::ensure_unit()
{
    if (shared_unit_)
    {
        return true;
    }

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &shared_unit_);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_ADC, "adc_oneshot_new_unit failed: %s", esp_err_to_name(err));
        shared_unit_ = nullptr;
        return false;
    }
    return true;
}

bool AdcSampler::configure_channel(int gpio, adc_atten_t atten, adc_bitwidth_t bitwidth)
{
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = atten,
        .bitwidth = bitwidth,
    };
    esp_err_t err = adc_oneshot_config_channel(shared_unit_, channel_, &chan_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_ADC, "adc_oneshot_config_channel failed (GPIO %d, CH %d): %s", gpio, channel_, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool AdcSampler::map_gpio_to_channel(int gpio, adc_unit_t &unit, adc_channel_t &channel)
{
    unit = ADC_UNIT_1;
    if (gpio >= 1 && gpio <= 10)
    {
        channel = static_cast<adc_channel_t>(gpio - 1);
        return true;
    }
    channel = ADC_CHANNEL_0;
    return false;
}

#endif // CONFIG_IDF_TARGET_ESP32S3
