# ChirkWeather USB diagnostics

## Hardware and start-up

Target: **Heltec WiFi LoRa 32 V4**, native USB CDC at 115200 baud.

The converter enable wire was moved from **GPIO19 to GPIO47** on 16 September 2026. GPIO19/20 are USB data pins; firmware refuses to drive them as converter enable in a native USB build. `PIN_12V_EN` in `XPins.h` is now 47, active HIGH. Readback indicates the GPIO state, **not a measured 12 V output**. Verify output with a meter; coil noise is not a reliable voltage measurement.

Production defaults to `diagnosticMode=false`. Normal operation checks the battery, warms the wind sensors for five seconds, takes a sample, switches the converter off, sends one unconfirmed uplink, completes the receive windows, and sleeps for **at least 600 seconds**. Expect roughly ten minutes plus measurement/receive time between reports. OLED and converter stay off during sleep; the BME280 and radio are put into sleep, and converter/Vext/ADC control levels are held through deep sleep.

A cold start, RESET, brownout or watchdog reset waits a full interval before the first transmission. A blank display and disappearing USB port are therefore normal. Do not keep resetting it to obtain an immediate reading.

To enter diagnostics, press RESET alone, then press PROG during the following three seconds; alternatively send `mode diagnostic` over USB during that window. Holding PROG *while* resetting enters the download bootloader instead. Diagnostic mode bypasses the battery cutoff, stays awake, keeps OLED and converter on, and accepts `boost off` until changed again. Automatic and manual uplinks are still limited to a minimum of ten minutes; diagnostics no longer uses the old two-minute interval.

The screen is updated before/after TX/RX. USB commands remain usable during warm-up; during synchronous radio calls they queue until the call returns. This is not a real-time emergency-stop interface. I2C buses have timeouts, and a 60-second task watchdog covers a stuck main task, including library initialization or radio calls. It is fed between operations, not from a background timer that could conceal a stalled main task.

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
| `interval 600` | Set automatic interval in whole seconds, 600–3600 |
| `stream 5` | Periodic status heartbeat in seconds, 1–3600; sample age explicitly shows cached data |
| `stream 0` | Disable periodic heartbeat |
| `sensors retry` | Reinitialize BME280 detection and take a fresh reading |
| `logs` | Replay recent RAM log |
| `mode normal` | Switch to normal mode and sleep first; no immediate extra transmission |
| `mode diagnostic` | Enter diagnostics during the boot window; confirm it once already awake |
| `reboot` | Restart into normal mode with a full interval before transmitting |

PROG in diagnostic mode refreshes status. Runtime console settings are not saved; RESET returns to normal mode. Session state and transmission counters **are** saved. Keys, arbitrary GPIO writes, and flash erasure are not console commands.

## Unattended operation

- The battery is checked before BME initialization, wind sensor power or radio transmission, and checked again under the converter load. Below the calculated **3.62 V**, normal operation sleeps for an hour and checks again. An RTC-memory latch requires **3.80 V** before resuming. Invalid readings also inhibit transmission. The latch survives deep sleep, but complete power loss clears it. Verify ADC voltage against a multimeter; this is not a replacement for a protected battery and suitable charger.
- `XRadio.cpp` saves RadioLib's complete session, including receive settings and downlink counters, in an atomic NVS blob. It reserves the next uplink counter **before** using the radio; a power cut may skip a counter but must not reuse one. Missing/corrupt state or a failed storage write stops transmissions instead of starting at zero.
- First upgrade migrates the legacy saved counter with a one-time 32-count safety gap. It seeds RX1=5 seconds and RX2=869.525 MHz/DR3 from this device's live TTN session verified on 16 September 2026. Subsequent wakes restore learned settings. This migration is specific to the existing station, not a generic provisioning mechanism. A new/erased device needs deliberate provisioning.
- Keep **RadioLib 7.7.1** pinned. The counter/repetition adapter deliberately fails compilation for a different version until reviewed. Unconfirmed transmissions have one physical attempt; no rapid retry loop. MAC replies requiring their own packet are saved and use a later scheduled slot, possibly replacing a weather report.
- At SF9/BW125, a four-byte report without extra MAC commands takes about **0.165 seconds** on air. At most 144/day is about **23.8 seconds/day**, below TTN Community's 30-second daily allowance. Larger MAC-bearing packets automatically extend the next sleep, targeting at most 24 seconds/day. This assumes the configured EU868 DR3; do not change radio parameters without updating the calculation. Downlinks remain network-controlled and should be kept sparse.
- Before leaving it unattended: verify converter output actually falls when disabled, measure sleep current with USB disconnected, check battery voltage calibration, and let it run overnight. Reset it once and confirm the next TTN frame counter increases. A watchdog cannot fix a short, moisture ingress, failed converter, or exhausted unprotected battery.

