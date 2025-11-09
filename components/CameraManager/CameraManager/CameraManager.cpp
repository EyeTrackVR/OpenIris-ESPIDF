#include "CameraManager.hpp"

const char *CAMERA_MANAGER_TAG = "[CAMERA_MANAGER]";

CameraManager::CameraManager(std::shared_ptr<ProjectConfig> projectConfig, QueueHandle_t eventQueue)
    : projectConfig(projectConfig), eventQueue(eventQueue) {}

void CameraManager::setupCameraPinout()
{
  // Workaround for espM5SStack not having a defined camera
#ifdef CONFIG_CAMERA_MODULE_NAME
  ESP_LOGI(CAMERA_MANAGER_TAG, "[Camera]: Camera module is %s", CONFIG_CAMERA_MODULE_NAME);
#else
  ESP_LOGI(CAMERA_MANAGER_TAG, "[Camera]: Camera module is undefined");
#endif

  // camera external clock signal frequencies
  // 10000000 stable
  // 16500000 optimal freq on ESP32-CAM (default)
  // 20000000 max freq on ESP32-CAM
  // 24000000 optimal freq on ESP32-S3 // 23MHz same fps
  int xclk_freq_hz = CONFIG_CAMERA_WIFI_XCLK_FREQ;
  ESP_LOGI(CAMERA_MANAGER_TAG, "Base WIFI XCLK freq (from config): %d Hz", xclk_freq_hz);

#if CONFIG_CAMERA_MODULE_ESP_EYE
  /* IO13, IO14 is designed for JTAG by default,
   * to use it as generalized input,
   * firstly declare it as pullup input
   **/
  gpio_reset_pin(13);
  gpio_reset_pin(14);
  gpio_set_direction(13, GPIO_MODE_INPUT);
  gpio_set_direction(14, GPIO_MODE_INPUT);
  gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(14, GPIO_PULLUP_ONLY);
  ESP_LOGI(CAMERA_MANAGER_TAG, "ESP_EYE");
#elif CONFIG_CAMERA_MODULE_CAM_BOARD
  /* IO13, IO14 is designed for JTAG by default,
   * to use it as generalized input,
   * firstly declare it as pullup input
   **/

  gpio_reset_pin(13);
  gpio_reset_pin(14);
  gpio_set_direction(13, GPIO_MODE_INPUT);
  gpio_set_direction(14, GPIO_MODE_INPUT);
  gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode(14, GPIO_PULLUP_ONLY);

  ESP_LOGI(CAMERA_MANAGER_TAG, "CAM_BOARD");
#endif
#if CONFIG_GENERAL_INCLUDE_UVC_MODE
  // Use code default for UVC if no per-sensor override applies
  constexpr int CAMERA_USB_XCLK_FREQ_DEFAULT = 20000000; // 20 MHz default
  xclk_freq_hz = CAMERA_USB_XCLK_FREQ_DEFAULT;
  ESP_LOGI(CAMERA_MANAGER_TAG, "UVC mode active: using USB default XCLK freq: %d Hz", xclk_freq_hz);
#endif

  config = {
      .pin_pwdn = CONFIG_PWDN_GPIO_NUM,     // CAM_PIN_PWDN,
      .pin_reset = CONFIG_RESET_GPIO_NUM,   // CAM_PIN_RESET,
      .pin_xclk = CONFIG_XCLK_GPIO_NUM,     // CAM_PIN_XCLK,
      .pin_sccb_sda = CONFIG_SIOD_GPIO_NUM, // CAM_PIN_SIOD,
      .pin_sccb_scl = CONFIG_SIOC_GPIO_NUM, // CAM_PIN_SIOC,

      .pin_d7 = CONFIG_Y9_GPIO_NUM,       /// CAM_PIN_D7,
      .pin_d6 = CONFIG_Y8_GPIO_NUM,       /// CAM_PIN_D6,
      .pin_d5 = CONFIG_Y7_GPIO_NUM,       // CAM_PIN_D5,
      .pin_d4 = CONFIG_Y6_GPIO_NUM,       // CAM_PIN_D4,
      .pin_d3 = CONFIG_Y5_GPIO_NUM,       // CAM_PIN_D3,
      .pin_d2 = CONFIG_Y4_GPIO_NUM,       // CAM_PIN_D2,
      .pin_d1 = CONFIG_Y3_GPIO_NUM,       // CAM_PIN_D1,
      .pin_d0 = CONFIG_Y2_GPIO_NUM,       // CAM_PIN_D0,
      .pin_vsync = CONFIG_VSYNC_GPIO_NUM, // CAM_PIN_VSYNC,
      .pin_href = CONFIG_HREF_GPIO_NUM,   // CAM_PIN_HREF,
      .pin_pclk = CONFIG_PCLK_GPIO_NUM,   // CAM_PIN_PCLK,

      .xclk_freq_hz = xclk_freq_hz, // Set in config
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,

      // this causes problems
      .pixel_format = PIXFORMAT_JPEG,  // YUV422,GRAYSCALE,RGB565,JPEG
      .frame_size = FRAMESIZE_240X240, // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

  .jpeg_quality = 8, // 0-63, for OV series camera sensors, lower number means higher quality // Below 6 stability problems
  .fb_count = 2,     // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
  .fb_location = CAMERA_FB_IN_DRAM,
  .grab_mode = CAMERA_GRAB_WHEN_EMPTY, // was CAMERA_GRAB_LATEST; new mode reduces frame skips at cost of minor latency
  };
}

