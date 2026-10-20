# Nexus N3 AR HUD

Experimental Zephyr firmware for the XIAO ESP32S3 Sense, a 128x64 SSD1306,
physical button and vibration motor. The default build now receives real NEIA
events for one subject and two Movella DOTs using `standard_loading_intensity`.

Power the HUD on **before** initializing the session in the NEIA UI. Initialize,
connect sensors and start/stop through that UI. The HUD sends no Core commands;
a button press cycles local sensor views, including slots with `No events`.

## Connection and configuration

The default endpoint is `ws://<DHCP-gateway>:8050/api/v1/gateway/events`.
When the master hosts both the AP and NEIA, connect to its AP and the HUD derives
the destination automatically. No master IP needs to be entered. The gateway is
resolved again after reconnecting. Without a usable gateway, the HUD stays
responsive and retries.

Kconfig settings (defaults):

```text
CONFIG_NEXUS_HOST=""
CONFIG_NEXUS_PORT=8050
CONFIG_NEXUS_PATH="/api/v1/gateway/events"
CONFIG_NEXUS_DUMMY=n
```

For NEIA elsewhere, set `NEXUS_HOST` to an IPv4 address or DNS name without a
scheme/path/port. This overrides DHCP gateway discovery. Plain IPv4 WebSocket is
the implemented transport; TLS/authentication, snapshots and replay are outside
this phase. No bridge, direct ZeroMQ or Core/NEIA changes are needed.

`NEXUS_DUMMY=y` instead builds the asynchronous simulated source; its content
is visibly prefixed `Demo`. Only one source is linked. It runs an automatic
lifecycle/results demonstration; buttons never transmit commands in either mode.

## Display and feedback

Row 1 always shows `Wi-Fi` and `N3` with hollow, blinking or solid circles.
Wi-Fi becomes solid only with an IP; N3 means the real WebSocket is connected,
which does not prove that Core is streaming. Rows 2–4 show the selected location,
`LI 0-6 Hz`, and magnitude or exactly `No events`. Connection loss preserves
slots/values and marks retained results stale. Lifecycle screens show warm-up,
finalization and errors; a press lets you inspect sensor slots during warm-up. Error and finalization
screens stay visible until the lifecycle changes.
Every accepted real-time result requests a nonblocking 50 ms vibration pulse,
even for the nonselected sensor. Closely spaced pulses merge.

The OLED/controller start before Wi-Fi. Its existing one-worker recovery uses
1, 2, 4, 8, then 15-second maximum backoff and a 45-second association/DHCP
deadline. The independent WebSocket worker reconnects with bounded backoff.

## Build and test

Run `bash tests/run.sh` for focused sanitizer-enabled parser/model/controller,
WebSocket assembly/gateway, vibration, button and Wi-Fi tests. Run
`bash build.sh` for the existing Zephyr board/overlay/credentials build workflow.
[design.md](design.md) describes cache overrides, limits, contracts and the full
hardware procedure.

There is no replay: initialization and sensor registration emitted while the
HUD is offline cannot be reconstructed automatically. The first workflow assumes
subscription before session setup. This is a prototype; radio/socket timing,
OLED appearance and motor cues still need hardware verification.
