# Nexus N3 AR HUD

`rs-nexus-ar-hud` is an experimental work in progress.

The goal of this project is to provide a simple augmented reality display for viewing computed outputs produced by the Nexus N3 edge software.

The repository currently contains Zephyr firmware targeting the XIAO ESP32S3 Sense, with support for:

- Wi-Fi connectivity
- a 128x64 SSD1306 OLED display
- a physical button input
- vibration feedback
- a HUD state machine for presenting Nexus N3 session and streaming states

This HUD is intended to pair with `nexus-n3-core`, the edge-processing component that generates the session and compute data shown in the display:

- [Nexus-N3/nexus-n3-core](https://github.com/Nexus-N3/nexus-n3-core)

That repository is not public yet, but it is expected to open soon.

## Status

This project is still an experimental prototype.

The firmware and device-state flow are already implemented, but the real Nexus client integration is still being replaced by a dummy event source used to simulate connection, session initialization, and streaming transitions. Until that is swapped for the actual Nexus transport, this repository should be treated as a hardware and interaction prototype rather than a finished product.

## Notes

- Scope and integration details may change as `nexus-n3-core` is finalized for public release.
- The current implementation is focused on proving the embedded HUD experience and state presentation on-device.
