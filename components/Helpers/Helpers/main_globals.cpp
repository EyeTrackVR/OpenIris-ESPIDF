#include "main_globals.hpp"
#include "esp_log.h"

// used to force starting the stream setup process via commands
extern void force_activate_streaming();
// used to start just the stream server without mode changes
extern void force_start_stream_server();

static bool s_startupCommandReceived = false;
bool getStartupCommandReceived()
{
    return s_startupCommandReceived;
}

void setStartupCommandReceived(bool startupCommandReceived)
{
    s_startupCommandReceived = startupCommandReceived;
}

// Global pause state
static bool s_startupPaused = false;
bool getStartupPaused()
{
    return s_startupPaused;
}

void setStartupPaused(bool startupPaused)
{
    s_startupPaused = startupPaused;
}

// Function to manually activate streaming
void activateStreaming(void *arg)
{
    force_activate_streaming();
}

// Function to start just the stream server
void startStreamServerOnly()
{
    force_start_stream_server();
}

// USB handover state
static bool s_usbHandoverDone = false;
bool getUsbHandoverDone() { return s_usbHandoverDone; }
void setUsbHandoverDone(bool done) { s_usbHandoverDone = done; }