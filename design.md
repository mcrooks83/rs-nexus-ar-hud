# Nexus AR HUD design

## Scope and transport

The HUD displays Nexus lifecycle events and selected loading-intensity results,
and supports start/stop control. This supersedes the earlier receive-only scope.
It does not initialize Core sessions, select algorithms, or request readiness.
Real transport is **pending**. No bridge, ZeroMQ client, WebSocket connection,
or compute JSON parser is implemented. The placeholder client/parser files stay
uncompiled. The only command destination is the asynchronous simulated client.

Commands use a bounded enum (`START`, `STOP`). A future transport must map these
to Core commands and deliver events through the same owned event queue. Selecting
that transport and its recovery/snapshot contract is separate work.

## Ownership and execution

One controller thread owns the screen/session model and all OLED drawing after
display initialization. Network callbacks, dummy callbacks, GPIO interrupts and
button work handlers never draw. Producers post fixed-size events with inline
strings and values to a 32-entry Zephyr message queue. There are no borrowed
payload pointers. Posting is nonblocking; overflow returns an error and sets a
controller fault (`Queue full`) rather than failing silently.

The controller stores Wi-Fi connectivity, Nexus event-source connectivity and
session lifecycle independently. Connectivity changes never overwrite the
observed lifecycle or clear results. Selected sensor values are retained in two
bounded slots. New sessions clear slots; a new official stream waits for fresh
samples. Unsupported extra locations are rejected with a logged error.

The controller checks for blink/sample-age updates at least every 500 ms and
redraws only when the resulting screen changes. Nonselected sensor updates do
not redraw unchanged content. The former marquee thread is removed. Callback
paths contain no sleeps. The dummy's dedicated thread may wait for commands or
its next demonstration deadline without blocking the controller or Wi-Fi.

## Startup

1. Initialize the SSD1306 and its existing 10x16 font.
2. Start the HUD controller, showing `NEXUS HUD` / `Booting`.
3. Register the debounced button.
4. Start the asynchronous dummy, independently of network availability.
5. Initialize Wi-Fi, retain the existing three-second driver settling delay,
   and request a connection. Failure is logged and does not stop the HUD.

Vibration is not initialized or used. Its existing source files are retained.

## Connectivity row

The 128x64 monochrome OLED has four 16-pixel rows, with at most 12 text
characters in each content row. Row 1 is permanently reserved:

- `Wi-Fi` at x=0; circle centered at (58,7).
- `N3` at x=76; circle centered at (108,7).

Each circle has radius 4 pixels. Disconnected is hollow, connecting alternates
hollow/filled every 500 ms, and connected is filled. Circles are drawn with
framebuffer primitives; no Unicode or colour is used. `Demo` in content row 4 identifies simulated screens; on result screens
it prefixes the numeric value (for example `Demo 12.48`). The status row has
no separate dummy marker. Indicator positions stay fixed on every screen.

Wi-Fi association is still connecting. Only an available preferred IPv4 address
makes `Wi-Fi` connected. Connect failure, disconnection or removal of the last
preferred IPv4 address clears connectivity. Automatic retry/reconnection uses
the worker described below. Nexus status describes the event source, not acquisition.
Dummy Nexus connectivity is independent of Wi-Fi.

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
retained. Real Nexus transport/reconnection remains pending and independent.

## Content rows (2–4)

| Lifecycle | Row 2 | Row 3 | Row 4 |
|---|---|---|---|
| Boot | NEXUS HUD | Booting | |
| No known session | Waiting for | status | |
| Initialized / drained | Ready | abbreviated session | |
| START_REQUESTED | Start sent | Waiting | |
| Warm-up | Warming up | | |
| Official, before data | LIVE | Waiting data | |
| Official, fresh result | Left ankle | LI 0-6 Hz | 12.48 |
| Official, stale result | Left ankle | LI 0-6 STALE | 12.48 |
| STOP_REQUESTED | Stop sent | Waiting | |
| Stopped, draining | Finalizing | | |
| Startup or relevant failure | ERROR | short reason | |

The table shows lifecycle content; simulated screens additionally show `Demo`
in row 4 or before the result value.

The title appears only at boot. Session/location/error strings are bounded,
converted to printable ASCII and clipped deterministically to 12 characters.
Nothing wraps or scrolls. Numeric output has two decimals (compact scientific notation when needed to
fit the `Demo` prefix); excessive magnitudes
show `Out of range`, and nonfinite inputs are rejected. `LI 0-6 Hz` is the
projected loading-intensity band label, with no invented output unit or severity.
The current dummy supplies realistic left/right values around 12.48 and 11.76;
these are demonstration values, not computed Core measurements.

