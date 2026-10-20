# Nexus AR HUD design

## Scope and source contracts

The default build receives real NEIA events and displays one subject, two
Movella DOT sensor slots and `standard_loading_intensity` real-time magnitudes.
Initialize the session, connect sensors and start/stop acquisition through the
NEIA UI. Power the HUD on and establish its subscription **before** initializing
through the UI. No HUD gesture sends a Core command in this phase. The existing
command enums and requested states remain reusable but are inactive.

The client uses NEIA's existing plain WebSocket:
`ws://<resolved-host>:8050/api/v1/gateway/events`. No new bridge, direct ZeroMQ,
snapshot, lifecycle replay, or Core/NEIA changes are included.

Contracts were checked against the workspace sources:

- NEIA `neia-api/app/main.py`, `app/api/gateway.py`, and
  `app/gateway/{manager,zeromq_client}.py`: transparent event forwarding and the
  initial unrelated `voice_status` message.
- Core `nexus_n3/core/core.py`, `core/orchestrators/event_assembler.py`, and
  `distributed/master_node.py`: completed initialization, sensor connection
  batches and generic compute envelopes.
- Algorithm `standard_loading_intensity/{core_schema,core}.py`: real-time stage,
  four named frequency bands and `axis_values.mag`.

## Configuration and destination discovery

Defaults are `CONFIG_NEXUS_HOST=""`, `CONFIG_NEXUS_PORT=8050`, and
`CONFIG_NEXUS_PATH="/api/v1/gateway/events"`. No master IP needs to be entered.
With the default topology, the master hosts both the AP and NEIA. The receiver
uses the Wi-Fi manager's interface and the installed Zephyr API
`net_if_ipv4_get_gw()` to copy its DHCP-provided IPv4 gateway. It requires Wi-Fi
READY, a preferred IPv4 address and a usable gateway. Missing/unspecified,
loopback, multicast and broadcast gateway values keep Nexus disconnected and
are retried with backoff. The destination is read again on every attempt,
including after Wi-Fi reconnects; an obsolete gateway is never cached.

For NEIA on another host, set `CONFIG_NEXUS_HOST` to an IPv4 address or DNS name
(without scheme, port or path). The override takes precedence over gateway
selection and requires a usable local IPv4 address; it does not require a DHCP
gateway when the destination is directly reachable. Port and path can be
configured separately. IPv6, TLS and authentication are outside this phase.
Each resolved host/port/path is logged once per connection attempt. The gateway
is only destination discovery, never proof of NEIA or Core health.

`CONFIG_NEXUS_DUMMY=y` selects a mutually exclusive build-time demo. CMake links
only one event client, and both clients guard repeated starts. The demo emits
initialization, two sensor slots, warm-up, live data, stop and drain on its own
dedicated thread. Content has a permanent `Demo ` prefix in that build. The
simulated command function remains reusable, but the HUD never calls it.

## Ownership and concurrency

One controller thread owns mutable session/sensor state, view selection,
rendering and vibration decisions. Producers post fixed-size events with inline
strings, sensor entries and values into the existing 32-entry queue. No queued
payload points into a socket, JSON token, callback or reused receive buffer.
Queue overflow is reported as a controller fault and the receiver reconnects
instead of silently losing stream events. Invalid/unsupported events are logged
and cannot partially modify slot state.

One dedicated NEIA receiver owns its TCP/WebSocket descriptors and all receive
buffers. It never renders or pulses the motor. Wi-Fi callbacks only notify it
through an epoch counter/semaphore; they never perform socket operations. The
Wi-Fi retry thread and the system work queue used for button debounce remain
independent. Only the receiver closes its descriptors, avoiding cross-thread
close/reuse races.

The WebSocket worker waits for Wi-Fi/IP/destination availability, resolves the
host, connects TCP with a five-second deadline, then performs a three-second
upgrade. It reports Nexus connecting/connected/disconnected separately from
Wi-Fi and lifecycle. Failure retries at 1, 2, 4, 8 and at most 15 seconds; a
successful WebSocket resets its backoff. Wi-Fi loss invalidates the connection
epoch, causes the receiver to close and preserves retained sensor state. Receive
waits are bounded to 500 ms; an in-progress handshake/DNS operation may delay
closure until that bounded operation completes, without blocking the HUD.

Text messages are assembled across partial reads and continuation frames, with
ping/pong/close handled independently, including control frames between data
fragments. Client control frames are masked. There are no application/command
writes. Ping is sent after 15 seconds; missing pong times out after ten seconds.
An incomplete message also has a ten-second deadline. Close shuts down TCP
before releasing the wrapper, bounding the library's close handshake.

The installed Zephyr receive function can return a partial payload on timeout
without updating its type/remainder outputs. A one-byte first read establishes
frame metadata; subsequent chunks use the known frame type and subtract partial
byte counts when those outputs are absent. This avoids parsing a partial JSON
message as a complete event. The HTTP scratch buffer lives for the entire
WebSocket session and retains any bytes following the upgrade response.

