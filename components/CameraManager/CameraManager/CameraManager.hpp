#pragma once
#ifndef CAMERAMANAGER_HPP
#define CAMERAMANAGER_HPP

#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <ProjectConfig.hpp>
#include <StateManager.hpp>

#define OV5640_XCLK_FREQ_HZ CONFIG_CAMERA_WIFI_XCLK_FREQ

class CameraManager
{
   private:
    sensor_t* camera_sensor;
    std::shared_ptr<ProjectConfig> projectConfig;
    QueueHandle_t eventQueue;
    camera_config_t config;

    esp_err_t lastCameraError = ESP_OK;
    uint8_t* diagnosticJpegBuf = nullptr;
    size_t diagnosticJpegLen = 0;
    bool cameraOk = false;

   public:
    CameraManager(std::shared_ptr<ProjectConfig> projectConfig, QueueHandle_t eventQueue);
    int setCameraResolution(framesize_t frameSize);
    bool setupCamera();
    int setVFlip(int direction);
    int setHFlip(int direction);
    int setVieWindow(int offsetX, int offsetY, int outputX, int outputY);

    // Returns the last esp_err_t returned by esp_camera_init(), only meaningful after setupCamera() fails.
    esp_err_t getLastCameraError() const;

    // Fetches a cached JPEG that renders getLastCameraError() as large 7-segment-style hex digits,
    // so streaming clients (Baballonia/EyeTrackVR/etc.) see the failure instead of just losing the feed.
    // Returns false if no diagnostic frame has been generated (e.g. camera never failed).
    bool getDiagnosticFrame(const uint8_t** outBuf, size_t* outLen) const;

    // True once setupCamera() has succeeded. Checked by the retry task below and by
    // StreamServer to decide whether to serve live frames or the diagnostic fallback.
    bool isCameraOk() const;

    // Spawns a background task that keeps retrying setupCamera() every 5s while it hasn't
    // succeeded yet (e.g. the sensor was reseated after a failed boot), so the camera can
    // recover on its own without a manual reboot.
    void startAutoRetry();

   private:
    void loadConfigData();
    void setupCameraPinout();
    void setupCameraSensor();
    void generateDiagnosticFrame();
};

#endif  // CAMERAMANAGER_HPP