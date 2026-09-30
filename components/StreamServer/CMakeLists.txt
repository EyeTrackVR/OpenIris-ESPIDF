idf_component_register(SRCS "StreamServer/StreamServer.cpp"
  INCLUDE_DIRS "StreamServer"
  REQUIRES esp32-camera StateManager ProjectConfig esp_http_server Helpers WebSocketLogger CameraManager
)