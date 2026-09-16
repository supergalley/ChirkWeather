# ChirkWeather USB diagnostics

## Hardware and start-up

Target: **Heltec WiFi LoRa 32 V4**, native USB CDC at 115200 baud.

The converter enable wire was moved from **GPIO19 to GPIO47** on 16 September 2026. GPIO19/20 are USB data pins; firmware refuses to drive them as converter enable in a native USB build. `PIN_12V_EN` in `XPins.h` is now 47, active HIGH. Readback indicates the GPIO state, **not a measured 12 V output**. Verify output with a meter; coil noise is not a reliable voltage measurement.

Cold boot/reset starts with `diagnosticMode=true`. Diagnostic mode bypasses the battery sleep cutoff, stays awake without a USB host, powers the OLED continuously, and requests converter ON continuously. `boost off` overrides this for the rest of the awake session. Sensor reads and uplinks do not silently turn it back on.

The first automatic cycle starts ten seconds after setup; it samples after a five-second warm-up, then sends. Subsequent automatic cycles start two minutes after the preceding radio operation. The screen is updated before/after TX/RX, never during RadioLib's synchronous radio operation. The BME and OLED use separate I2C buses, each with a timeout. USB commands remain usable during warm-up; during radio initialization and TX/RX they wait in the USB receive buffer until the call returns. This is not a real-time emergency-stop interface.

USB logging is buffered in a 64-line RAM journal. A disconnected/slow Mac does not block the firmware; old lines are overwritten. `logs` replays the latest 32 lines. Reset loses the journal. No keys are logged.

## Commands

Send one line per command (Enter, LF, CR, or CRLF). Commands are case-insensitive.

| Command | Effect |
|---|---|
| `help` | List commands |
| `status` | Mode, phase, reset/wake reasons, free memory, power state, radio counters, cached readings |
| `pins` | Configured GPIO assignments and possible V4 conflicts |
| `read` | Wait five seconds, then read/encode sensors without transmitting; leaves converter setting alone |
| `sample` | Read/encode sensors immediately, including when converter is off |
| `radio` | Local radio status, counters, attempts, return code |
| `tx` | Fresh measurement after five seconds, then unconfirmed port-2 uplink |
| `cancel` | Cancel pending warm-up/send; automatic schedule remains enabled unless disabled separately |
| `boost on` / `boost off` | Hold converter enable HIGH/LOW; cancels a pending measurement |
| `oled on` / `oled off` | Show/blank the OLED; keep its power rail unchanged |
| `auto on` / `auto off` | Enable/disable automatic transmissions; `off` also cancels a queued send |
| `interval 120` | Set automatic interval in whole seconds, 120–3600 |
| `stream 5` | Periodic status heartbeat in seconds, 1–3600; sample age explicitly shows cached data |
| `stream 0` | Disable periodic heartbeat |
| `sensors retry` | Reinitialize BME280 detection and take a fresh reading |
| `logs` | Replay recent RAM log |
| `mode normal` | Perform a normal battery-gated measurement/send cycle, then ten-minute deep sleep |
| `mode diagnostic` | Confirm diagnostic mode (USB is unavailable in deep sleep) |
| `reboot` | Restart into diagnostics; session counter restoration is still a known issue |

PROG in diagnostic mode refreshes status; it does not unexpectedly put the board to sleep. Normal timer wakes continue normal mode using an RTC-memory marker. RESET/cold boot returns to diagnostics. Do not hold PROG while resetting unless entering the bootloader intentionally.

No runtime settings persist across a reset. LoRaWAN keys, counters, arbitrary GPIO writes, and flash erasure are intentionally not exposed as commands.

## From the Mac

Close Arduino Serial Monitor before opening another serial console.

```sh
cd ~/Documents/Arduino/ChirkWeather
python3 tools/usb_console.py
```

Or capture a bounded check:

```sh
python3 tools/usb_console.py --command 'auto off' --command status --watch 10
```

Use `--port /dev/cu.usbmodem...` if multiple USB serial devices are connected. The helper uses only Python's standard library. Ctrl-C closes it without sending a reboot command.

To test the converter: `auto off`, `boost off`, `sample`; measure output voltage. Then `boost on`, `read`; measure again and compare raw wind-sensor inputs. Keep the antenna attached for `tx`. `TX local OK` does not prove TTN accepted an uplink; compare device live data with gateway live data.

## Build and upload

Tested dependencies: Arduino CLI 1.5.1, Heltec core 3.3.8, RadioLib 7.7.1, Heltec ESP32 Dev-Boards 2.1.6, U8g2 2.36.19, Adafruit BME280 2.3.0 (and its dependencies).

Copy `XSecrets.example.h` to ignored `XSecrets.h` and supply credentials on a fresh checkout. Existing local credentials have already been migrated there.

```sh
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc --build-path /tmp/chirk-build .
arduino-cli board list
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc --port /dev/cu.usbmodem1101 --input-dir /tmp/chirk-build .
```

Use the actual current port. This build leaves the existing radio-front-end configuration unchanged; verify the exact V4 revision and its FEM configuration before altering those options. Do not erase all flash: that would remove stored device state.

## Known radio and sensor limitations

- `XRadio::begin()` now initializes only once per awake session, so diagnostic uplinks retain the active RAM session. Previously each call to `beginABP()` cleared that session.
- **Session restoration across reset/deep sleep is not fixed here.** The old code saves `fcntup` but does not restore RadioLib's session. Logs show saved/live/transmitted counters to diagnose this without silently changing TTN state. Do not reset TTN counters as a permanent workaround.
- ABP does not perform an over-the-air join. The active sender remains RadioLib in `XRadio.cpp`; `XLoRa.cpp` is an unused older implementation.
- Uplinks remain unconfirmed. A return of zero is local completion without a downlink; positive results identify a received downlink window. Detailed event fields and received bytes are logged after the radio call.
- Existing ADR-off/duty-cycle-off radio settings are preserved. Diagnostic commands enforce at least 120 seconds between attempted transmissions; this alone is not a universal airtime/duty-cycle calculation.
- `PIN_BME_SDA=7` is inherited wiring. Heltec V4 pin maps also label GPIO7 as radio FEM power control, and some revisions use GPIO5 for FEM control. **Check the exact board revision and sensor wiring before treating simultaneous sensor/radio operation as validated.** These changes do not claim to resolve those board-level conflicts.
- Battery conversion/control polarity remain as in the original project and need comparison with a meter. Normal mode retains the calculated 3.62 V cutoff. Missing BME readings now encode deterministic placeholders with the sensor-error bit instead of rounding NaN.

Official pin map: https://resource.heltec.cn/download/WiFi_LoRa_32_V4/Pinmap/V4_pinmap.png

## Verification and snapshots

`tests/run.sh` runs the actual command handler, USB parser/journal, converter guard and encoder against host hardware mocks, with address/undefined-behaviour sanitizers. It checks the GPIO19 USB conflict, GPIO47 switching, low-battery diagnostics, warm-up responsiveness, TX throttling/cancellation, malformed commands, disconnected USB, OLED blanking, and normal-mode timer wake behaviour. Mocks cannot validate physical voltage or RF reception.

The exact original local snapshot is commit `7101842`, tag `before-usb-diagnostics`. It contains the pre-existing hardcoded credentials and must remain local. The GitHub branch is based on the repository's original README commit and contains only credential-free source plus a placeholder secrets example. `.env`, `XSecrets.h`, and `.secrets/` are excluded.
