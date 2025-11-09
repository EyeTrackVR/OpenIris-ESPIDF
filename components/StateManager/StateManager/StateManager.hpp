#pragma once
#ifndef STATEMANAGER_HPP
#define STATEMANAGER_HPP
#include <variant>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// LED status categories
// Naming kept stable for existing queues; documented meanings added.
enum class LEDStates_e
{
  LedStateNone,           // Idle / no indication (LED off)
  LedStateStreaming,      // Active streaming (UVC or WiFi) – steady ON
  LedStateStoppedStreaming, // Streaming stopped intentionally – steady OFF (could differentiate later)
  CameraError,            // Camera init / runtime failure – double blink pattern
  CameraDataError,        // Repeated NO-SOI (JPEG start marker missing) – distinct rapid pattern
  WiFiStateError,         // WiFi connection error – distinctive blink sequence
  WiFiStateConnecting,    // WiFi association / DHCP pending – slow blink
  WiFiStateConnected      // WiFi connected (momentary confirmation burst)
};

enum class WiFiState_e
{
  WiFiState_NotInitialized,
  WiFiState_Initialized,
  WiFiState_ReadyToConnect,
  WiFiState_Connecting,
  WiFiState_WaitingForIp,
  WiFiState_Connected,
  WiFiState_Disconnected,
  WiFiState_Error
};

enum class MDNSState_e
{
  MDNSState_Stopped,
  MDNSState_Starting,
  MDNSState_Started,
  MDNSState_Stopping,
  MDNSState_Error,
  MDNSState_QueryStarted,
  MDNSState_QueryComplete
};

enum class CameraState_e
{
  Camera_Disconnected,
  Camera_Success,
  Camera_Error
};

enum class StreamState_e
{
  Stream_OFF,
  Stream_ON,
};

enum class EventSource
{
  WIFI,
  MDNS,
  CAMERA,
  STREAM
};

struct SystemEvent
{
  EventSource source;
  std::variant<WiFiState_e, MDNSState_e, CameraState_e, StreamState_e> value;
};

class StateManager
{
public:
  StateManager(QueueHandle_t eventQueue, QueueHandle_t ledStateQueue);
  void HandleUpdateState();
  WiFiState_e GetWifiState();
  CameraState_e GetCameraState();

private:
  QueueHandle_t eventQueue;
  QueueHandle_t ledStateQueue;

  WiFiState_e wifi_state;
  MDNSState_e mdns_state;
  CameraState_e camera_state;
  StreamState_e stream_state;
};

void HandleStateManagerTask(void *pvParameters);

#endif // STATEMANAGER_HPP
