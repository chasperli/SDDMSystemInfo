# Release Notes — SddmSystemInfo

This document describes step by step how the project is made available to end users. The project consists of two independently versioned components:

1. **`sddm-systeminfo`** — QML Plugin
2. **`prelogin-statusd`** — Optional system daemon

---

## Arch Linux — AUR (Arch User Repository)

### Prerequisites
- An AUR account (https://aur.archlinux.org/)
- `base-devel`, `git`

### QML Plugin

```bash
git clone ssh://aur@aur.archlinux.org/sddm-systeminfo.git
cd sddm-systeminfo
cp ~/Projects/sddm-systeminfo/packaging/arch/PKGBUILD .
# Adjust placeholders: pkgver, pkgrel, sha256sums
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Release X.Y.Z"
git push origin master
```

### Daemon (separate AUR package)

```bash
git clone ssh://aur@aur.archlinux.org/prelogin-statusd.git
cd prelogin-statusd
cp ~/Projects/sddm-systeminfo/packaging/arch/PKGBUILD .
# Adjust: pkgname=prelogin-statusd, dependencies +qt6-dbus
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Release X.Y.Z"
git push origin master
```

---

## Ubuntu / Debian — PPA (Personal Package Archive)

### Prerequisites
- A Launchpad account (https://launchpad.net/)
- `devscripts`, `build-essential`, `debhelper`, `dh-make`, `lintian`

### QML Plugin

```bash
cd ~/Projects/sddm-systeminfo
cp -r packaging/debian/ debian/
dch -i  # Update changelog
dpkg-buildpackage -us -uc -b
```

### Daemon

```bash
cd ~/Projects/sddm-systeminfo/prelogin-statusd
cp -r ../packaging/debian/ debian/
# Adjust debian/control: package name, dependencies
# Adjust debian/rules: daemon installation, D-Bus policy
dch -i
dpkg-buildpackage -us -uc -b
```

---

## Automation with GitHub Actions

Recommended: Create a GitHub Action `.github/workflows/build.yml` that triggers on every tag (`v*`) and automatically:
1. Builds the source tarball
2. Builds Debian and Arch packages for **both** components
3. Uploads both as GitHub Releases

---

## General Checklist

### Before every release

- [ ] Version updated in `CMakeLists.txt` (`project(VERSION X.Y.Z)`)
- [ ] Version updated in `prelogin-statusd/CMakeLists.txt` (if changed)
- [ ] `docs/README.md` checked for up-to-date links and version
- [ ] `docs/prelogin-status-concept.md` updated if necessary
- [ ] Compatibility tested with Qt 6.4+
- [ ] Compatibility tested with Qt 6.6+ (latest)

### Version numbers

We follow SemVer **per component** independently:
- `MAJOR` — API break (properties removed, signatures changed)
- `MINOR` — New features, backward compatible
- `PATCH` — Bugfixes

**Important:** Plugin and daemon may carry different version numbers. In changelogs and Git tags, prefix if needed: `plugin-1.2.0`, `daemon-1.1.0`.

### Test commands

```bash
# Plugin installed?
ls /usr/lib/qt6/qml/SddmSystemInfo/

# Daemon installed?
ls /usr/bin/prelogin-statusd
ls /usr/share/dbus-1/system.d/org.prelogin.Status1.conf

# Can greeter load the plugin?
sddm-greeter --test-mode --theme /usr/share/sddm/themes/breeze/
# In the QML console: import SddmSystemInfo 1.0

# Invalidate Qt plugin cache (if needed)
rm -rf ~/.cache/sddm/
sudo rm -rf /var/cache/sddm/
```

---

## Support & Bug Reports

Use GitHub Issues for:
- Build failures on specific distros
- Qt version incompatibilities
- Feature requests
- D-Bus registration problems

---

**Goal:** Users should simply run `yay -S sddm-systeminfo prelogin-statusd` or `apt install sddm-systeminfo prelogin-statusd`.
