# Prelogin Status Service — Revised Concept

## 1. Starting Point & Correction

The original concept envisioned a centralizing D-Bus daemon that collected system information from various sources and published it as its own D-Bus API. This is **not sensible** when the information is already available in a standardized form on the D-Bus system bus.

**New guideline:** `prelogin-statusd` is a **gap-filler**, not a wrapper. It publishes exclusively status information that is **not yet present** on the system bus. All clients access existing system services directly for standard information.

---

## 2. What is already available on D-Bus

This information must **not** be duplicated. Clients (SDDM-QML, CLI, Plasma) access these services directly:

| Information | D-Bus Service | Path/Interface |
|-------------|---------------|----------------|
| Network status, SSID, signal strength | `org.freedesktop.NetworkManager` | `/org/freedesktop/NetworkManager` |
| Battery state, charging state | `org.freedesktop.UPower` | `/org/freedesktop/UPower/devices/battery_BAT0` |
| Hostname, static system info | `org.freedesktop.hostname1` | `/org/freedesktop/hostname1` |
| System time, uptime | `org.freedesktop.timedate1`, `/proc/uptime` | — |

> **Consequence:** The QML plugin or client uses QtDBus directly for these standard services. There is no reason to route `WifiSsid` or `BatteryPercent` through an additional daemon.

---

## 3. What prelogin-statusd actually provides

A custom service is only justified for information that has **no** standardized D-Bus endpoint:

| Domain | Example Properties | Justification |
|--------|-------------------|---------------|
| **Tailscale** | `TailscaleState`, `TailscalePeerCount`, `ExitNodeActive` | Tailscale provides no system bus daemon. Status comes from `tailscale status` or the local API socket. |
| **Directory Services** | `DirectoryServiceState`, `DirectoryServiceType` | Requires an active check (LDAP connect, Kerberos reachability). No existing service measures this regularly. |
| **VPN (non-NM)** | `WireguardActive`, `CustomVpnState` | WireGuard has no own D-Bus service. OpenVPN may not either, depending on setup. |
| **Derived states** | `InternetReachable`, `CorporateNetworkReachable` | Aggregation of multiple sources into a semantic result that does not exist anywhere directly. |

> **Important:** The daemon does not provide raw data that NM/UPower/etc. already have. It only provides data that otherwise does **not** exist on the bus at all.

---

## 4. Architecture

```text
+----------------------------------------------------------+
|  System Bus (already present)                             |
|  ┌─────────────────┐  ┌─────────────────┐                |
|  | NetworkManager  |  | UPower          |                |
|  | hostname1       |  | timedate1       |                |
|  └────────┬────────┘  └────────┬────────┘                |
|           │                    │                          |
|           └──────┬─────────────┘                          |
|                  │   Direct access                        |
+------------------┼────────────────────────────────────────+
                   │
           +-------+-------+
           │   Clients     │
           │  SDDM/QML     │
           │  Plasma       │
           │  CLI          │
           +-------+-------+
                   │
                   │  For gap information
                   v
+------------------------------------------+
|  prelogin-statusd                        │
|  ┌─────────────────────────────────┐    │
|  | TailscaleProvider               │    │
|  | DirectoryServiceProvider        │    │
|  | CustomVpnProvider (optional)    │    │
|  | Aggregator                      │    │
|  └─────────────────────────────────┘    │
|  Busname: org.prelogin.Status1          │
+------------------------------------------+
```

### 4.1 Direct access vs. Daemon

- **Clients shall access `org.freedesktop.NetworkManager` directly** when they need network info.
- **Clients shall access `org.freedesktop.UPower` directly** when they need battery data.
- **Clients shall access `org.prelogin.Status1` only** when they need Tailscale, directory, or aggregation data.

### 4.2 Proposed D-Bus interface (reduced)

```text
Busname:    org.prelogin.Status1
Objectpath: /org/prelogin/Status1
Interface:  org.prelogin.Status1
```

**Properties (only own domains):**

```text
Version                    string
TailscaleState             string      # unknown | disconnected | connected
TailscalePeerCount         uint32
TailscaleExitNodeActive    boolean
DirectoryServiceState      string      # unknown | unavailable | reachable | timeout | error
DirectoryServiceType       string      # ldap | ldaps | kerberos | none
VpnState                   string      # unknown | inactive | active
LastUpdateTimestamp        uint64
```

**Methods:**

```text
Refresh()              # Forces manual update of own providers
GetCapabilities()      # Lists available providers
```