Bounds: 8192 bytes per message, 768 JSON tokens, nesting depth 16, at most 125
control bytes, 63-character subject/address IDs, 47-character locations/session
names and two sensor slots. Unknown metadata is structurally skipped. Known
labels/identities must be printable ASCII (ASCII JSON escapes are supported).
Malformed JSON, duplicate required keys, excessive strings/tokens/depth,
nonfinite magnitudes and oversized frames are rejected. A bad complete JSON
event is skipped; an oversized or invalid frame closes the connection.

The controller checks blink and sample-age updates at least every 500 ms and
redraws only if the screen changed. Nonselected results do not redraw unchanged
content. No callback sleeps and no socket operation runs in the controller.

## Startup

1. Initialize the SSD1306 and existing 10x16 font.
2. Start the controller, showing `NEXUS HUD` / `Booting`.
3. Initialize the vibration GPIO and register the debounced button.
4. Start the selected event worker before Wi-Fi. Real mode initially shows
   waiting; demo mode progresses without an AP.
5. Initialize Wi-Fi, preserve its three-second driver settling delay and enable
   the existing asynchronous Wi-Fi recovery worker.

## Wi-Fi recovery

`wifi_manager_connect()` enables automatic recovery after the existing startup
settling delay; it returns immediately. A single dedicated `hud_wifi` thread
owns every connect/disconnect request, retry deadline and Wi-Fi state update.
Repeated init/connect calls return `-EALREADY`, creating no extra thread and
never bypassing backoff. Network callbacks only copy a failure flag and signal
a semaphore; they retain no event-data pointers, render nothing and perform no
network requests. OLED and button processing use their existing contexts.

After failure or loss, retry waits 1, 2, 4, 8, then at most 15 seconds. The delay
begins once the previous driver attempt has ended. Only obtaining a preferred
IPv4 address on an associated link resets the next delay to one second.
Association alone leaves the indicator blinking and does not reset the delay.
The total association-plus-DHCP deadline is 45 seconds per attempt; successful
association does not extend it. Synchronous request errors also trigger recovery.

Failure, loss or timeout enters cancellation: the worker stops DHCP to clear
the old lease and asks the driver to disconnect. The indicator is hollow during
cancellation/backoff. No new connection is requested until the driver's status
confirms idle. Disconnect is retried at one-second intervals if the driver
remains busy; late IP events during cancellation cannot restore READY. A driver
that cannot cancel is kept isolated rather than given overlapping attempts.
The worker polls cancellation status every 250 ms and other states at most once
per second, with network events waking it early. This also detects address loss
without a Wi-Fi disconnect event. Unknown driver state never permits a connect.

`CONFIG_ESP32_WIFI_STA_RECONNECT=n` is explicit and guarded at build time: the
ESP32 driver's own reconnect path retries immediately in its event handler and
provides neither this bounded backoff nor the IP deadline. Enabling both would
create competing retry owners. The existing ESP32 automatic DHCP startup is
retained. NEIA WebSocket recovery is a separate receiver concern and never restarts
Wi-Fi or acquisition.

## Initialization, slots and compute projection

Only `system_initialized` with `initialization_complete: true`, installed
configuration and exactly one `subject_ids` entry establishes a subject. Both
standalone and completed master envelopes are supported; `node_initialized` and
incomplete/recovery workflows never reset the model. Completed initialization
clears slots, old results and selection. Session metadata is not a sensor roster.

`sensor_connected.payload.subjects[].connected_sensors[]` registers CONNECTED
sensors by the established subject ID and address, retaining location for the
label. Separate/repeated batches merge without duplicates, erasing values or
changing selection. A whole batch is validated before modification. Additional
subjects or sensors are rejected with a capacity diagnostic, preserving existing
slots. Each slot starts with exactly `No events` in the value row.

Compute projection accepts only `compute_result` for
`standard_loading_intensity`, nested stage `real_time`, the established subject
and a registered address. It finds `band_name == "0-6"` regardless of order,
requires numeric finite `axis_values.mag`, and accepts zero. Duplicated result
identities, when present, must agree with the envelope. Only the addressed slot
updates; its registered location and current selection are retained. Performance,
other algorithms/stages, `voice_status`, and unrelated events do nothing.
Lifecycle subject scopes, when supplied, must match the established subject.
Core errors accept either a payload string or an object with `message`; display
messages are decoded and safely truncated to 47 characters plus a terminator.

Every accepted compute result causes the controller to request one nonblocking
50 ms vibration pulse, including results for a nonselected sensor. One delayed
stop is rescheduled, so closely spaced pulses merge rather than forming a
backlog. Lifecycle, connections, ignored messages and rejected results do not
vibrate. The existing GPIO vibration manager provides the pulse and timed stop.

## Four-row OLED and button

The 128x64 monochrome OLED keeps its 10x16 font and maximum 12 characters per
content row. The permanently reserved top row keeps `Wi-Fi` at x=0 and its
circle at (58,7), plus `N3` at x=76 and its circle at (108,7). Radius-four circles
are hollow when disconnected, alternate filled/hollow every 500 ms when
connecting, and stay filled when connected. `Wi-Fi` requires an actual IP;
`N3` means the WebSocket is connected, not that Core is streaming or healthy.

