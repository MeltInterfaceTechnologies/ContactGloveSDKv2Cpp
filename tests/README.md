# Native runtime and protocol regressions

These tests exercise the SDK and its pinned, locally patched oscpack runtime.
All UDP destinations are loopback. No glove, DivingStation, Isaac, ROS2,
haptics, or robot APIs are needed or exercised.

Build the opt-in executables, then run them through CTest so shutdown regressions
are bounded by a 60-second timeout per test:

```sh
cmake -S . -B build
cmake --build build --config Release --target runtime_regression protocol_v2_validation c_sample_regression
ctest --test-dir build -C Release --output-on-failure
```

`c_sample_regression` links the actual C sample against a socket-free mock SDK.
A receiver thread repeatedly invokes the callback while the main thread polls.
The callback ID differs from the cached ID, so the test checks that polling
uses its own SDK snapshot instead of shared callback state. The mock also tests
an empty list and an ID buffer that is too small. It does not replace
a thread sanitizer or real SDK integration testing.

Coverage:

- `runtime_pre-run`: deterministic `Break()` and `AsynchronousBreak()` before
  `Run()`, normal sequential reuse, and a receiver-start ordering controlled by
  an atomic gate. A timer provides a fail-fast fallback for a lost cancellation.
  An additional 100,000 `AsynchronousBreak()` / `Run()` pairs reuse the same
  multiplexer without timers, exceeding typical POSIX pipe capacities to catch
  accumulated, unread wake bytes. CTest bounds a blocked pipe write.
- `runtime_malformed`: seven malformed message, bundle, nested-bundle, and
  truncated-argument fixtures. Each is followed on the same UDP socket by a
  valid raw-skeleton frame; the test checks the callback count and frame data.
  Callback completion uses a condition variable, not a sleep-based absence test.
- `runtime_timestamp`: explicitly checks OSC argument positions and SDK callback
  reconstruction for zero, low-word-only, high-word-only, different nonzero words,
  and words with their sign bit set. Expected values are independent literals.
- `runtime_rapid`: 250 immediate connect/disconnect pairs, 250 concurrent
  connect/disconnect races, and 100 immediate SDK destructions after connecting.
- `runtime_callbacks`: a callback getter overlapping Disconnect, a reconnect
  waiting for that shutdown to release the same receive/send ports, destruction
  draining a callback after its last SDK access, and exception isolation between
  callbacks and subsequent messages. The Disconnect packet gates the getter so
  the lock-held-join regression cannot pass by missing the race.
- `protocol_v2_validation`: Device field validation, rejection of trailing
  skeleton arguments, cache capacity and disconnected-device callbacks, and
  cache cleanup across separate sessions and reconnects.

`runtime_rapid` is the required C++ connect/disconnect concurrency release gate.
It replaces the standalone `connect_disconnect_concurrency` test; the standalone
source and build target have been removed. The concurrent loop checks successful
connection, allowed disconnect return codes, and absence of exceptions. It reuses
one receive port to exercise reconnects, rather than changing ports on each
iteration as the standalone test did. Per-iteration port changes are not a
separate release requirement. CI runs this gate through CTest with the other
runtime modes; a failure or the 60-second timeout must block release.

The runtime executable also accepts `--pre-run`, `--malformed`, `--timestamp`, `--rapid`, `--callbacks`, or `--all`
(the default). Direct execution does not impose the CTest timeout.

Windows uses Winsock handles and oscpack's process-wide network initializer.
Runtime receive ports are probed in 20000–29999, below the usual OS ephemeral sender
ranges, to avoid a reconnect colliding with its own automatically selected
sender port. A small reservation-to-bind window remains; these tests are
serialized within CTest. The protocol test uses receive ports 30250, 30260,
and 30262 and is also serialized. Passing loopback tests does not verify physical-device
operation or LAN approval.
