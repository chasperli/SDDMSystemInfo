# Integration Test Requirements

This document describes what is required to run the automated integration tests for `prelogin-statusd`.

## Target Environment

- **OS:** Ubuntu (22.04 LTS or newer recommended, other Debian-based distros should work)
- **Container/Tool:** Distrobox (or any root-capable container with D-Bus utilities)
- **No systemd required.** The tests manage their own temporary D-Bus system bus.

## Required Packages

```bash
sudo apt update
sudo apt install -y \
    bash \
    python3 \
    dbus-daemon \
    dbus-x11 \
    cmake \
    build-essential \
    qt6-base-dev \
    libqt6dbus6
```

### Why these packages?

| Package | Purpose |
|---------|---------|
| `bash` | Test runner script (`run_tests.sh`) |
| `python3` | Tailscale mock server (`mock_tailscale.py`) |
| `dbus-daemon` | Temporary system bus for isolated D-Bus testing |
| `dbus-x11` | Provides `dbus-send` CLI tool for test assertions |
| `cmake` + `build-essential` | Compile the daemon before testing |
| `qt6-base-dev` | Qt6 headers and `find_package(Qt6)` support |
| `libqt6dbus6` | Runtime library for Qt6 D-Bus bindings |

> **Note:** `qt6-base-dev` on Ubuntu includes the DBus and Network modules; no separate `qt6-dbus-dev` package is needed.

## What the Tests Do

The integration tests (`run_tests.sh`) perform the following automatically:

1. **Compile** the daemon (`prelogin-statusd`) if not already built.
2. **Start a temporary D-Bus system bus** with a private socket and config directory.
3. **Install a temporary D-Bus policy** allowing the daemon to own `org.prelogin.Status1`.
4. **Launch the daemon** against a test configuration (`fixtures/test-config.ini`).
5. **Execute test cases** via `dbus-send` to verify:
   - D-Bus registration works
   - Properties return expected values (Version, TailscaleState, DirectoryServiceState, ...)
   - Methods respond correctly (GetCapabilities, Refresh)
   - Configuration parsing behaves as expected
6. **Shut down** daemon and temporary bus cleanly.
7. **Print a percentage score** of passed vs. total tests.

## Host vs. Container

The tests are designed to run **inside a container** (Distrobox, Docker, LXC) to avoid interfering with the host D-Bus system bus.

If you run them on the host, the temporary bus is still isolated, but the test requires root privileges to start `dbus-daemon` in system mode.

## Exit Codes

| Exit Code | Meaning |
|-----------|---------|
| 0 | All tests passed (100%) |
| 1 | One or more tests failed (score < 100%) |
| 2 | Fatal error (could not compile, could not start D-Bus, etc.) |

## Quick Start

```bash
cd /run/host/home/lukas/Projects/sddm-systeminfo/tests/integration
bash run_tests.sh
```

Expected output example:
```
[TEST] D-Bus Registration .................... PASS
[TEST] Property Version ...................... PASS
[TEST] Property TailscaleState ............... PASS
...
==============================
Results: 7/8 passed (87%)
==============================
```
