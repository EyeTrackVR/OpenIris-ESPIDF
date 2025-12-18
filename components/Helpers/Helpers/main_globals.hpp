#pragma once
#ifndef MAIN_GLOBALS_HPP
#define MAIN_GLOBALS_HPP

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Function to manually activate streaming
// designed to be scheduled as a task
// so that the serial manager has time to return the response
void activateStreaming(void *arg);

// Starts only the stream server (used by startStreamingCommand in SETUP mode)
void startStreamServerOnly();

bool getStartupCommandReceived();
void setStartupCommandReceived(bool startupCommandReceived);

bool getStartupPaused();
void setStartupPaused(bool startupPaused);

// Tracks whether USB handover from usb_serial_jtag to TinyUSB was performed
bool getUsbHandoverDone();
void setUsbHandoverDone(bool done);

#endif