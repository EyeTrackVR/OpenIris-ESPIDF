#include "LEDManager.hpp"

const char *LED_MANAGER_TAG = "[LED_MANAGER]";

// Pattern design rules:
//  - Error states: isError=true, repeat indefinitely, easily distinguishable (avoid overlap).
//  - Non-error repeating: show continuous activity (e.g. streaming ON steady, connecting blink).
//  - Non-repeating notification (e.g. Connected) gives user a brief confirmation burst then turns off.
// Durations in ms.
ledStateMap_t LEDManager::ledStateMap = {
    { LEDStates_e::LedStateNone, { /*isError*/false, /*repeat*/false, {{LED_OFF, 1000}} } },
    { LEDStates_e::LedStateStreaming, { false, /*repeat steady*/true, {{LED_ON, 1000}} } },
    { LEDStates_e::LedStateStoppedStreaming, { false, true, {{LED_OFF, 1000}} } },
    // CameraError: double blink pattern repeating
    { LEDStates_e::CameraError, { true, true, {{ {LED_ON,300}, {LED_OFF,300}, {LED_ON,300}, {LED_OFF,700} }} } },
    // WiFiStateConnecting: balanced slow blink 400/400
    { LEDStates_e::WiFiStateConnecting, { false, true, {{ {LED_ON,400}, {LED_OFF,400} }} } },
    // WiFiStateConnected: short 3 quick flashes then done (was long noisy burst before)
    { LEDStates_e::WiFiStateConnected, { false, false, {{ {LED_ON,150}, {LED_OFF,150}, {LED_ON,150}, {LED_OFF,150}, {LED_ON,150}, {LED_OFF,600} }} } },
    // WiFiStateError: asymmetric attention pattern (fast, pause, long, pause, fast)
    { LEDStates_e::WiFiStateError, { true, true, {{ {LED_ON,200}, {LED_OFF,100}, {LED_ON,500}, {LED_OFF,300} }} } },
};

LEDManager::LEDManager(gpio_num_t pin, gpio_num_t illumninator_led_pin,
                       QueueHandle_t ledStateQueue, std::shared_ptr<ProjectConfig> deviceConfig) : blink_led_pin(pin),
                                                                                                   illumninator_led_pin(illumninator_led_pin),
                                                                                                   ledStateQueue(ledStateQueue),
                                                                                                   currentState(LEDStates_e::LedStateNone),
                                                                                                   deviceConfig(deviceConfig)
{
}

void LEDManager::setup()
{
    ESP_LOGI(LED_MANAGER_TAG, "Setting up status led.");
#ifdef CONFIG_LED_DEBUG_ENABLE
    gpio_reset_pin(blink_led_pin);
    gpio_set_direction(blink_led_pin, GPIO_MODE_OUTPUT);
    this->toggleLED(LED_OFF);
#else
    ESP_LOGI(LED_MANAGER_TAG, "Debug LED disabled via Kconfig (LED_DEBUG_ENABLE=n)");
#endif

#ifdef CONFIG_LED_EXTERNAL_CONTROL
    ESP_LOGI(LED_MANAGER_TAG, "Setting up illuminator led.");
    const int freq = CONFIG_LED_EXTERNAL_PWM_FREQ;
    const auto resolution = LEDC_TIMER_8_BIT;
    const auto deviceConfig = this->deviceConfig->getDeviceConfig();

    const uint32_t dutyCycle = (deviceConfig.led_external_pwm_duty_cycle * 255) / 100;

    ESP_LOGI(LED_MANAGER_TAG, "Setting dutyCycle to: %lu ", dutyCycle);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = resolution,
        .timer_num = LEDC_TIMER_1, // moved to TIMER_1 to avoid conflict with camera XCLK (TIMER_0)
        .freq_hz = freq,
        .clk_cfg = LEDC_AUTO_CLK};

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = this->illumninator_led_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1, // moved to CHANNEL_1 to avoid conflict
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = dutyCycle,
        .hpoint = 0};

    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
#endif

    ESP_LOGD(LED_MANAGER_TAG, "Done.");
}

