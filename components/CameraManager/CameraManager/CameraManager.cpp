#include "CameraManager.hpp"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "img_converters.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

const char* CAMERA_MANAGER_TAG = "[CAMERA_MANAGER]";

namespace
{
// 5x7 pixel font, hex digits 0-F only. Each glyph is 7 rows, bit4 = leftmost column.
// Hand-drawn (not lifted from any font asset) - legibility over accuracy to any real typeface.
constexpr uint8_t FONT_5X7[16][7] = {
    {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},  // 0
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // 1
    {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},  // 2
    {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},  // 3
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},  // 4
    {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},  // 5
    {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},  // 6
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},  // 7
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},  // 8
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},  // 9
    {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // A
    {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},  // b
    {0b01111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b01111},  // C
    {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100},  // d
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},  // E
    {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},  // F
};

constexpr uint16_t FRAME_DIM = 240;
constexpr uint8_t BG_SHADE = 30;
constexpr uint8_t FG_SHADE = 255;

void fillRect(uint8_t* buf, int x0, int y0, int x1, int y1, uint8_t shade)
{
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    x1 = std::min(x1, (int)FRAME_DIM);
    y1 = std::min(y1, (int)FRAME_DIM);
    for (int y = y0; y < y1; y++)
    {
        std::memset(buf + (size_t)y * FRAME_DIM + x0, shade, std::max(0, x1 - x0));
    }
}

// A 3-hex-digit label is a grid of cells GRID_W (17) x GRID_H (7): each character is 5 cols
// wide with a 1-col gap between characters (5+1+5+1+5 = 17), 7 rows tall.
constexpr int GRID_W = 17;
constexpr int GRID_H = 7;

// Rotates a label-local cell coordinate by 0/90/180/270 degrees (clockwise) around the label's
// own origin. Since rotations are multiples of 90 degrees, grid cells map to grid cells exactly -
// no interpolation needed. rotation is 0-3 (x90 degrees).
void rotateCell(int gx, int gy, int rotation, int* outGx, int* outGy)
{
    switch (rotation & 0x3)
    {
    case 0:
        *outGx = gx;
        *outGy = gy;
        break;
    case 1:  // 90 CW
        *outGx = GRID_H - 1 - gy;
        *outGy = gx;
        break;
    case 2:  // 180
        *outGx = GRID_W - 1 - gx;
        *outGy = GRID_H - 1 - gy;
        break;
    default:  // 270 CW
        *outGx = gy;
        *outGy = GRID_W - 1 - gx;
        break;
    }
}

// Draws a 3-hex-digit label rotated by `rotation` (0-3, x90 degrees clockwise), with its
// rotated bounding box's top-left corner at (originX, originY) in the destination buffer.
void drawLabelRotated(uint8_t* buf, int originX, int originY, int scale, int rotation, const uint8_t nibbles[3])
{
    for (int charIdx = 0; charIdx < 3; charIdx++)
    {
        const auto& rows = FONT_5X7[nibbles[charIdx] & 0xF];
        for (int row = 0; row < 7; row++)
        {
            for (int col = 0; col < 5; col++)
            {
                if (!(rows[row] & (1 << (4 - col))))
                    continue;

                const int gx = charIdx * 6 + col;  // 5 cols + 1 gap per character
                const int gy = row;
                int rgx, rgy;
                rotateCell(gx, gy, rotation, &rgx, &rgy);

                const int x = originX + rgx * scale;
                const int y = originY + rgy * scale;
                fillRect(buf, x, y, x + scale, y + scale, FG_SHADE);
            }
        }
    }
}
}  // namespace

CameraManager::CameraManager(std::shared_ptr<ProjectConfig> projectConfig, QueueHandle_t eventQueue) : projectConfig(projectConfig), eventQueue(eventQueue) {}

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
    xclk_freq_hz = CONFIG_CAMERA_USB_XCLK_FREQ;
