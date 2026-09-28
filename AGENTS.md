# Agent Context: SddmSystemInfo

## Project Overview

SddmSystemInfo is a Qt6/QML extension plugin for SDDM greeter themes. It provides the QML type `JsonStatusReader`, which monitors local JSON files and exposes their contents as QML properties.

The repository additionally contains `prelogin-statusd` — an optional system daemon that publishes status information on the D-Bus system bus for data that is **not** already standardized and available (e.g., Tailscale status, directory service reachability).

- **Language:** C++17 (Qt6)
- **Build System:** CMake (min. 3.16)
- **License:** GPL-2.0-or-later
- **Plugin install target:** `/usr/lib/qt6/qml/SddmSystemInfo/`
- **Daemon install target:** `/usr/bin/prelogin-statusd`
- **Build environment:** The QML plugin is built in the opencode development environment. `prelogin-statusd` is built and tested in a **separate Distrobox/container environment** (due to D-Bus system bus requirements and root privileges).

---

## Project Structure

```text
sddm-systeminfo/
├── AGENTS.md                   # This file (agent context)
├── CMakeLists.txt              # Build configuration (QML plugin)
├── qmldir                      # QML module definition
├── LICENSE                     # GPL-2.0+
├── src/                        # QML plugin sources
│   ├── sddmsysteminfo_plugin.h / .cpp
│   └── jsonstatusreader.h / .cpp
├── prelogin-statusd/           # Optional system daemon
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── configreader.h / .cpp
│   │   ├── statusdaemon.h / .cpp
│   │   ├── tailscaleprovider.h / .cpp
│   │   ├── directoryserviceprovider.h / .cpp
│   │   ├── directorychecker.h        # Interface
│   │   ├── ldapchecker.h / .cpp      # LDAP over TCP
│   │   ├── ldapshttpschecker.h / .cpp # LDAPS over TLS
│   │   └── kerberoschecker.h / .cpp   # Kerberos TCP+UDP
│   ├── dbus/
│   │   └── org.prelogin.Status1.conf
│   └── config/
│       └── config.ini.example
├── docs/                       # User & developer documentation
│   ├── README.md
│   ├── note.md
│   └── prelogin-status-concept.md
└── packaging/
    ├── arch/                   # AUR PKGBUILD
    └── debian/                 # Debian/Ubuntu packaging
```

---

## Build & Test

### Build QML Plugin

```bash
mkdir build && cd build
cmake ..
make
sudo make install
```

### Build `prelogin-statusd`

```bash
cd prelogin-statusd
mkdir build && cd build
cmake ..
make
sudo make install
```

### Dependencies

| Distro | Packages (Plugin) | Packages (Daemon) |
|--------|------------------|-------------------|
| Arch | `base-devel`, `cmake`, `qt6-base`, `qt6-declarative` | `qt6-dbus`, `qt6-network` |
| Debian/Ubuntu | `build-essential`, `cmake`, `qt6-base-dev`, `qt6-declarative-dev` | `qt6-base-dev` |

### Verify Installation

```bash
# QML plugin
ls /usr/lib/qt6/qml/SddmSystemInfo/
# Expected: libSddmSystemInfo.so, qmldir

# Daemon
ls /usr/bin/prelogin-statusd
ls /usr/share/dbus-1/system.d/org.prelogin.Status1.conf
ls /etc/prelogin-statusd/config.ini
```

### SDDM Greeter Test

```bash
sddm-greeter --test-mode --theme /usr/share/sddm/themes/breeze/
# In the QML console: import SddmSystemInfo 1.0
```

### Invalidate Qt Plugin Cache

```bash
rm -rf ~/.cache/sddm/
sudo rm -rf /var/cache/sddm/
```

---

## Coding Conventions

### C++

- **Standard:** C++17
- **Qt guidelines:** camelCase for methods/variables, PascalCase for classes, `m_` prefix for members
- **String literals:** Prefer `QStringLiteral(...)` for non-translated strings
- **QLatin1String:** Use for comparisons without allocation
- **QObject parents:** Always pass `this` as parent for Qt objects
- **Signals/Slots:** Use new signal-slot syntax (`&Class::signal`)

### Example (as in codebase)

```cpp
class JsonStatusReader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    // ...
private:
    QFileSystemWatcher *m_watcher;
    QUrl m_source;
    QJsonObject m_data;
    QString m_error;
    bool m_valid = false;
    QTimer *m_reloadTimer;
};
```

### Header Guards

```cpp
#ifndef JSONSTATUSREADER_H
#define JSONSTATUSREADER_H
// ...
#endif
```

---

## Architecture Notes

### `JsonStatusReader` (QML Plugin)

- Uses `QFileSystemWatcher` for native file monitoring (no XMLHttpRequest)
- **Debouncing:** `QTimer` with 100ms interval catches rapid file operations (`mv tmp final`)
- Monitors both file and directory to detect replacements
- Re-adds files to the watcher on changes (important because `fileChanged` only fires once on replacement)
- Only local files (`file://` scheme) are supported
- Emits dedicated error messages for: non-local file, file not found, open error, JSON parse error, root not an object

### Plugin Registration

```cpp
qmlRegisterType<JsonStatusReader>(uri, 1, 0, "JsonStatusReader");
```

- Module name: `SddmSystemInfo`
- Version: `1.0`
- Type: `JsonStatusReader`

### `prelogin-statusd` (System Daemon)

