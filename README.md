| Supported Targets | ESP32-S3 · Project Babble · FaceFocusVR |
| ----------------- | --------------------------------------- |

## OpenIris-ESPIDF

Firmware and tools for OpenIris — Wi‑Fi, UVC streaming, and a Python setup CLI.

---

## What’s inside

- ESP‑IDF firmware (C/C++) with modules for Camera, Wi‑Fi, UVC, REST/Serial commands, and more
- Python tools for setup over USB serial:
  - `tools/switchBoardType.py` — choose a board profile (builds the right sdkconfig)
  - `tools/setup_openiris.py` — interactive CLI for Wi‑Fi, MDNS/Name, Mode, LED PWM, Logs, and a Settings Summary
 - Composite USB (UVC + CDC) when UVC mode is enabled (`GENERAL_INCLUDE_UVC_MODE`) for simultaneous video streaming and command channel
 - LED current monitoring (if enabled via `MONITORING_LED_CURRENT`) with filtered mA readings
 - Configurable debug LED + external IR LED control with optional error mirroring (`LED_DEBUG_ENABLE`, `LED_EXTERNAL_AS_DEBUG`)
 - Auto‑discovered per‑board configuration overlays under `boards/`
 - Command framework (JSON over serial / CDC / REST) for mode switching, Wi‑Fi config, OTA credentials, LED brightness, info & monitoring
 - Single source advertised name (`CONFIG_GENERAL_ADVERTISED_NAME`) used for both UVC device name and mDNS hostname (unless overridden at runtime)

---

## First-time setup on Windows (VS Code + ESP‑IDF extension)

If you’re starting fresh on Windows, this workflow is smooth and reliable:

1. Install tooling

- Git: https://git-scm.com/downloads/win
- Visual Studio Code: https://code.visualstudio.com/

2. Get the source code