#endif

    config = {
        .pin_pwdn = CONFIG_PWDN_GPIO_NUM,      // CAM_PIN_PWDN,
        .pin_reset = CONFIG_RESET_GPIO_NUM,    // CAM_PIN_RESET,
        .pin_xclk = CONFIG_XCLK_GPIO_NUM,      // CAM_PIN_XCLK,
        .pin_sccb_sda = CONFIG_SIOD_GPIO_NUM,  // CAM_PIN_SIOD,
        .pin_sccb_scl = CONFIG_SIOC_GPIO_NUM,  // CAM_PIN_SIOC,

        .pin_d7 = CONFIG_Y9_GPIO_NUM,        /// CAM_PIN_D7,
        .pin_d6 = CONFIG_Y8_GPIO_NUM,        /// CAM_PIN_D6,
        .pin_d5 = CONFIG_Y7_GPIO_NUM,        // CAM_PIN_D5,
        .pin_d4 = CONFIG_Y6_GPIO_NUM,        // CAM_PIN_D4,
        .pin_d3 = CONFIG_Y5_GPIO_NUM,        // CAM_PIN_D3,
        .pin_d2 = CONFIG_Y4_GPIO_NUM,        // CAM_PIN_D2,
        .pin_d1 = CONFIG_Y3_GPIO_NUM,        // CAM_PIN_D1,
        .pin_d0 = CONFIG_Y2_GPIO_NUM,        // CAM_PIN_D0,
        .pin_vsync = CONFIG_VSYNC_GPIO_NUM,  // CAM_PIN_VSYNC,
        .pin_href = CONFIG_HREF_GPIO_NUM,    // CAM_PIN_HREF,
        .pin_pclk = CONFIG_PCLK_GPIO_NUM,    // CAM_PIN_PCLK,

        .xclk_freq_hz = xclk_freq_hz,  // Set in config
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,   // YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_240X240,  // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has
                                          // improved a lot, but JPEG mode always gives better frame rates.

        .jpeg_quality = 8,  // 0-63, for OV series camera sensors, lower number means higher quality // Below 6 stability problems
        .fb_count = 2,      // When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
        .fb_location = CAMERA_FB_IN_DRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,  // was CAMERA_GRAB_LATEST; new mode reduces frame skips at cost of minor latency
    };
}

void CameraManager::setupCameraSensor()
{
    ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera sensor");

    camera_sensor = esp_camera_sensor_get();
    // fixes corrupted jpegs, https://github.com/espressif/esp32-camera/issues/203
    // documentation https://www.uctronics.com/download/cam_module/OV2640DS.pdf
    camera_sensor->set_reg(camera_sensor, 0xff, 0xff,
                           0x00);                          // banksel, here we're directly writing to the registers.
                                                           // 0xFF==0x00 is the first bank, there's also 0xFF==0x01
    camera_sensor->set_reg(camera_sensor, 0xd3, 0xff, 5);  // clock
    camera_sensor->set_brightness(camera_sensor, 2);       // -2 to 2
    camera_sensor->set_contrast(camera_sensor, 2);         // -2 to 2
    camera_sensor->set_saturation(camera_sensor, -2);      // -2 to 2

    // white balance control
    camera_sensor->set_whitebal(camera_sensor, 1);  // 0 = disable , 1 = enable
    camera_sensor->set_awb_gain(camera_sensor, 0);  // 0 = disable , 1 = enable
    camera_sensor->set_wb_mode(camera_sensor,
                               0);  // 0 to 4 - if awb_gain enabled (0 - Auto, 1 -
                                    // Sunny, 2 - Cloudy, 3 - Office, 4 - Home)

    // controls the exposure
    camera_sensor->set_exposure_ctrl(camera_sensor,
                                     0);               // 0 = disable , 1 = enable
    camera_sensor->set_aec2(camera_sensor, 0);         // 0 = disable , 1 = enable
    camera_sensor->set_ae_level(camera_sensor, 0);     // -2 to 2
    camera_sensor->set_aec_value(camera_sensor, 300);  // 0 to 1200

    // controls the gain
    camera_sensor->set_gain_ctrl(camera_sensor, 0);  // 0 = disable , 1 = enable

    // automatic gain control gain, controls by how much the resulting image
    // should be amplified
    camera_sensor->set_agc_gain(camera_sensor, 2);                                 // 0 to 30
    camera_sensor->set_gainceiling(camera_sensor, static_cast<gainceiling_t>(6));  // 0 to 6

    // black and white pixel correction, averages the white and black spots
    camera_sensor->set_bpc(camera_sensor, 1);  // 0 = disable , 1 = enable
    camera_sensor->set_wpc(camera_sensor, 1);  // 0 = disable , 1 = enable
    // digital clamp white balance
    camera_sensor->set_dcw(camera_sensor, 0);  // 0 = disable , 1 = enable

    // gamma correction
    camera_sensor->set_raw_gma(camera_sensor,
                               1);  // 0 = disable , 1 = enable (makes much lighter and noisy)

    camera_sensor->set_lenc(camera_sensor, 0);  // 0 = disable , 1 = enable // 0 =
                                                // disable , 1 = enable

    camera_sensor->set_colorbar(camera_sensor, 0);  // 0 = disable , 1 = enable

    camera_sensor->set_special_effect(camera_sensor,
                                      2);  // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint,
                                           // 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)

    // it gets overriden somewhere somehow
    camera_sensor->set_framesize(camera_sensor, FRAMESIZE_240X240);
    ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera sensor done");
}