TTN policy: https://www.thethingsnetwork.org/docs/lorawan/duty-cycle/

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

Tested dependencies: Arduino CLI 1.5.1, Heltec core 3.3.8, RadioLib 7.7.1, U8g2 2.36.19, Adafruit BME280 2.3.0 (and its dependencies).

Copy `XSecrets.example.h` to ignored `XSecrets.h` and supply credentials on a fresh checkout. Existing local credentials have already been migrated there.

```sh
arduino-cli compile --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc --build-path /tmp/chirk-build .
arduino-cli board list
arduino-cli upload --fqbn Heltec-esp32:esp32:heltec_wifi_lora_32_V4:CDCOnBoot=cdc --port /dev/cu.usbmodem101 --input-dir /tmp/chirk-build .
```

Use the actual current port. This build leaves the existing radio-front-end configuration unchanged; verify the exact V4 revision and its FEM configuration before altering those options. Do not erase all flash: that would remove stored device state.

## Known radio and sensor limitations

- ABP does not perform an over-the-air join. The active sender is RadioLib in `XRadio.cpp`. Historical `XLoRa.cpp` is now excluded from compilation; its unused Heltec library also shadowed the ESP-IDF GPIO sleep header.
- Local TX success does not prove TTN acceptance. Check application/device live data or the display website, not just gateway reception. Radio logs show the actual transmitted counter separately from RadioLib's event counter (which is one higher).
- ADR is off and DR3 is fixed to match the proven installation and airtime calculation. Library duty-cycle enforcement is enabled in addition to sleep-side enforcement.
- `PIN_BME_SDA=7` is inherited wiring. Heltec V4 pin maps also label GPIO7 as radio FEM power control, and some revisions use GPIO5 for FEM control. **Check the exact board revision and sensor wiring before treating simultaneous sensor/radio operation as validated.** These changes do not claim to resolve those board-level conflicts.
- Battery divider/control polarity remain as in the original project; readings are now averaged and still need comparison with a meter. Missing BME readings now encode deterministic placeholders with the sensor-error bit instead of rounding NaN.

Official pin map: https://resource.heltec.cn/download/WiFi_LoRa_32_V4/Pinmap/V4_pinmap.png

## Verification and snapshots

`tests/run.sh` runs actual command handling, USB parsing/journaling, power guards, payload encoding and production scheduling against host hardware mocks with address/undefined-behaviour sanitizers. It checks normal reset holdoff, low-battery inhibition/recovery, ten-minute limits, both GPIO19 and GPIO47, and explicit diagnostic entry. Separate tests exercise the actual radio wrapper against fake storage/radio: power loss after RF but before saving, counter reservation, deferred MAC replies across restart, cooldown, storage failure and corrupt records. Mocks cannot validate actual flash atomicity, RF timing, voltage, current, watchdog hardware reset or TTN acceptance; the ESP32 build and an on-board soak test complement them.

The exact original local snapshot is commit `7101842`, tag `before-usb-diagnostics`. It contains the pre-existing hardcoded credentials and must remain local. The GitHub branch is based on the repository's original README commit and contains only credential-free source plus a placeholder secrets example. `.env`, `XSecrets.h`, and `.secrets/` are excluded.