- Runs on the **D-Bus system bus** as `org.prelogin.Status1`
- Provides **only** information that is **not** already available through NetworkManager, UPower, hostname1, etc.
- Uses Qt6-DBus for D-Bus registration and properties
- Providers are specialized C++ classes (e.g., `TailscaleProvider`), no external dependencies for standard info

### Directory Service Checker (Daemon)

`DirectoryServiceProvider` selects a specialized checker based on configuration (`Type = ldap | ldaps | kerberos`):

| Checker | Protocol | Check Method |
|---------|----------|-------------|
| `LdapChecker` | LDAP (389/tcp) | BER-encoded anonymous bind request, parsed result code 0 |
| `LdapsChecker` | LDAPS (636/tcp) | `QSslSocket` with TLS handshake, then identical LDAP bind |
| `KerberosChecker` | Kerberos (88/tcp+udp) | TCP connection **or** UDP datagram to KDC |

All checkers support configurable endpoints (`host`, `host:port`) and timeouts. No checker performs actual authentication — only protocol reachability is checked.

### Rule for new data

> Before a new type or property lands in the daemon, check: Is there already a standardized D-Bus service for it? If yes → client uses it directly. If no → build a daemon provider.

---

## Important Implementation Details

### Error Handling (QML Plugin)

- All errors set `valid = false` and populate `error`
- `dataChanged` is only emitted on actual change (`m_data != newData`)
- On parse errors, `data` is cleared (`QJsonObject()`)

### QFileSystemWatcher Limitations

- When a file is replaced by `mv`/`rename`, the watch on the old inode disappears
- Therefore: call `setupWatcher()` on every `fileChanged` to register the new file
- Directory monitoring additionally ensures reliable replacement detection

### D-Bus Registration (Daemon)

- The daemon requires privileges on the system bus (policy file + root/appropriate user)
- Registration fails if another process already holds the bus name
- `QDBusConnection::systemBus()` must be available (D-Bus daemon running)

### Retry Behavior (Daemon)

When a service is unreachable, `StatusDaemon` automatically switches to a retry mode:

| Situation | Interval | Max Duration | Return |
|-----------|----------|--------------|--------|
| Normal check | `UpdateIntervalSeconds` (Config) | ∞ | — |
| Service unreachable | **5 seconds** | **5 minutes** | Automatic after reachability or timeout |

- `QTimer` for normal rhythm + separate `QTimer` for retry
- `evaluateAndSwitchMode()` decides after each refresh which timer must be active
- Prevents permanent polling on permanent outage, but accelerates detection for temporary outages

---

## Release Process

### Before every release

- [ ] Version updated in `CMakeLists.txt` (`project(VERSION X.Y.Z)`)
- [ ] Version updated in `prelogin-statusd/CMakeLists.txt`
- [ ] `docs/note.md` updated if necessary
- [ ] `docs/README.md` checked for up-to-date links/version
- [ ] Compatibility tested with Qt 6.4+
- [ ] Compatibility tested with Qt 6.6+

### Version Numbers (SemVer)

- `MAJOR` — API break (properties removed, signatures changed)
- `MINOR` — New features, backward compatible
- `PATCH` — Bugfixes only

### Packaging

- **Arch Linux:** `packaging/arch/PKGBUILD` → AUR
- **Debian/Ubuntu:** `packaging/debian/` → PPA or local `dpkg-buildpackage`
- Generate `.SRCINFO`: `makepkg --printsrcinfo > .SRCINFO`

---

## Testing

### Integration Tests (`tests/integration/`)

Runs the daemon against a **temporary D-Bus system bus** — no systemd required. Useful for CI and container environments (Distrobox, Docker).

```bash
cd tests/integration
bash run_tests.sh
```

**Verified in tests:**
- D-Bus registration (`org.prelogin.Status1`)
- Properties: `Version`, `TailscaleState`, `DirectoryServiceState`, `TailscalePeerCount`, ...
- Methods: `GetCapabilities()`, `Refresh()`
- Config file loading (`UpdateIntervalSeconds`, `ExposeErrors`, ...)

**Test environment packages:**
| Distro | Packages |
|--------|----------|
| Arch | `dbus`, `qt6-base`, `python3`, `bash` |
| Debian/Ubuntu | `dbus-daemon`, `dbus-x11`, `qt6-base-dev`, `python3`, `bash` |

**Tailscale in tests:**
- The test config points to a **non-existent socket** — the daemon correctly reports `unavailable`.
- To test with real Tailscale data, start `tailscaled` or provide a mock Unix socket that replies to `/localapi/v0/status`.

---

## Future Plans

`prelogin-statusd` is actively in development within this repository. See `docs/prelogin-status-concept.md` for the revised architecture concept (gap-filler instead of centralizer).

**Important:** Changes to the QML plugin and the daemon are independently versionable. The daemon is optional — the plugin works without `prelogin-statusd`.

---

## Common Pitfalls

1. **`.gitignore` contains `CMakeLists.txt`** — This is intentional (or a bug?), check if it gets committed
2. **QML_IMPORT_PATH:** After installation, the Qt plugin cache may need invalidation
3. **Greeter runs as `sddm` user:** Local files must be readable by this user
4. **Debouncing:** The 100ms is a trade-off — too short = duplicate reloads, too long = noticeable delay
5. **Daemon without D-Bus policy:** If `/usr/share/dbus-1/system.d/org.prelogin.Status1.conf` is missing, the daemon cannot register the bus name
6. **No D-Bus duplicates:** If a type is built for NetworkManager/UPower etc., it is probably wrong — the data is already on the bus