void CameraManager::setupBasicResolution()
{

  if (!esp_psram_is_initialized())
  {
    ESP_LOGE(CAMERA_MANAGER_TAG, "PSRAM not initialized!");
    ESP_LOGD(CAMERA_MANAGER_TAG, "Setting fb_location to CAMERA_FB_IN_DRAM with lower picture quality");
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.jpeg_quality = 7;
    config.fb_count = 2;
    return;
  }

  ESP_LOGI(CAMERA_MANAGER_TAG, "PSRAM size: %u", esp_psram_get_size());
}

void CameraManager::setupCameraSensor()
{
  ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera sensor");

  camera_sensor = esp_camera_sensor_get();
  // fixes corrupted jpegs, https://github.com/espressif/esp32-camera/issues/203
  // documentation https://www.uctronics.com/download/cam_module/OV2640DS.pdf
  camera_sensor->set_reg(
      camera_sensor, 0xff, 0xff,
      0x00);                                            // banksel, here we're directly writing to the registers.
                                                        // 0xFF==0x00 is the first bank, there's also 0xFF==0x01
  camera_sensor->set_reg(camera_sensor, 0xd3, 0xff, 5); // clock
  camera_sensor->set_brightness(camera_sensor, 2);      // -2 to 2
  camera_sensor->set_contrast(camera_sensor, 2);        // -2 to 2
  camera_sensor->set_saturation(camera_sensor, -2);     // -2 to 2

  // white balance control
  camera_sensor->set_whitebal(camera_sensor, 1); // 0 = disable , 1 = enable
  camera_sensor->set_awb_gain(camera_sensor, 0); // 0 = disable , 1 = enable
  camera_sensor->set_wb_mode(camera_sensor,
                             0); // 0 to 4 - if awb_gain enabled (0 - Auto, 1 -
                                 // Sunny, 2 - Cloudy, 3 - Office, 4 - Home)

  // controls the exposure
  camera_sensor->set_exposure_ctrl(camera_sensor,
                                   0);              // 0 = disable , 1 = enable
  camera_sensor->set_aec2(camera_sensor, 0);        // 0 = disable , 1 = enable
  camera_sensor->set_ae_level(camera_sensor, 0);    // -2 to 2
  camera_sensor->set_aec_value(camera_sensor, 300); // 0 to 1200

  // controls the gain
  camera_sensor->set_gain_ctrl(camera_sensor, 0); // 0 = disable , 1 = enable

  // automatic gain control gain, controls by how much the resulting image
  // should be amplified
  camera_sensor->set_agc_gain(camera_sensor, 2);                   // 0 to 30
  camera_sensor->set_gainceiling(camera_sensor, (gainceiling_t)6); // 0 to 6

  // black and white pixel correction, averages the white and black spots
  camera_sensor->set_bpc(camera_sensor, 1); // 0 = disable , 1 = enable
  camera_sensor->set_wpc(camera_sensor, 1); // 0 = disable , 1 = enable
  // digital clamp white balance
  camera_sensor->set_dcw(camera_sensor, 0); // 0 = disable , 1 = enable

  // gamma correction
  camera_sensor->set_raw_gma(
      camera_sensor,
      1); // 0 = disable , 1 = enable (makes much lighter and noisy)

  camera_sensor->set_lenc(camera_sensor, 0); // 0 = disable , 1 = enable // 0 =
                                             // disable , 1 = enable

  camera_sensor->set_colorbar(camera_sensor, 0); // 0 = disable , 1 = enable

  camera_sensor->set_special_effect(
      camera_sensor,
      2); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint,
          // 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)

  // it gets overriden somewhere somehow
  camera_sensor->set_framesize(camera_sensor, FRAMESIZE_240X240);
  ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera sensor done");
}

