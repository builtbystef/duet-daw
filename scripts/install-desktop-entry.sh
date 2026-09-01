#!/usr/bin/env sh
#
# Put Duet in the desktop's application list, with the brand mark as its icon.
#
# Installs per-user (no root): the mark goes into the hicolor icon theme as a
# scalable SVG — every current desktop renders those, and it needs no raster
# tool on the machine — and a .desktop entry points at the Debug binary of the
# checkout this script sits in, run through pw-jack the way the checks in
# AGENTS.md run it. The entry names StartupWMClass=Duet, which is the WM_CLASS
# the JUCE window carries; matching it is what makes the taskbar group the
# running window under this entry and its icon.
#
# Re-running the script is how the entry follows a moved checkout.

set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
binary="$root/build/modules/duet_app/duet_app_artefacts/Debug/Duet"
data="${XDG_DATA_HOME:-$HOME/.local/share}"

icon_dir="$data/icons/hicolor/scalable/apps"
mkdir -p "$icon_dir"
cp "$root/modules/duet_gui/assets/brand/duet-mark.svg" "$icon_dir/duet.svg"

mkdir -p "$data/applications"
cat > "$data/applications/duet.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=Duet
Comment=The DAW you produce with, together with the Collaborator
Exec=pw-jack "$binary"
Icon=duet
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupWMClass=Duet
ENTRY

# The caches are a courtesy: desktops rescan on their own, just more slowly.
command -v update-desktop-database > /dev/null && update-desktop-database "$data/applications" || true
command -v gtk-update-icon-cache > /dev/null && gtk-update-icon-cache -q "$data/icons/hicolor" || true

echo "Installed: $data/applications/duet.desktop -> $binary"
