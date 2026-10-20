# Nexus N3 AR HUD

`rs-nexus-ar-hud` is an experimental work in progress.

The goal of this project is to provide a simple augmented reality display for viewing computed outputs produced by the Nexus N3 edge software.

The repository currently contains Zephyr firmware targeting the XIAO ESP32S3 Sense, with support for:

- Wi-Fi connectivity
- a 128x64 SSD1306 OLED display
- a physical button input
- independent Wi-Fi and Nexus indicators with clear text labels
- Nexus lifecycle/result screens and asynchronous simulated start/stop control

This HUD is intended to pair with `nexus-n3-core`, the edge-processing component that generates the session and compute data shown in the display:

- [Nexus-N3/nexus-n3-core](https://github.com/Nexus-N3/nexus-n3-core)

That repository is not public yet, but it is expected to open soon.

## Status

This project is still an experimental prototype.

The firmware uses a simulated Nexus event source and command destination.
The OLED starts before Wi-Fi and remains usable without an access point. The status row uses `Wi-Fi` and `N3` labels.
`Demo` in the content area identifies simulated screens and results.

Wi-Fi automatically retries after failure or loss with 1, 2, 4, 8, then
15-second maximum backoff. Obtaining an IP resets backoff; association/DHCP has
a 45-second attempt deadline. One Wi-Fi worker owns recovery independently of
the OLED and button handlers.

Single press starts from Ready or stops during startup/streaming. Double press
cycles left/right sensor views while streaming, without sending stop. Single
presses are deferred for 350 ms to distinguish the gestures. Lifecycle events
confirm command effects; pending commands do not imply acquisition has changed.

Real transport is pending. No bridge, ZeroMQ client or WebSocket connection is
implemented. See [design.md](design.md) for screens, ownership, controls and
hardware acceptance checks. Run `bash tests/run.sh` for focused host tests and
`bash build.sh` for the existing Zephyr build workflow.

## Notes

- Scope and integration details may change as `nexus-n3-core` is finalized for public release.
- The current implementation is focused on proving the embedded HUD experience and state presentation on-device.
