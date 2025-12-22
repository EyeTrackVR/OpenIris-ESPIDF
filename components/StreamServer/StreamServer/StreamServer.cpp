#include "StreamServer.hpp"

constexpr static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
constexpr static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
constexpr static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lli.%06li\r\n\r\n";

static const char *STREAM_SERVER_TAG = "[STREAM_SERVER]";

StreamServer::StreamServer(const int STREAM_PORT, StateManager *stateManager) : STREAM_SERVER_PORT(STREAM_PORT), stateManager(stateManager)
{
}

esp_err_t StreamHelpers::stream(httpd_req_t *req)
{
  camera_fb_t *fb = nullptr;
  struct timeval _timestamp;

  esp_err_t response = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = nullptr;

  // Buffer for multipart header; was mistakenly declared as array of pointers
  char part_buf[256];
  static int64_t last_frame = 0;
  if (!last_frame)
    last_frame = esp_timer_get_time();

  response = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (response != ESP_OK)
    return response;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");

  while (true)
  {
    fb = esp_camera_fb_get();

    if (!fb)
    {
      ESP_LOGE(STREAM_SERVER_TAG, "Camera capture failed");
      response = ESP_FAIL;
      // Don't break immediately, try to recover
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    else
    {
      _timestamp.tv_sec = fb->timestamp.tv_sec;
      _timestamp.tv_usec = fb->timestamp.tv_usec;
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }
    if (response == ESP_OK)
      response = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (response == ESP_OK)
    {
      size_t hlen = snprintf((char *)part_buf, sizeof(part_buf), STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
      response = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (response == ESP_OK)
      response = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    if (fb)
    {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    }
    else if (_jpg_buf)
    {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (response != ESP_OK)
      break;

    if (esp_log_level_get(STREAM_SERVER_TAG) >= ESP_LOG_INFO)
    {
      static long last_request_time = 0;
      if (last_request_time == 0)
      {
        last_request_time = Helpers::getTimeInMillis();
      }

      // Only log every 100 frames to reduce overhead
      const int frame_window = 100;
      static int frame_count = 0;
      if (++frame_count % frame_window == 0)
      {
        long request_end = Helpers::getTimeInMillis();
        long window_ms = request_end - last_request_time;
        last_request_time = request_end;
        long fps = 0;
        if (window_ms > 0)
        {
          fps = (frame_window * 1000) / window_ms;
        }
        ESP_LOGI(STREAM_SERVER_TAG, "%i Frames Size: %uKB, Time: %lims (%lifps)",frame_window, _jpg_buf_len / 1024, window_ms, fps);
      }
    }
  }
  last_frame = 0;
  return response;
}

esp_err_t StreamHelpers::ws_logs_handle(httpd_req_t *req)
{
  auto ret = webSocketLogger.register_socket_client(req);
  return ret;
}

esp_err_t StreamServer::startStreamServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 20480;
  config.max_uri_handlers = 10;
  config.server_port = STREAM_SERVER_PORT;
  config.ctrl_port = STREAM_SERVER_PORT;
  config.recv_wait_timeout = 5;   // 5 seconds for receiving
  config.send_wait_timeout = 5;   // 5 seconds for sending
  config.lru_purge_enable = true; // Enable LRU purge for better connection handling

  httpd_uri_t stream_page = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = &StreamHelpers::stream,
      .user_ctx = nullptr,
  };

  httpd_uri_t logs_ws = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = &StreamHelpers::ws_logs_handle,
      .user_ctx = nullptr,
      .is_websocket = true,
  };

  int status = httpd_start(&camera_stream, &config);

  if (status != ESP_OK)
  {
    ESP_LOGE(STREAM_SERVER_TAG, "Cannot start stream server.");
    return status;
  }

  httpd_register_uri_handler(camera_stream, &logs_ws);
  if (this->stateManager->GetCameraState() != CameraState_e::Camera_Success)
  {
    ESP_LOGE(STREAM_SERVER_TAG, "Camera not initialized. Cannot start stream server. Logs server will be running.");
    return ESP_FAIL;
  }

  httpd_register_uri_handler(camera_stream, &stream_page);

  ESP_LOGI(STREAM_SERVER_TAG, "Stream server started on port %d", STREAM_SERVER_PORT);

  return ESP_OK;
}