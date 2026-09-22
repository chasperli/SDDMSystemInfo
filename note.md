# Veröffentlichungs-Notizen — SddmSystemInfo

Dieses Dokument beschreibt Schritt für Schritt, wie das Plugin für Endnutzer verfügbar gemacht wird.

---

## Arch Linux — AUR (Arch User Repository)

### Voraussetzungen
- Einen AUR-Account (https://aur.archlinux.org/)
- `base-devel`, `git`

### Repository anlegen

```bash
git clone ssh://aur@aur.archlinux.org/sddm-systeminfo.git
cd sddm-systeminfo
```

Kopiere die Datei `packaging/arch/PKGBUILD` in das Repo:

```bash
cp ~/Projects/sddm-systeminfo/packaging/arch/PKGBUILD .
```

Setze Platzhalter in der PKGBUILD an:
- `pkgver` → aktuelle Version (z. B. 1.0.0)
- `pkgrel` → auf 1 setzen
- `sha256sums` → berechnen mit `updpkgsums` oder `makepkg -g`
- `source` → auf den GitHub Release-Tarball zeigen

### Testen

```bash
makepkg -si
pacman -Q sddm-systeminfo
```

### Hochladen

```bash
git add PKGBUILD .SRCINFO
git commit -m "Initial release 1.0.0"
git push origin master
```

`.SRCINFO` generieren mit `makepkg --printsrcinfo > .SRCINFO`.

### Update

Bei einem neuen Release:
1. `pkgver` in PKGBUILD erhöhen
2. `sha256sums` neu berechnen
3. `git commit`, `git push`

---

## Ubuntu / Debian — PPA (Personal Package Archive)

### Voraussetzungen
- Ein Launchpad-Account (https://launchpad.net/)
- `devscripts`, `build-essential`, `debhelper`, `dh-make`, `lintian`

### Vorbereitung

```bash
cd ~/Projects/sddm-systeminfo
mkdir debian-release && cd debian-release
```

### Debian-Verzeichnis vorbereiten

```bash
cp -r ../packaging/debian/ .
# Oder, falls du dh_make nutzen willst:
dh_make --createorig -p sddm-systeminfo_1.0.0
```

### PPA einrichten

```bash
dput ppa:dein-username/sddm-extras ../sddm-systeminfo_1.0.0-1_source.changes
```

Oder lokal bauen:

```bash
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../sddm-systeminfo_1.0.0-1_amd64.deb
```

### Automatisierung mit GitHub Actions

Empfohlen: GitHub Action `.github/workflows/build.yml` anlegen, die bei jedem Tag (`v*`) automatisch:
1. Source-Tarball baut
2. Debian-Paket baut
3. Arch-Paket baut
4. Beide als GitHub Releases hochlädt

### Update

Bei neuem Release:
1. `debian/changelog` mit `dch -i` erweitern
2. `git tag v1.0.1`
3. `git push --tags`
4. CI baut und veröffentlicht automatisch

---

## Allgemeine Checkliste

### Vor jedem Release

- [ ] Version in `CMakeLists.txt` aktualisiert
- [ ] `CHANGELOG.md` (sofern vorhanden) aktualisiert
- [ ] `README.md` prüfen, Links funktionieren
- [ ] Kompatibilität mit Qt 6.4+ getestet
- [ ] Kompatibilität mit Qt 6.6+ getestet (neueste)

### Versionsnummern

Wir folgen SemVer:
- `MAJOR` — API-Bruch (neue Properties entfernt, Signatur geändert)
- `MINOR` — Neue Features, rückwärtskompatibel
- `PATCH` — Bugfixes

### Test-Befehle

```bash
# Plugin installiert?
ls /usr/lib/qt6/qml/SddmSystemInfo/

# Greeter kann es laden?
sddm-greeter --test-mode --theme /usr/share/sddm/themes/breeze/
# Dann in der QML-Konsole: import SddmSystemInfo 1.0

# Qt-Plugin-Cache invalidieren (falls nötig)
rm -rf ~/.cache/sddm/
sudo rm -rf /var/cache/sddm/
```

---

## Support & Bugreports

GitHub Issues nutzen für:
- Build-Fehler auf bestimmten Distros
- Qt-Version-Inkompatibilitäten
- Feature-Requests

---

**Ziel:** Nutzer sollen einfach nur `yay -S sddm-systeminfo` oder `apt install sddm-systeminfo` aufrufen können.