bool CameraManager::setupCamera()
{
  ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera pinout");
  this->setupCameraPinout();
  ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera with resolution");
  // this->setupBasicResolution();

  ESP_LOGI(CAMERA_MANAGER_TAG, "Initializing camera...");

  if (auto const hasCameraBeenInitialized = esp_camera_init(&config); hasCameraBeenInitialized == ESP_OK)
  {
    ESP_LOGI(CAMERA_MANAGER_TAG, "Camera initialized: %s \r\n",
             esp_err_to_name(hasCameraBeenInitialized));
  ESP_LOGI(CAMERA_MANAGER_TAG, "Initial camera XCLK freq applied: %d Hz", config.xclk_freq_hz);

    constexpr auto event = SystemEvent{EventSource::CAMERA, CameraState_e::Camera_Success};
    xQueueSend(this->eventQueue, &event, 10);
  }
  else
  {
    ESP_LOGE(CAMERA_MANAGER_TAG, "Camera initialization failed with error: %s \r\n",
             esp_err_to_name(hasCameraBeenInitialized));
    ESP_LOGE(CAMERA_MANAGER_TAG, "Camera most likely not seated properly in the socket. "
                                 "Please "
                                 "fix the "
                                 "camera and reboot the device.\r\n");
    constexpr auto event = SystemEvent{EventSource::CAMERA, CameraState_e::Camera_Error};
    xQueueSend(this->eventQueue, &event, 10);
    return false;
  }

#if CONFIG_GENERAL_INCLUDE_UVC_MODE
  const auto temp_sensor = esp_camera_sensor_get();

  // Thanks to lick_it, we discovered that OV5640 likes to overheat when
  // running at higher than usual xclk frequencies.
  // Hence, why we're limiting the faster ones for OV2640
  if (const auto camera_id = temp_sensor->id.PID; camera_id == OV5640_PID)
  {
    config.xclk_freq_hz = OV5640_XCLK_FREQ_HZ;
    esp_camera_deinit();
    if (auto const err = esp_camera_init(&config); err == ESP_OK)
    {
      ESP_LOGI(CAMERA_MANAGER_TAG, "OV5640 detected: adjusted XCLK freq to %d Hz", config.xclk_freq_hz);
    }
    else
    {
      ESP_LOGE(CAMERA_MANAGER_TAG, "OV5640 reinit failed after XCLK adjust: %s", esp_err_to_name(err));
    }
  }

  // Minimal per-sensor override logic (only if enabled via Kconfig and different)
  if (temp_sensor)
  {
    const auto pid = temp_sensor->id.PID;
    int desired_freq = 0;
#ifdef CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV2640
    if (pid == OV2640_PID && CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV2640 > 0)
    {
      desired_freq = CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV2640;
    }
#endif
#ifdef CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV3660
    if (pid == OV3660_PID && CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV3660 > 0)
    {
      desired_freq = CONFIG_CAMERA_XCLK_FREQ_OVERRIDE_OV3660;
    }
#endif
    if (desired_freq > 0 && desired_freq != config.xclk_freq_hz)
    {
      ESP_LOGI(CAMERA_MANAGER_TAG, "Applying XCLK override for sensor PID 0x%X: %d Hz (was %d)", pid, desired_freq, config.xclk_freq_hz);
      config.xclk_freq_hz = desired_freq;
      esp_camera_deinit();
      if (auto const err = esp_camera_init(&config); err == ESP_OK)
      {
        ESP_LOGI(CAMERA_MANAGER_TAG, "Camera reinitialized with override frequency");
        ESP_LOGI(CAMERA_MANAGER_TAG, "Final camera XCLK freq: %d Hz (PID 0x%X)", config.xclk_freq_hz, pid);
      }
      else
      {
        ESP_LOGE(CAMERA_MANAGER_TAG, "Failed to reinit camera with override freq (%s). Reverting.", esp_err_to_name(err));
        // Attempt revert to previous frequency
        // (previous value was lost after assignment; we can fall back to USB/WIFI config selection logic if needed)
      }
    }
    else
    {
      ESP_LOGI(CAMERA_MANAGER_TAG, "No XCLK override applied for PID 0x%X. Active freq: %d Hz", pid, config.xclk_freq_hz);
    }
  }

#endif

  this->setupCameraSensor();
  // this->loadConfigData(); // move this to update method once implemented
  return true;
}

void CameraManager::loadConfigData()
{
  ESP_LOGD(CAMERA_MANAGER_TAG, "Loading camera config data");
  CameraConfig_t cameraConfig = projectConfig->getCameraConfig();
  this->setHFlip(cameraConfig.href);
  this->setVFlip(cameraConfig.vflip);
  this->setCameraResolution(static_cast<framesize_t>(cameraConfig.framesize));
  camera_sensor->set_quality(camera_sensor, cameraConfig.quality);
  camera_sensor->set_agc_gain(camera_sensor, cameraConfig.brightness);
  ESP_LOGD(CAMERA_MANAGER_TAG, "Loading camera config data done");
}

int CameraManager::setCameraResolution(const framesize_t frameSize)
{
  if (camera_sensor->pixformat == PIXFORMAT_JPEG)
  {
    return camera_sensor->set_framesize(camera_sensor, frameSize);
  }
  return -1;
}

int CameraManager::setVFlip(const int direction)
{
  return camera_sensor->set_vflip(camera_sensor, direction);
}

int CameraManager::setHFlip(const int direction)
{
  return camera_sensor->set_hmirror(camera_sensor, direction);
}

int CameraManager::setVieWindow(int offsetX,
                                int offsetY,
                                int outputX,
                                int outputY)
{

  // todo safariMonkey made a PoC, implement it here
  return 0;
}