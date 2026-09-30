# Camera diagnostic frame - error code reference

When `esp_camera_init()` fails, `CameraManager::generateDiagnosticFrame()` renders the
returned `esp_err_t` as a 3-hex-digit code (e.g. `105`) tiled across the fallback MJPEG
frame instead of a live picture, so eye-tracking clients (Baballonia, EyeTrackVR, etc.)
show *something* explanatory instead of a dead feed.

**Only the low 3 hex digits are shown.** That's enough to disambiguate every code this
component can realistically produce (see table below), but it isn't unique across the
whole `esp_err_t` space - see [Codes outside this range](#codes-outside-this-range).

## Codes reachable from `esp_camera_init()`

These come from `esp_camera_init()` (`managed_components/espressif__esp32-camera/driver/esp_camera.c`),
specifically `camera_probe()` / `SCCB_Probe()` for the sensor detection step.

| Code    | Name                     | Meaning                                                              | Typical cause |
|---------|--------------------------|-----------------------------------------------------------------------|----------------|
| `0x105` | `ESP_ERR_NOT_FOUND`      | SCCB (I2C) probe got no ACK from any known sensor address             | Camera sensor not seated in its socket, bad ribbon/connector, or dead sensor. |
| `0x101` | `ESP_ERR_NO_MEM`         | Frame buffer / DMA allocation failed                                   | PSRAM not detected or exhausted before camera init. |
| `0x102` | `ESP_ERR_INVALID_ARG`    | Bad `camera_config_t` (pins, `xclk_freq_hz`, format/framesize combo)   | Firmware bug, not a hardware fault - check `CameraManager::setupCameraPinout()` against the board's Kconfig pin defaults. |
| `0x103` | `ESP_ERR_INVALID_STATE`  | `esp_camera_init()` called twice without `esp_camera_deinit()`         | Firmware bug (double-init) - see the OV5640-specific reinit path in `CameraManager::setupCamera()`. |

## Full base `esp_err_t` table (for reference)

Every generic ESP-IDF error code fits in 3 hex digits, so if the frame ever shows one of
these it's unambiguous:

| Code    | Name                       |
|---------|----------------------------|
| `0x101` | `ESP_ERR_NO_MEM`           |
| `0x102` | `ESP_ERR_INVALID_ARG`      |
| `0x103` | `ESP_ERR_INVALID_STATE`    |
| `0x104` | `ESP_ERR_INVALID_SIZE`     |
| `0x105` | `ESP_ERR_NOT_FOUND`        |
| `0x106` | `ESP_ERR_NOT_SUPPORTED`    |
| `0x107` | `ESP_ERR_TIMEOUT`          |
| `0x108` | `ESP_ERR_INVALID_RESPONSE` |
| `0x109` | `ESP_ERR_INVALID_CRC`      |
| `0x10A` | `ESP_ERR_INVALID_VERSION`  |
| `0x10B` | `ESP_ERR_INVALID_MAC`      |
| `0x10C` | `ESP_ERR_NOT_FINISHED`     |
| `0x10D` | `ESP_ERR_NOT_ALLOWED`      |

(Source: `esp-idf/components/esp_common/include/esp_err.h`.)

## Codes outside this range

`esp32-camera` also defines its own error base, which does **not** fit in 3 hex digits and
will get silently truncated by the diagnostic frame's `& 0xFFF` mask:

| Full code | Name                                       | Would render as |
|-----------|---------------------------------------------|------------------|
| `0x20001` | `ESP_ERR_CAMERA_NOT_DETECTED`                | `001` |
| `0x20002` | `ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE`     | `002` |
| `0x20003` | `ESP_ERR_CAMERA_FAILED_TO_SET_OUT_FORMAT`     | `003` |
| `0x20004` | `ESP_ERR_CAMERA_NOT_SUPPORTED`                | `004` |

(Source: `managed_components/espressif__esp32-camera/driver/include/esp_camera.h`.)

`esp_camera_init()` itself (`esp_camera.c`, ~lines 278-293, via `camera_probe()`/`SCCB_Probe()`)
only ever returns `ESP_ERR_NOT_FOUND` (`0x105`) on a missing sensor. The `ESP_ERR_CAMERA_*`
codes above aren't returned from the init path at all - they live in the unrelated
`esp_camera_save_to_nvs()` / `esp_camera_load_from_nvs()` helpers, which `CameraManager`
doesn't call. So they're currently unreachable through this diagnostic path, not just an
edge case of it. If that ever changes (e.g. a future change starts persisting camera
settings through those NVS helpers and surfacing their errors here), the frame would need
to render 4+ hex digits (or special-case the `0x2xxxx` camera error base) to stay
unambiguous - see `CameraManager::generateDiagnosticFrame()`.
