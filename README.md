# SddmSystemInfo — SDDM QML System Info Plugin

A universal Qt6/QML extension for SDDM greeter themes. Provides a `JsonStatusReader` type that watches local JSON files and exposes their contents as QML properties.

The repository also optionally contains `prelogin-statusd` — a system daemon that publishes status information on the D-Bus system bus for data that is **not** already available through standardized services (e.g., Tailscale status).

> **Decision rule for clients:** Standard information (network, battery, hostname) is already available on the D-Bus (`org.freedesktop.NetworkManager`, `org.freedesktop.UPower`, `org.freedesktop.hostname1`). Access these directly. Only use the custom daemon for Tailscale, directory service reachability, etc.

## Features

- **No XMLHttpRequest** — native file watching via `QFileSystemWatcher`
- **Universal** — works in any SDDM theme
- **Auto-reload** — file changes are detected automatically
- **Debounced** — short write operations (`mv tmp final`) are handled cleanly
- **Safe** — the greeter runs as an unprivileged user, local file access still works
- **Optional daemon** — provides gap-filling information over D-Bus, no duplication

## Installation

### Dependencies

| Distro | Packages (Plugin) | Packages (Daemon, optional) |
|--------|-------------------|-----------------------------|
| Arch | `base-devel`, `cmake`, `qt6-base`, `qt6-declarative` | `qt6-dbus`, `qt6-network` |
| Debian/Ubuntu | `build-essential`, `cmake`, `qt6-base-dev`, `qt6-declarative-dev` | `qt6-base-dev` |

### Build QML Plugin

```bash
git clone https://github.com/dein-user/sddm-systeminfo.git
cd sddm-systeminfo
mkdir build && cd build
cmake ..
make
sudo make install
```

Installs the plugin to `/usr/lib/qt6/qml/SddmSystemInfo/`.

### Build `prelogin-statusd` (optional)

```bash
cd sddm-systeminfo/prelogin-statusd
mkdir build && cd build
cmake ..
make
sudo make install
```

Installs the daemon to `/usr/bin/prelogin-statusd`, the D-Bus policy to `/usr/share/dbus-1/system.d/org.prelogin.Status1.conf`, and a sample configuration to `/etc/prelogin-statusd/config.ini`.

### Packages

#### Arch Linux (AUR)

```bash
yay -S sddm-systeminfo       # Plugin only
yay -S prelogin-statusd      # Optional: daemon
```

#### Ubuntu / Kubuntu / Debian

```bash
sudo add-apt-repository ppa:dein-user/sddm-extras
sudo apt update
sudo apt install sddm-systeminfo      # Plugin only
sudo apt install prelogin-statusd     # Optional: daemon
```

## Usage in QML

### Watch JSON files (plugin)

```qml
import SddmSystemInfo 1.0

JsonStatusReader {
    id: netStatus
    source: "file:///tmp/sddm-status/network-status.json"
    onDataChanged: {
        var d = netStatus.data;
        wifiText.text = d.wlan_name || "NOT CONNECTED";
    }
}
```

### Query D-Bus information (clients)

Standard services (directly via QtDBus):
```qml
// NetworkManager: org.freedesktop.NetworkManager
// UPower: org.freedesktop.UPower
// hostname1: org.freedesktop.hostname1
```

Custom daemon (if installed):
```qml
import Qt6DBus 1.0

DBusInterface {
    service: "org.prelogin.Status1"
    path: "/org/prelogin/Status1"
    iface: "org.prelogin.Status1"
}
```

## API

### JsonStatusReader

| Property | Type | Description |
|----------|------|-------------|
| `source` | `url` | Local file path (`file://` scheme) |
| `data` | `QJsonObject` | Parsed JSON content |
| `error` | `string` | Last error message or empty |
| `valid` | `bool` | `true` if the file exists and contains valid JSON |

| Method | Description |
|--------|-------------|
| `reload()` | Re-reads the file immediately |

### prelogin-statusd (D-Bus Properties)

| Property | Type | Description |
|----------|------|-------------|
| `Version` | `string` | Daemon version |
| `TailscaleState` | `string` | `unknown`, `disconnected`, `connected` |
| `TailscalePeerCount` | `uint32` | Number of visible peers |
| `TailscaleExitNodeActive` | `bool` | An exit node is in use |
| `DirectoryServiceState` | `string` | `unknown`, `unavailable`, `reachable`, `error` |
| `DirectoryServiceType` | `string` | `none`, `ldap`, `ldaps`, `kerberos` |

## Configuration

### `prelogin-statusd`

The daemon reads its configuration from `/etc/prelogin-statusd/config.ini` (INI format). On first `make install`, a sample file is copied to this location.

**Example configuration:**

```ini
[General]
UpdateIntervalSeconds=60
ExposeErrors=false

[Tailscale]
Socket=/var/run/tailscale/tailscaled.sock

[Directory]
Type=none
Endpoint=
TimeoutMilliseconds=2000
```

| Section | Key | Default | Description |
|---------|-----|---------|-------------|
| `General` | `UpdateIntervalSeconds` | `60` | Normal check interval in seconds (`0` = off) |
| `General` | `ExposeErrors` | `false` | Publish detailed error messages on D-Bus |
| `Tailscale` | `Socket` | `/var/run/tailscale/tailscaled.sock` | Path to tailscaled Unix socket |
| `Directory` | `Type` | `none` | `none`, `ldap`, `ldaps`, `kerberos` |
| `Directory` | `Endpoint` | *(empty)* | Hostname or IP with optional port |
| `Directory` | `TimeoutMilliseconds` | `2000` | Timeout for reachability checks |

### Retry Behavior

If a service is **not reachable** (e.g., Tailscale `disconnected` or Directory `unavailable`), the daemon automatically enters a **retry mode**:

- **Interval:** Every **5 seconds**
- **Duration:** Maximum **5 minutes**
- As soon as the service is reachable again, the normal interval (`UpdateIntervalSeconds`) resumes
- After the 5-minute window expires, the daemon also returns to the normal interval, even if the service is still down

This ensures that a temporary network outage or a still-starting service is detected quickly, without consuming resources permanently.

An alternative configuration path can be passed at startup:

```bash
prelogin-statusd --config /path/to/config.ini
```

## Testing

### Integration Tests (Daemon)

Automated integration tests are available in `tests/integration/`. They run **without systemd** by launching a temporary D-Bus system bus.

```bash
cd tests/integration
bash run_tests.sh
```

**What the tests verify:**
- D-Bus registration as `org.prelogin.Status1`
- Property reads (Version, TailscaleState, DirectoryServiceState, ...)
- Method calls (GetCapabilities, Refresh)
- Configuration file parsing

**Output example:**
```
[PASS] D-Bus registration and property Version
[PASS] Property TailscaleState reports unavailable when socket missing
...
Results: 8/8 passed (100%)
```

**Test environment requirements:**
- `python3` (for the Tailscale mock server)
- `dbus-daemon`, `dbus-x11` (for `dbus-send`)
- `qt6-base-dev` (on Ubuntu), or `qt6-base` + `qt6-dbus` (on Arch)
- `bash`

See `tests/requirements.md` for a detailed package list per distribution.

### Tailscale Testing

The integration tests detect a missing Tailscale socket as `unavailable`. This is a valid test state.

To test with a real Tailscale connection, install `tailscale` in the test environment and start `tailscaled` with a valid socket path. For CI, a mock Unix socket can be used instead.

## License

GPL-2.0-or-later