**Signals:**

```text
PropertiesChanged                # Standard D-Bus
TailscaleStateChanged(string)
DirectoryServiceStateChanged(string)
```

> No `WifiSsid`, `BatteryPercent`, `Hostname` properties. These exist elsewhere.

---

## 5. D-Bus Policies & Security

Since the daemon runs on the **system bus**, a policy file is required:

```xml
<!-- /usr/share/dbus-1/system.d/org.prelogin.Status1.conf -->
<busconfig>
  <policy user="root">
    <allow own="org.prelogin.Status1"/>
  </policy>
  <policy context="default">
    <allow send_destination="org.prelogin.Status1"/>
    <allow receive_sender="org.prelogin.Status1"/>
  </policy>
</busconfig>
```

### Security principles (retained)

- **Minimal disclosure:** Peer names, IP addresses, internal endpoints shall not be published by default
- **Read-only:** No methods for modifying system state
- **Sandboxing:** Dedicated system user, `NoNewPrivileges=true`, `PrivateTmp=true`
- **External processes:** No shell invocation, fixed argument lists, timeouts

---

## 6. Providers (only where needed)

### 6.1 TailscaleProvider

- Communicates with `tailscaled` via its LocalAPI (Unix socket `/var/run/tailscale/tailscaled.sock`) or parses `tailscale status --json`
- Provides: Backend state, connected/disconnected, peer count, exit-node status
- Does not expose peer names or IPs by default

### 6.2 DirectoryServiceProvider

- Performs configurable checks (TCP connect, LDAP StartTLS bind probe without credentials)
- States: `unknown | unavailable | reachable | timeout | error`
- Network endpoint shall not be published as a property by default

### 6.3 No providers for:

- NetworkManager (already on D-Bus)
- UPower (already on D-Bus)
- System load (readable via `/proc`, optionally via systemd)
- Hostname (already on D-Bus)

---

## 7. SDDM Integration

The current QML plugin (`SddmSystemInfo` with `JsonStatusReader`) remains a **separate, standalone tool**. For the D-Bus variant, there are two options:

### Option A: D-Bus capable QML plugin (new)

An additional QML module that either:
- accesses `org.freedesktop.NetworkManager` and `org.prelogin.Status1` directly via D-Bus, **or**
- is a universal `DBusStatusReader` that monitors arbitrary bus names/properties

### Option B: Clients use D-Bus directly in QML

```qml
import Qt6DBus 1.0

DBusInterface {
    service: "org.freedesktop.NetworkManager"
    path: "/org/freedesktop/NetworkManager"
    iface: "org.freedesktop.NetworkManager"
    // Properties directly usable
}
```

> Pragmatically: A new plugin `PreloginStatus 1.0` with convenience elements for D-Bus would be sensible, but it should **not** centralize data that is already on the bus.

---

## 8. Packaging

Since the daemon is much leaner, the package structure simplifies:

```text
prelogin-statusd              # Core daemon + systemd unit + D-Bus policy
prelogin-statusd-tailscale    # Optional: Tailscale provider
prelogin-statusd-sddm         # Optional: QML plugin for SDDM
prelogin-statusd-cli          # Optional: CLI client (statusctl)
```

No separate provider packages for network or battery, since the daemon no longer offers these.

---

## 9. Differences from the original concept

| Point | Original | Revised |
|-------|----------|---------|
| Role | Centralized system status daemon | Gap-filler for missing D-Bus services |
| NM/UPower/Hostname | Own properties in daemon | Clients access directly |
| QML plugin | Reads JSON (current) or daemon properties | Uses QtDBus directly for system services |
| Number of providers | Many (NM, Tailscale, Battery, System, LDAP) | Few (only Tailscale, Directory, VPN) |
| Package split | Complex, many split packages | Lean, only where needed |
| Added value | One bus name for everything | One bus name for what is otherwise missing |

---

## 10. Next Steps (Proposal)

1. **Prioritize:** Which information from the concept is actually needed in the SDDM theme?
2. **Evaluate:** Is it sufficient if the QML plugin uses `Qt6DBus` directly?
3. **Prototype:** Minimal `prelogin-statusd` with only one provider (e.g., Tailscale)
4. **Omit:** If only Tailscale is missing, a small QML D-Bus module without a separate daemon might suffice

---

*This document replaces the original monolith concept. The goal is a lean, justifiable system service without redundant D-Bus exposure.*