- Create a folder where you want the repo (e.g., `D:\OpenIris-ESPIDF\`). In File Explorer, right‑click the folder and choose “Open in Terminal”.
- Clone and open in VS Code:

```cmd
git clone https://github.com/lorow/OpenIris-ESPIDF.git
cd OpenIris-ESPIDF
code .
```

3. Install the ESP‑IDF VS Code extension

- In VS Code, open the Extensions tab and install: https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension

4. Set the default terminal profile to Command Prompt

- Press Ctrl+Shift+P → search “Terminal: Select Default Profile” → choose “Command Prompt”.
- Restart VS Code from its normal shortcut (not from Git Bash). This avoids running ESP‑IDF in the wrong shell.

5. Configure ESP‑IDF in the extension

- On first launch, the extension may prompt to install ESP‑IDF and tools — follow the steps. It can take a while.
- If you see the extension’s home page instead, click “Configure extension”, pick “EXPRESS”, choose “GitHub” as the server and version “v5.4.2”.
- Then open the ESP‑IDF Explorer tab and click “Open ESP‑IDF Terminal”. We’ll use that for builds.

After this, you’re ready for the Quick start below.

---

## Quick start

### 1) Pick your board (loads the default configuration)
Boards are auto‑discovered from the `boards/` directory. First list them, then pick one:

Windows (cmd):

```cmd
python .\tools\switchBoardType.py --list
python .\tools\switchBoardType.py --board seed_studio_xiao_esp32s3 --diff
```

macOS/Linux (bash):

```bash
python3 ./tools/switchBoardType.py --list
python3 ./tools/switchBoardType.py --board seed_studio_xiao_esp32s3 --diff
```
Notes:
- Use `--list` to see all detected board keys.
- Board key = relative path under `boards/` with `/` replaced by `_` (and duplicate tail segments collapsed, e.g. `project_babble/project_babble` -> `project_babble`).
- `--diff` shows what will change vs the current `sdkconfig`.
- You can also pass partial or path‑like inputs (e.g. `facefocusvr/eye_L`), the tool normalizes them.

### 2) Build & flash

- Set the target (e.g., ESP32‑S3).
- Build, flash, and open the serial monitor.
 - (Optional) For UVC mode ensure `GENERAL_INCLUDE_UVC_MODE=y`. If you want device to boot directly into UVC: also set `START_IN_UVC_MODE=y`.
 - Disable Wi‑Fi services for pure wired builds: `GENERAL_ENABLE_WIRELESS=n`.

### 3) Use the Python setup CLI (recommended)

Configure the device over USB serial.

Before you run it:

- If you still have the serial monitor open, close it (the port must be free).
- In VS Code, open the sidebar “ESP‑IDF: Explorer” and click “Open ESP‑IDF Terminal”. We’ll run the CLI there so Python and packages are in the right environment.

Then run:

```cmd
python .\tools\setup_openiris.py --port COMxx
```

Examples:

- Windows: `python .\tools\setup_openiris.py --port COM69`, …
- macOS: idk
- Linux: idk

What the CLI can do:

- Wi‑Fi menu: automatic (scan → pick → password → connect → wait for IP) or manual (scan, show, configure, connect, status)
- Set MDNS/Device name (also used for the UVC device name)
- Switch mode (Wi‑Fi / UVC / Setup)
- Adjust LED PWM
- Show a Settings Summary (MAC, Wi‑Fi status, mode, PWM, …)
- View logs

---

## Serial number & MAC

- Internally, the serial number is derived from the Wi‑Fi MAC address.
- The CLI displays the MAC by default (clearer); it’s the value used as the serial number.
- The UVC device name is based on the MDNS hostname.

## Advertised Name (UVC + mDNS)
`CONFIG_GENERAL_ADVERTISED_NAME` (Kconfig) defines the base name announced over:
- USB UVC descriptor (appears in OS camera list)
- mDNS hostname / service name

Runtime override: If the setup CLI (or a JSON command) provides a new device name, that value supersedes the compile-time default until next flash/reset of settings.

---

## Common workflows

- Fast Wi‑Fi setup: in the CLI, go to “Wi‑Fi settings” → “Automatic setup”, then check “status”.
- Change name/MDNS: set the device name in the CLI, then replug USB — UVC will show the new name.
- Adjust brightness/LED: set LED PWM in the CLI.
 - Switch to UVC mode over commands (CDC/serial):
   `{"commands":[{"command":"switch_mode","data":{"mode":"uvc"}}]}` then reboot.
 - Read filtered LED current (if enabled):
   `{"commands":[{"command":"get_led_current"}]}`

---

## Project layout (short)

- `main/` — entry point
- `components/` — modules (Camera, WiFi, UVC, CommandManager, …)
- `tools/` — Python helper tools (board switch, setup CLI, scanner)

If you want to dig deeper: commands are mapped via the `CommandManager` under `components/CommandManager/...`.

---

## Troubleshooting

### USB Composite (UVC + CDC)
When UVC support is compiled in, the device enumerates as a composite USB device:
- UVC interface: video streaming (JPEG frames)
- CDC (virtual COM): command channel accepting newline‑terminated JSON objects

Example newline‑terminated JSON commands over CDC (one per line):
```
{"commands":[{"command":"ping"}]}
{"commands":[{"command":"get_who_am_i"}]}
{"commands":[{"command":"switch_mode","data":{"mode":"wifi"}}]}
```

Chained commands in a single request (processed in order):
```
{"commands":[
  {"command":"set_mdns","data":{"hostname":"tracker"}},
  {"command":"set_wifi","data":{"name":"main","ssid":"your_network","password":"password","channel":0,"power":0}}
]}
```
Responses are JSON blobs flushed immediately.

---

### Monitoring (LED Current)
Enabled with `MONITORING_LED_CURRENT=y` plus shunt/gain settings. The task samples every `CONFIG_MONITORING_LED_INTERVAL_MS` ms and maintains a filtered moving average over `CONFIG_MONITORING_LED_SAMPLES` samples. Use `get_led_current` command to query.

### Debug & External LED Configuration
| Kconfig | Effect |
|---------|--------|
| LED_DEBUG_ENABLE | Enables/disables discrete status LED GPIO init & drive |
| LED_EXTERNAL_CONTROL | Enables PWM control for IR / external LED |
| LED_EXTERNAL_PWM_DUTY_CYCLE | Default duty % applied at boot (0–100) |
| LED_EXTERNAL_AS_DEBUG | Mirrors only error patterns onto external LED (0%/50%) when debug LED absent or also for redundancy |

### Board Profiles
Each file under `boards/` overlays `sdkconfig.base_defaults`. The merge order: base → board file → (optional) dynamic Wi‑Fi overrides via `switchBoardType.py` flags. Duplicate trailing segment directories collapse to unique keys.

- UVC doesn’t appear on the host?
  - Switch mode to UVC via CLI tool, replug USB and wait 20s.

### Adding a new board configuration
1. Create a new config file under `boards/` (you can nest folders): for example `boards/my_family/my_variant`.
2. Populate it with only the `CONFIG_...` lines that differ from the shared defaults. Shared baseline lives in `boards/sdkconfig.base_defaults` and is always merged first.
3. The board key the script accepts will be the relative path with `/` turned into `_` (example: `boards/my_family/my_variant` -> `my_family_my_variant`).
4. Run `python tools/switchBoardType.py --list` to verify it’s detected, then switch using `-b my_family_my_variant`.
5. If you accidentally create two files that collapse to the same key the last one found wins—rename to keep keys unique.

Tips:
- Use `--diff` after adding a board to sanity‑check only the intended keys change.
- For Wi‑Fi overrides on first flash: add none—pass `--ssid` / `--password` when switching if needed.

---

## Troubleshooting
### LED Status / Error Patterns
The firmware uses a small set of LED patterns to indicate status and blocking errors. When `LED_DEBUG_ENABLE` is disabled and `LED_EXTERNAL_AS_DEBUG` is enabled the external IR LED mirrors ONLY error patterns (0%/50% duty). Non‑error patterns are not mirrored.

| State | Visual | Category | Timing Pattern (ms) | Meaning |
|-------|--------|----------|---------------------|---------|
| LedStateNone | ![idle](docs/led_patterns/idle.svg) | idle | (off) | No activity / heartbeat window waiting |
| LedStateStreaming | ![stream](docs/led_patterns/streaming.svg) | active | steady on | Streaming running (UVC or Wi‑Fi) |
| LedStateStoppedStreaming | ![stopped](docs/led_patterns/stopped.svg) | inactive | steady off | Streaming intentionally stopped |
| CameraError | ![camera error](docs/led_patterns/camera_error.svg) | error | 300/300 300/700 (loop) | Camera init/runtime failure (check sensor, ribbon, power) |
| WiFiStateConnecting | ![wifi connecting](docs/led_patterns/wifi_connecting.svg) | transitional | 400/400 (loop) | Wi‑Fi associating / DHCP pending |
| WiFiStateConnected | ![wifi connected](docs/led_patterns/wifi_connected.svg) | notification | 150/150×3 then 600 off | Wi‑Fi connected successfully |
| WiFiStateError | ![wifi error](docs/led_patterns/wifi_error.svg) | error | 200/100 500/300 (loop) | Wi‑Fi failed (auth timeout or no AP) |

---

Feedback, issues, and PRs are welcome.
