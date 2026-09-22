# SddmSystemInfo — SDDM QML System Info Plugin

Universelle Qt6/QML-Erweiterung für SDDM-Greeter-Themes. Stellt einen `JsonStatusReader`-Typ bereit, der lokale JSON-Dateien überwacht und den Inhalt als QML-Property bereitstellt.

## Features

- **Kein XMLHttpRequest** — native Dateiüberwachung via `QFileSystemWatcher`
- **Universell** — funktioniert in jedem SDDM-Theme
- **Auto-reload** — Dateiänderungen werden automatisch erkannt
- **Debounced** — kurze Schreibvorgänge (mv tmp final) werden sauber abgefangen
- **Sicher** — Greeter läuft als unprivilegierter User, Lokale-Datei-Zugriff funktioniert trotzdem

## Installation

### Aus dem Quellcode

```bash
git clone https://github.com/dein-user/sddm-systeminfo.git
cd sddm-systeminfo
mkdir build && cd build
cmake ..
make
sudo make install
```

Installiert das Plugin nach `/usr/lib/qt6/qml/SddmSystemInfo/`.

### Arch Linux (AUR)

```bash
yay -S sddm-systeminfo
# oder
paru -S sddm-systeminfo
```

### Ubuntu / Kubuntu / Debian

```bash
sudo add-apt-repository ppa:dein-user/sddm-extras
sudo apt update
sudo apt install sddm-systeminfo
```

## Abhängigkeiten

| Distro | Pakete |
|--------|--------|
| Arch | `base-devel`, `cmake`, `qt6-base`, `qt6-declarative` |
| Debian/Ubuntu | `build-essential`, `cmake`, `qt6-base-dev`, `qt6-declarative-dev` |

## Verwendung im QML

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

## API

### Properties

| Property | Typ | Beschreibung |
|----------|-----|--------------|
| `source` | `url` | Lokaler Dateipfad (schema `file://`) |
| `data` | `QJsonObject` | Geparster JSON-Inhalt |
| `error` | `string` | Letzte Fehlermeldung oder leer |
| `valid` | `bool` | `true`, wenn die Datei existiert und gültiges JSON enthält |

### Methoden

| Methode | Beschreibung |
|---------|--------------|
| `reload()` | Liest die Datei sofort neu ein |

## Lizenz

GPL-2.0-or-later