bool CameraManager::setupCamera()
{
    ESP_LOGI(CAMERA_MANAGER_TAG, "Setting up camera pinout");
    this->setupCameraPinout();
    ESP_LOGI(CAMERA_MANAGER_TAG, "Initializing camera...");

    if (auto const hasCameraBeenInitialized = esp_camera_init(&config); hasCameraBeenInitialized == ESP_OK)
    {
        ESP_LOGI(CAMERA_MANAGER_TAG, "Camera initialized: %s \r\n", esp_err_to_name(hasCameraBeenInitialized));

        this->cameraOk = true;
        constexpr auto event = SystemEvent{EventSource::CAMERA, CameraState_e::Camera_Success};
        xQueueSend(this->eventQueue, &event, 10);
    }
    else
    {
        ESP_LOGE(CAMERA_MANAGER_TAG, "Camera initialization failed with error: %s \r\n", esp_err_to_name(hasCameraBeenInitialized));
        ESP_LOGE(CAMERA_MANAGER_TAG,
                 "Camera most likely not seated properly in the socket. "
                 "Please fix the camera - it'll be retried automatically, no reboot needed.\r\n");
        this->cameraOk = false;
        this->lastCameraError = hasCameraBeenInitialized;
        this->generateDiagnosticFrame();
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
        esp_camera_init(&config);
    }

#endif

    this->setupCameraSensor();
    return true;
}

bool CameraManager::isCameraOk() const
{
    return cameraOk;
}

namespace
{
void CameraRetryTask(void* param)
{
    auto* self = static_cast<CameraManager*>(param);
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!self->isCameraOk())
        {
            ESP_LOGI(CAMERA_MANAGER_TAG, "Camera not initialized, retrying...");
            self->setupCamera();
        }
    }
}
}  // namespace

