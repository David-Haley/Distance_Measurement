# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a Raspberry Pi Pico W that reads distance (mm) from an ST VL53L0X
time-of-flight sensor over I2C and prints it over USB serial. See
`instructions.txt` for the original task spec and `Documents/` for the ST
VL53L0X API datasheet/PDF.

## Build

Environment variables (`PICO_SDK_PATH`, `FREERTOS_KERNEL_PATH`) are set by
`/home/david/pico/env.sh`, sourced automatically in interactive shells. In a
non-interactive shell, source it manually first.

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DPICOTOOL_FORCE_FETCH_FROM_GIT=1
cmake --build . -j$(nproc)
```

`-DPICOTOOL_FORCE_FETCH_FROM_GIT=1` is only needed on the first configure in a
fresh `build/` directory: the system `picotool` package (2.1.1) is older than
what this pico-sdk checkout requires (2.3.0), so CMake fetches and builds a
matching picotool from source into `build/_deps/`. Subsequent reconfigures of
the same build directory don't need the flag re-passed.

Output: `build/distance_measurement.uf2` and `.elf`.

## Flash and monitor

```bash
picotool load -f -v -x build/distance_measurement.uf2
```

`-f` forces a device that's already running application code (exposing a USB
CDC serial port) to reboot into BOOTSEL mode for the load, then reboots it
back into the application afterward — no manual BOOTSEL-button/replug dance
needed.

After flashing/rebooting, the board re-enumerates and `/dev/ttyACM0` briefly
disappears and reappears. Wait for that before attaching, e.g.:

```bash
for i in $(seq 1 50); do [ -e /dev/ttyACM0 ] && break; sleep 0.1; done
timeout 20 cat /dev/ttyACM0
```

`main.c` waits 2s at boot before printing anything, which covers the
re-enumeration race. `picotool reboot -a -f` reboots the running application
without reflashing, useful for re-capturing serial output from a clean boot.

## Architecture

- `Api/core/` — ST's manufacturer VL53L0X API (ranging/calibration
  algorithms). Platform-agnostic C, untouched from the vendor drop. Don't
  modify unless fixing an actual algorithm bug.
- `Api/platform/` — the porting layer the vendor API calls into. Only two
  files are part of the Pico build:
  - `src/vl53l0x_i2c_platform.c` — the actual I2C transport, rewritten
    against the Pico SDK's `hardware_i2c` (originally a Windows DLL-backed
    implementation). I2C0 on GPIO0 (SDA) / GPIO1 (SCL) — physical header pins
    1 and 2 — is hardcoded here via `VL53L0X_I2C_INSTANCE/_SDA_PIN/_SCL_PIN`.
  - `src/vl53l0x_platform.c` — generic register read/write glue that calls
    into `vl53l0x_i2c_platform.c`; only patched to replace one Windows-only
    delay call with `sleep_ms()`.
  - `src/vl53l0x_i2c_win_serial_comms.c` and `src/vl53l0x_platform_log.c` are
    the original Windows-only files, kept for reference but **not** listed in
    `CMakeLists.txt`'s sources — don't add them to the build.
- `src/main.c` — application entry point: brings up I2C via
  `VL53L0X_comms_initialise()`, runs the sensor init sequence, then loops
  `VL53L0X_PerformSingleRangingMeasurement()` and prints `Distance: N mm`
  (or the range status) every 200ms over USB serial (`pico_enable_stdio_usb`
  is on, UART stdio is off).

### Known API gotcha

`VL53L0X_WaitDeviceBooted()` (declared in `Api/core/inc/vl53l0x_api.h`) is an
unimplemented stub in this API version — it always returns
`VL53L0X_ERROR_NOT_IMPLEMENTED` regardless of hardware state. Do not call it;
it's not part of the real VL53L0X init sequence. The correct sequence (as
used in `main.c`) is: `VL53L0X_comms_initialise` → `VL53L0X_DataInit` →
`VL53L0X_StaticInit` → `VL53L0X_PerformRefCalibration` →
`VL53L0X_PerformRefSpadManagement` → `VL53L0X_SetDeviceMode`.

`RangeStatus` on a measurement is a sensor-reported quality code, not a
plumbing error — e.g. status 4 (`PHASE_FAIL`) just means no target is in
range, which is expected with nothing in front of the sensor.
