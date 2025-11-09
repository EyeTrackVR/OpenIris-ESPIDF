#include "UVCStream.hpp"
#include <cstdio> // for snprintf
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// no deps on main globals here; handover is performed in main before calling setup when needed

static const char *UVC_STREAM_TAG = "[UVC DEVICE]";

// Tracks whether a frame has been handed to TinyUSB and not yet returned.
// File scope so both get_cb and return_cb can access it safely.
static bool s_frame_inflight = false;

extern "C"
{
  static char serial_number_str[13];

  const char *get_uvc_device_name()
  {
    return deviceConfig->getMDNSConfig().hostname.c_str();
  }

  const char *get_serial_number(void)
  {
    if (serial_number_str[0] == '\0')
    {
      uint8_t mac_address[6];
      esp_err_t result = esp_efuse_mac_get_default(mac_address);
      if (result != ESP_OK)
      {
        ESP_LOGE(UVC_STREAM_TAG, "Failed to get MAC address of the board, returning default serial number");
        return CONFIG_TUSB_SERIAL_NUM;
      }

      // 12 hex chars without separators
      snprintf(serial_number_str, sizeof(serial_number_str), "%02X%02X%02X%02X%02X%02X",
               mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    }
    return serial_number_str;
  }
}

// single definition of shared framebuffer storage
UVCStreamHelpers::fb_t UVCStreamHelpers::s_fb = {};

static esp_err_t UVCStreamHelpers::camera_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
  ESP_LOGI(UVC_STREAM_TAG, "Camera Start");
  ESP_LOGI(UVC_STREAM_TAG, "Format: %d, width: %d, height: %d, rate: %d", format, width, height, rate);
  framesize_t frame_size = FRAMESIZE_QVGA;

  if (format != UVC_FORMAT_JPEG)
  {
    ESP_LOGE(UVC_STREAM_TAG, "Only support MJPEG format");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (width == 240 && height == 240)
  {
    frame_size = FRAMESIZE_240X240;
  }
  else
  {
    ESP_LOGE(UVC_STREAM_TAG, "Unsupported frame size %dx%d", width, height);
    return ESP_ERR_NOT_SUPPORTED;
  }

  cameraHandler->setCameraResolution(frame_size);

  constexpr SystemEvent event = {EventSource::STREAM, StreamState_e::Stream_ON};
  xQueueSend(eventQueue, &event, 10);

  return ESP_OK;
}

static void UVCStreamHelpers::camera_stop_cb(void *cb_ctx)
{
  (void)cb_ctx;
  if (s_fb.cam_fb_p)
  {
    esp_camera_fb_return(s_fb.cam_fb_p);
    s_fb.cam_fb_p = nullptr;
  }

  constexpr SystemEvent event = {EventSource::STREAM, StreamState_e::Stream_OFF};
  xQueueSend(eventQueue, &event, 10);
}

static uvc_fb_t *UVCStreamHelpers::camera_fb_get_cb(void *cb_ctx)
{
  auto *mgr = static_cast<UVCStreamManager *>(cb_ctx);

  // Guard against requesting a new frame while previous is still in flight.
  // This was causing intermittent corruption/glitches because the pointer
  // to the underlying camera buffer was overwritten before TinyUSB returned it.

  // --- Frame pacing BEFORE grabbing a new camera frame ---
  static int64_t next_deadline_us = 0;    // next permitted capture time
  static int rem_acc = 0;                 // fractional remainder accumulator
  static const int target_fps = 60;          // desired FPS
  static const int64_t us_per_sec = 1000000; // 1e6 microseconds
  static const int base_interval_us = us_per_sec / target_fps; // 16666
  static const int rem_us = us_per_sec % target_fps;           // 40 (distributed)

  const int64_t now_us = esp_timer_get_time();
  if (next_deadline_us == 0)
  {
    // First allowed capture immediately
    next_deadline_us = now_us;
  }

  // If a frame is still being transmitted or we are too early, just signal no frame
  if (s_frame_inflight || now_us < next_deadline_us)
  {
    return nullptr; // host will poll again
  }

  // Acquire a fresh frame only when allowed and no frame in flight
  camera_fb_t *cam_fb = esp_camera_fb_get();
  if (!cam_fb)
  {
    return nullptr;
  }

  s_fb.cam_fb_p = cam_fb;
  s_fb.uvc_fb.buf = cam_fb->buf;
  s_fb.uvc_fb.len = cam_fb->len;
  s_fb.uvc_fb.width = cam_fb->width;
  s_fb.uvc_fb.height = cam_fb->height;
  s_fb.uvc_fb.format = UVC_FORMAT_JPEG;
  s_fb.uvc_fb.timestamp = cam_fb->timestamp;

  // Validate size fits into transfer buffer
  if (mgr && s_fb.uvc_fb.len > mgr->getUvcBufferSize())
  {
    ESP_LOGE(UVC_STREAM_TAG, "Frame size %d exceeds UVC buffer size %u", (int)s_fb.uvc_fb.len, (unsigned)mgr->getUvcBufferSize());
    esp_camera_fb_return(cam_fb);
    s_fb.cam_fb_p = nullptr;
    return nullptr;
  }

  // Schedule next frame time (distribute remainder for exact long‑term 60.000 fps)
  rem_acc += rem_us;
  int extra_us = 0;
  if (rem_acc >= target_fps)
  {
    rem_acc -= target_fps;
    extra_us = 1;
  }
  const int64_t candidate_next = next_deadline_us + base_interval_us + extra_us;
  next_deadline_us = (candidate_next < now_us) ? now_us : candidate_next;

  s_frame_inflight = true;
  return &s_fb.uvc_fb;
}

static void UVCStreamHelpers::camera_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
  (void)cb_ctx;
  assert(fb == &s_fb.uvc_fb);
  if (s_fb.cam_fb_p)
  {
    esp_camera_fb_return(s_fb.cam_fb_p);
    s_fb.cam_fb_p = nullptr;
  }
  s_frame_inflight = false;
}