On source loss, the indicator becomes hollow and retained values show `STALE`.
Real-source values would also become stale on Wi-Fi loss. Simulated values do
not depend on Wi-Fi. Samples also become stale after five seconds without an
update, with up to 500 ms tick latency. Reconnection alone does not make an old
sample fresh; each sensor needs a new result. Loss never implies Core stopped
streaming and never sends a stop command.

## Controls and confirmation

GPIO edges schedule 30 ms debounce work. A stable active edge begins a 350 ms
single-press window; release tracking prevents a held press from repeating.
The second stable press inside the window cancels the pending single and posts
a double press. Gesture callbacks only queue events.

- Single from READY sends start and enters START_REQUESTED.
- Single during START_REQUESTED, warm-up or official streaming sends stop and
  enters STOP_REQUESTED. Stop can supersede a pending start during startup.
- While stop is pending, further singles send nothing. Start cannot be sent
  again while awaiting start confirmation. Double presses never send commands.
- Double during official streaming cycles the retained sensor views locally;
  outside streaming it does nothing.
- Source disconnection disables command sending. A future real source would
  additionally require Wi-Fi connectivity. Dummy controls work without an AP.

A successful send only means the command was queued. `stream_started` or
warm-up events confirm startup, `stream_official_started` confirms live data,
`stream_stopped` confirms draining, and `stream_drained` returns to Ready.
A late start confirmation cannot cancel a pending stop. There is no fabricated
confirmation or command retry on a timeout; transport-level recovery remains
pending. Send failure produces a short error screen.

The dummy independently announces connecting, connected, waiting and initialized
states. It then waits for queued simulated start/stop commands. Start begins
warm-up, later official streaming and periodic left/right values. Stop interrupts
startup or streaming, emits stopped, then drained after a simulated delay.
No readiness command is needed and no real network command is sent.

## Validation

Run `bash tests/run.sh` for host C tests with address/undefined-behaviour
sanitizers (leak detection disabled for the sandbox). They cover lifecycle and
request transitions, stop during pending start, duplicate stop suppression,
no command on double press, view retention/selection, all 18 connectivity/mode
combinations and both blink phases across every screen state, text bounds,
numeric limits, sample expiry, reconnect freshness, inline event ownership,
and GPIO debounce/deferred-single/double/held-press behaviour using fake GPIO
and deterministic work scheduling. Wi-Fi tests exercise the real retry policy
and manager iterations with a fake asynchronous driver: initial request failure,
bounded backoff and cap, reset only on IP, association with stalled DHCP, late IP
after cancellation, AP loss/recovery, IP removal, duplicate init/connect calls,
foreign-interface events, callback isolation and no overlapping driver requests.
Tests advance a fake clock, so they do not sleep for retry intervals.

Build using `bash build.sh` (existing west board, overlay and credentials flow).
In a restricted environment, use writable cache paths with the equivalent:

```sh
source ./env.sh
export CCACHE_DIR=/tmp/hud-ccache
export CCACHE_TEMPDIR=/tmp/hud-ccache-tmp
mkdir -p "$CCACHE_TEMPDIR"
west build -p always -b xiao_esp32s3/esp32s3/procpu/sense "$APP" \
  --build-dir "$APP/build" -- \
  -DEXTRA_CONF_FILE="$APP/wifi_credentials.conf" \
  -DUSER_CACHE_DIR=/tmp/hud-zephyr-cache
```

Hardware acceptance still requires:

- Visible boot with no AP; dummy reaches Ready and responds to controls despite
  Wi-Fi failure.
- Boot with the AP unavailable, then enable it: Wi-Fi must connect without a
  reset. Disable and restore the AP again and check loss/recovery indicators.
- Association stays blinking until an IP is assigned; AP loss/address removal
  shows disconnected without changing the session lifecycle.
- Associate to an AP with DHCP disabled: within 45 seconds, the attempt must be
  cancelled and retried. Restore DHCP and check eventual recovery.
- Confirm logs show bounded backoff, reset after IP, and no overlapping attempts.
- `Demo` is legible in the content area and clearly identifies simulated data.
- All content fits four rows, with clean hollow/filled circles and no overlap.
- Single press starts/stops the simulated source; double press changes sensor
  without first stopping. Repeated presses during pending stop send no duplicate.
- Start/stop during warm-up, repeated cycles, stale-data display and long runs.

Pixel polarity, physical legibility, I2C updates, actual driver event ordering
and physical button timing cannot be proven by host tests. No device was flashed
as part of this change.