void LEDManager::handleLED()
{
    if (!this->finishedPattern)
    {
        displayCurrentPattern();
        return;
    }

    if (xQueueReceive(this->ledStateQueue, &buffer, 10))
    {
        this->updateState(buffer);
    }
    else
    {
        // we've finished displaying the pattern, so let's check if it's repeatable and if so - reset it
        if (ledStateMap[this->currentState].isRepeatable || ledStateMap[this->currentState].isError)
        {
            this->currentPatternIndex = 0;
            this->finishedPattern = false;
        }
    }
}

void LEDManager::displayCurrentPattern()
{
    auto [state, delayTime] = ledStateMap[this->currentState].patterns[this->currentPatternIndex];
    this->toggleLED(state);
    this->timeToDelayFor = delayTime;

    if (this->currentPatternIndex < ledStateMap[this->currentState].patterns.size() - 1)
        this->currentPatternIndex++;
    else
    {
        this->finishedPattern = true;
        this->toggleLED(LED_OFF);
    }
}

void LEDManager::updateState(const LEDStates_e newState)
{
    // If we've got an error state - that's it, keep repeating it indefinitely
    if (ledStateMap[this->currentState].isError)
        return;

    // Alternative (recoverable error states):
    // Allow recovery from error states by only blocking transitions when both, current and new states are error. Uncomment to enable recovery.
    // if (ledStateMap[this->currentState].isError && ledStateMap[newState].isError)
    //     return;

    // Only update when new state differs and is known.
    if (!ledStateMap.contains(newState))
        return;

    if (newState == this->currentState)
        return;

    // Handle external LED mirroring transitions (store/restore duty)
#if defined(CONFIG_LED_EXTERNAL_CONTROL) && defined(CONFIG_LED_EXTERNAL_AS_DEBUG)
    bool wasError = ledStateMap[this->currentState].isError;
    bool willBeError = ledStateMap[newState].isError;
    if (!wasError && willBeError)
    {
        // store current duty once
        if (!hasStoredExternalDuty)
        {
            storedExternalDuty = ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            hasStoredExternalDuty = true;
        }
    }
    else if (wasError && !willBeError)
    {
        // restore duty
        if (hasStoredExternalDuty)
        {
            ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, storedExternalDuty));
            ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
            hasStoredExternalDuty = false;
        }
    }
#endif

    this->currentState = newState;
    this->currentPatternIndex = 0;
    this->finishedPattern = false;
}

void LEDManager::toggleLED(const bool state) const
{
#ifdef CONFIG_LED_DEBUG_ENABLE
    gpio_set_level(blink_led_pin, state);
#endif

#if defined(CONFIG_LED_EXTERNAL_CONTROL) && defined(CONFIG_LED_EXTERNAL_AS_DEBUG)
    // Mirror only for error states
    if (ledStateMap.contains(this->currentState) && ledStateMap.at(this->currentState).isError)
    {
        // For pattern ON use 50%, OFF use 0%
    uint32_t duty = (state == LED_ON) ? ((50 * 255) / 100) : 0;
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    }
#endif
}

void LEDManager::setExternalLEDDutyCycle(uint8_t dutyPercent)
{
#ifdef CONFIG_LED_EXTERNAL_CONTROL
    const uint32_t dutyCycle = (static_cast<uint32_t>(dutyPercent) * 255) / 100;
    ESP_LOGI(LED_MANAGER_TAG, "Updating external LED duty to %u%% (raw %lu)", dutyPercent, dutyCycle);

    // Apply to LEDC hardware live
    // We configured channel 0 in setup with LEDC_LOW_SPEED_MODE
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, dutyCycle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
#else
    (void)dutyPercent; // unused
    ESP_LOGW(LED_MANAGER_TAG, "CONFIG_LED_EXTERNAL_CONTROL not enabled; ignoring duty update");
#endif
}

void HandleLEDDisplayTask(void *pvParameter)
{
    auto *ledManager = static_cast<LEDManager *>(pvParameter);
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (true)
    {
        ledManager->handleLED();
        const TickType_t delayTicks = pdMS_TO_TICKS(ledManager->getTimeToDelayFor());
        // Ensure at least 1 tick delay to yield CPU
        vTaskDelayUntil(&lastWakeTime, delayTicks > 0 ? delayTicks : 1);
    }
}