esp_err_t UVCStreamManager::setup()
{

#ifndef CONFIG_GENERAL_INCLUDE_UVC_MODE
  ESP_LOGE(UVC_STREAM_TAG, "The board does not support UVC, please, setup WiFi connection.");
  return ESP_FAIL;
#endif

  ESP_LOGI(UVC_STREAM_TAG, "Setting up UVC Stream");
  // Allocate a fixed-size transfer buffer (compile-time constant)
  uvc_buffer_size = UVCStreamManager::UVC_MAX_FRAMESIZE_SIZE;
  uvc_buffer = static_cast<uint8_t *>(malloc(uvc_buffer_size));
  if (uvc_buffer == nullptr)
  {
    ESP_LOGE(UVC_STREAM_TAG, "Allocating buffer for UVC Device failed");
    return ESP_FAIL;
  }

  uvc_device_config_t config = {
      .uvc_buffer = uvc_buffer,
      .uvc_buffer_size = UVCStreamManager::UVC_MAX_FRAMESIZE_SIZE,
      .start_cb = UVCStreamHelpers::camera_start_cb,
      .fb_get_cb = UVCStreamHelpers::camera_fb_get_cb,
      .fb_return_cb = UVCStreamHelpers::camera_fb_return_cb,
      .stop_cb = UVCStreamHelpers::camera_stop_cb,
      .cb_ctx = this,
  };

  esp_err_t ret = uvc_device_config(0, &config);
  if (ret != ESP_OK)
  {
    ESP_LOGE(UVC_STREAM_TAG, "Configuring UVC Device failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(UVC_STREAM_TAG, "Configured UVC Device");

  ESP_LOGI(UVC_STREAM_TAG, "Initializing UVC Device");
  ret = uvc_device_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(UVC_STREAM_TAG, "Initializing UVC Device failed: %s", esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(UVC_STREAM_TAG, "Initialized UVC Device");

  return ESP_OK;
}

esp_err_t UVCStreamManager::start()
{
  ESP_LOGI(UVC_STREAM_TAG, "Starting UVC streaming");
  // UVC device is already initialized in setup(), just log that we're starting
  return ESP_OK;
}