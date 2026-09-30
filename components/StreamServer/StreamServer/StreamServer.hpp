#pragma once
#ifndef STREAMSERVER_HPP
#define STREAMSERVER_HPP

#define PART_BOUNDARY "123456789000000000000987654321"

#include <CameraManager.hpp>
#include <StateManager.hpp>
#include <WebSocketLogger.hpp>
#include <helpers.hpp>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"

extern WebSocketLogger webSocketLogger;

// Passed as httpd_req_t::user_ctx so the stream handler can fall back to a diagnostic
// frame (see CameraManager::getDiagnosticFrame) when the camera failed to init.
struct StreamUserCtx
{
    StateManager* stateManager;
    CameraManager* cameraManager;
};

namespace StreamHelpers
{
esp_err_t stream(httpd_req_t* req);
esp_err_t ws_logs_handle(httpd_req_t* req);
}  // namespace StreamHelpers

class StreamServer
{
   private:
    int STREAM_SERVER_PORT;
    StateManager* stateManager;
    std::shared_ptr<CameraManager> cameraManager;
    StreamUserCtx userCtx{};
    httpd_handle_t camera_stream = nullptr;

   public:
    StreamServer(const int STREAM_PORT, StateManager* StateManager, std::shared_ptr<CameraManager> cameraManager);
    esp_err_t startStreamServer();

    esp_err_t stream(httpd_req_t* req);
    esp_err_t ws_logs_handle(httpd_req_t* req);
};

#endif