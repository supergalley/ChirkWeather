# ChirkWeather
Development of the Chirk Weather Station


Heltec WiFi LoRa 32 V4 weather station, EU868, four-byte payload on port 2.

Production starts with diagnostics off: one report followed by at least ten minutes of deep sleep. Cold starts/reset wait ten minutes before the first report. Low-battery recovery, session persistence and a 60-second watchdog are included.

See [USB diagnostics](DIAGNOSTICS.md) for wiring, commands, build instructions, and known issues.

Local credentials belong in ignored `XSecrets.h` (copy `XSecrets.example.h`). `.env` and `.secrets/` are also ignored.