void CameraManager::startAutoRetry()
{
    xTaskCreate(CameraRetryTask, "CameraRetryTask", 1024 * 3, this, 1, nullptr);
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

int CameraManager::setVieWindow(int offsetX, int offsetY, int outputX, int outputY)
{
    // todo safariMonkey made a PoC, implement it here
    return 0;
}

esp_err_t CameraManager::getLastCameraError() const
{
    return lastCameraError;
}

bool CameraManager::getDiagnosticFrame(const uint8_t** outBuf, size_t* outLen) const
{
    if (!diagnosticJpegBuf || diagnosticJpegLen == 0)
        return false;

    *outBuf = diagnosticJpegBuf;
    *outLen = diagnosticJpegLen;
    return true;
}

void CameraManager::generateDiagnosticFrame()
{
    if (diagnosticJpegBuf)
    {
        free(diagnosticJpegBuf);
        diagnosticJpegBuf = nullptr;
        diagnosticJpegLen = 0;
    }

    auto* raw = static_cast<uint8_t*>(heap_caps_malloc((size_t)FRAME_DIM * FRAME_DIM, MALLOC_CAP_SPIRAM));
    if (!raw)
    {
        ESP_LOGE(CAMERA_MANAGER_TAG, "Failed to allocate diagnostic frame buffer");
        return;
    }

    std::memset(raw, BG_SHADE, (size_t)FRAME_DIM * FRAME_DIM);

    // The error code magnitude fits comfortably in 3 hex digits for every esp_err_t this
    // component can produce (e.g. ESP_ERR_NOT_FOUND = 0x105), so we only render 3.
    const uint32_t code = static_cast<uint32_t>(std::abs(static_cast<int>(lastCameraError))) & 0xFFF;
    const uint8_t nibbles[3] = {
        static_cast<uint8_t>((code >> 8) & 0xF),
        static_cast<uint8_t>((code >> 4) & 0xF),
        static_cast<uint8_t>(code & 0xF),
    };

    // Eye-tracking clients (Baballonia et al.) crop/zoom this frame toward whatever they guess
    // is the pupil, so a single centered label isn't reliable - it can land almost anywhere,
    // rotated or clipped, and blow up into an unreadable blob.
    // Tile small instances of the code across the frame in a grid, each at a different 90-degree
    // rotation, so whatever crop/rotation the client applies, at least one instance lands both
    // inside the crop window AND right-side-up.
    constexpr int scale = 3;  // pixels per font cell
    constexpr int labelPxW = GRID_W * scale;
    constexpr int labelPxH = GRID_H * scale;
    // Max footprint across all 4 rotations is a square of the longer dimension, so every tile
    // gets the same amount of clearance regardless of which rotation lands there.
    constexpr int cellSpan = (labelPxW > labelPxH ? labelPxW : labelPxH);
    constexpr int centerXs[3] = {14 + cellSpan / 2, FRAME_DIM / 2, FRAME_DIM - 14 - cellSpan / 2};
    constexpr int centerYs[3] = {14 + cellSpan / 2, FRAME_DIM / 2, FRAME_DIM - 14 - cellSpan / 2};

    int rotation = 0;
    for (int cy : centerYs)
    {
        for (int cx : centerXs)
        {
            const bool swapped = (rotation & 1) != 0;  // 90/270 swap the bounding box's W/H
            const int w = swapped ? labelPxH : labelPxW;
            const int h = swapped ? labelPxW : labelPxH;
            drawLabelRotated(raw, cx - w / 2, cy - h / 2, scale, rotation, nibbles);
            rotation = (rotation + 1) & 0x3;
        }
    }

    // Thin border so it's obviously a diagnostic card rather than a corrupted stream frame.
    fillRect(raw, 0, 0, FRAME_DIM, 4, FG_SHADE);
    fillRect(raw, 0, FRAME_DIM - 4, FRAME_DIM, FRAME_DIM, FG_SHADE);
    fillRect(raw, 0, 0, 4, FRAME_DIM, FG_SHADE);
    fillRect(raw, FRAME_DIM - 4, 0, FRAME_DIM, FRAME_DIM, FG_SHADE);

    uint8_t* jpegOut = nullptr;
    size_t jpegLen = 0;
    const bool ok = fmt2jpg(raw, (size_t)FRAME_DIM * FRAME_DIM, FRAME_DIM, FRAME_DIM, PIXFORMAT_GRAYSCALE, 30, &jpegOut, &jpegLen);
    free(raw);

    if (!ok)
    {
        ESP_LOGE(CAMERA_MANAGER_TAG, "Failed to encode diagnostic frame to JPEG");
        return;
    }

    diagnosticJpegBuf = jpegOut;
    diagnosticJpegLen = jpegLen;
    ESP_LOGI(CAMERA_MANAGER_TAG, "Generated diagnostic frame for error 0x%03lx (%u bytes)", (unsigned long)code, (unsigned)jpegLen);
}