With slots available, Ready and official streaming show the selected sensor:

| Row | Before results | Fresh result | Retained stale result |
|---|---|---|---|
| 1 | fixed connectivity indicators | same | same |
| 2 | Left ankle | Left ankle | Left ankle |
| 3 | LI 0-6 Hz | LI 0-6 Hz | LI 0-6 STALE |
| 4 | No events | 12.48 | 12.48 |

`LEFT_ANKLE`/`RIGHT_ANKLE` become readable labels; other labels are clipped safely.
No wrapping, marquee, invented units or severity thresholds are used. Large magnitudes use compact scientific notation when fixed decimals would
exceed the row. Demo mode prefixes row 2 with `Demo `,
clipping the following label to fit; the source is never presented as real.

Before slot registration, lifecycle screens show `Waiting for` / `status`,
`Ready` / abbreviated session, or `LIVE` / `Waiting data`. Warm-up shows
`Warming up`, stopped/draining shows `Finalizing`, and failures show `ERROR` /
a short reason. Drained returns to Ready (or its selected sensor view).
A new warm-up clears previous run values without unregistering sensors.

A single debounced press cycles all registered slots, including `No events`
slots, even while offline. During warm-up it switches to sensor inspection; the next lifecycle event
restores its concise status. ERROR and Finalizing stay visible and ignore view
cycling until the lifecycle changes. Incoming
results never change the selected index. The existing 30 ms debounce and 350 ms
gesture deferral remain; double press also cycles locally and never transmits.

On Wi-Fi or WebSocket loss, slots, selection and values remain. Real values are
marked stale; reconnect alone does not freshen them. Each sensor needs a new
accepted result. Samples also become stale after 15 seconds without updates
(with up to 500 ms tick latency). A slot without a value continues to show
`No events`. Loss does not imply acquisition stopped and sends no Core command.

## Limitations and recovery

There is no snapshot or lifecycle replay. Events emitted before subscription or
during disconnection may be missed. Starting the HUD after initialization cannot
reconstruct the subject/sensor roster. Subscribe before UI initialization; if
initialization was missed, initialize again through the UI. A live connection is
not authoritative knowledge of Core state. Recovery preserves only events this
HUD actually observed and does not interrupt acquisition or operate the UI.
The scope is one subject/two sensors and one projected algorithm/band, with
bounded metadata and no dynamic sensor discovery queries.

## Validation and hardware procedure

Run `bash tests/run.sh` for sanitizer-enabled host tests. Fixtures under
`tests/fixtures` document their source contracts. Coverage includes standalone/
master completion, resets, separate/together/repeated slots, independent missing
values, interleaved address routing, shuffled bands/zero, ignored events and
malformed/nonfinite/oversized data, queue ownership after buffer reuse, no
commands, selected/nonselected vibration decisions and merged pulses, stale
retention, frame/control fragmentation, and gateway/missing/changed/override
selection. Existing Wi-Fi backoff/DHCP/overlap and button tests remain active.
Socket/I2C/radio timing is not established by these fake-driver tests.

Build using `bash build.sh` (existing west board/overlay/credentials workflow).
For restricted cache paths, the equivalent is:

```sh
source ./env.sh
export CCACHE_DIR=/tmp/hud-ccache
export CCACHE_TEMPDIR=/tmp/hud-ccache-tmp
mkdir -p "$CCACHE_TEMPDIR"
west build -b xiao_esp32s3/esp32s3/procpu/sense "$APP" \
  --build-dir "$APP/build" -- \
  -DEXTRA_CONF_FILE="$APP/wifi_credentials.conf" \
  -DUSER_CACHE_DIR:PATH=/tmp/hud-zephyr-cache
```

For an isolated demo build, use another build directory and add
`-DCONFIG_NEXUS_DUMMY=y`. Do not enable real and demo sources together.

Hardware acceptance (no flashing performed in this task):

1. Connect the HUD to the master's AP without entering the master's IP.
2. Confirm both Wi-Fi/N3 indicators become solid and the destination log shows
   the DHCP gateway with port 8050 and the NEIA events path.
3. Initialize one subject with two DOTs through the UI.
4. Connect sensors; cycle through both slots and verify `No events` independently.
5. Start through the UI; check warm-up and official lifecycle status.
6. Verify independent loading-intensity updates and short vibration cues,
   including while the other sensor is selected.
7. Cycle views while events continue; verify presses never stop acquisition.
8. Stop through the UI and verify finalization status and return to Ready/view.
9. Test AP and NEIA loss/reconnection without UI or acquisition disruption.
   Retained values must show stale; fresh per-sensor events restore freshness.
10. Boot with the AP absent, enable it, then disable/restore it. Repeat with
    stalled DHCP and a changed gateway. Verify display/buttons stay responsive.
11. Check real OLED bounds/polarity/legibility, motor pulse duration/merging and
    sustained operation at the actual compute rate. Subscription before setup,
    fragmentation/heartbeats against NEIA and real gateway discovery need device
    verification beyond the local build and host tests.
