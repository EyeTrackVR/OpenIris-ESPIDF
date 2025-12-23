#include "StateManager.hpp"

StateManager::StateManager(QueueHandle_t eventQueue, QueueHandle_t ledStateQueue) : eventQueue(eventQueue), ledStateQueue(ledStateQueue) {}

void StateManager::HandleUpdateState()
{
  SystemEvent eventBuffer;
  auto ledStreamState = LEDStates_e::LedStateNone;

  if (xQueueReceive(this->eventQueue, &eventBuffer, portMAX_DELAY))
  {
    switch (eventBuffer.source)
    {

    case EventSource::WIFI:
    {
      this->wifi_state = std::get<WiFiState_e>(eventBuffer.value);

      if (this->wifi_state == WiFiState_e::WiFiState_Connecting)
      {
        ledStreamState = LEDStates_e::WiFiStateConnecting;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }
      if (this->wifi_state == WiFiState_e::WiFiState_Connected)
      {
        ledStreamState = LEDStates_e::WiFiStateConnected;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }
      if (this->wifi_state == WiFiState_e::WiFiState_Error)
      {
        ledStreamState = LEDStates_e::WiFiStateError;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }

      break;
    }

    case EventSource::MDNS:
    {
      this->mdns_state = std::get<MDNSState_e>(eventBuffer.value);
      break;
    }

    case EventSource::CAMERA:
    {
      this->camera_state = std::get<CameraState_e>(eventBuffer.value);

      if (this->camera_state == CameraState_e::Camera_Error)
      {
        ledStreamState = LEDStates_e::CameraError;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }

      break;
    }

    case EventSource::STREAM:
    {
      this->stream_state = std::get<StreamState_e>(eventBuffer.value);

      if (this->stream_state == StreamState_e::Stream_ON)
      {
        ledStreamState = LEDStates_e::LedStateStreaming;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }
      else if (this->stream_state == StreamState_e::Stream_OFF)
      {
        ledStreamState = LEDStates_e::LedStateStoppedStreaming;
        xQueueSend(this->ledStateQueue, &ledStreamState, 10);
      }
      break;
    }

    default:
      break;
    }
  }
}

WiFiState_e StateManager::GetWifiState()
{
  return this->wifi_state;
}

CameraState_e StateManager::GetCameraState()
{
  return this->camera_state;
}

QueueHandle_t StateManager::GetEventQueue() const
{
  return this->eventQueue;
}

void HandleStateManagerTask(void *pvParameters)
{
  auto *stateManager = static_cast<StateManager *>(pvParameters);

  while (true)
  {
    stateManager->HandleUpdateState();
  }
}