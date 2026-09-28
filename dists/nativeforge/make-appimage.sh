#!/usr/bin/env bash
# Native Forge — build the standalone Versailles 1685 engine for Linux aarch64
# and package it as a portable AppImage (target: ArmadaOS / ROCKNIX-class ARM64
# handhelds, Raspberry Pi OS, generic aarch64 desktops).
#
# Usage:  dists/nativeforge/make-appimage.sh <variant>     # variant = qol | original
#
# Produces: dist/Versailles1685-NativeForge-<variant>-aarch64.AppImage
#
# No game data is bundled. The game reads a "game_data" folder placed next to the
# .AppImage at runtime (see AppRun). ScummVM's own data is bundled via make install
# and found through the $APPDIR variable the AppImage runtime sets.
set -euo pipefail

VARIANT="${1:-qol}"
case "$VARIANT" in
	qol)      DEFS='-DVERSAILLES_STANDALONE -DNDEBUG -DVERSAILLES_QOL' ;;
	original) DEFS='-DVERSAILLES_STANDALONE -DNDEBUG' ;;
	*) echo "unknown variant '$VARIANT' (expected: qol | original)" >&2; exit 2 ;;
esac

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

APPDIR="$ROOT/AppDir-$VARIANT"
TOOLS="$ROOT/.nf-tools"
OUT="$ROOT/dist"
NPROC="$(nproc 2>/dev/null || echo 2)"
mkdir -p "$TOOLS" "$OUT"

echo "=== [$VARIANT] configure (cryomni3d/versailles only, standalone, prefix=/usr) ==="
./configure --prefix=/usr \
	--disable-all-engines --enable-engine=cryomni3d,versailles \
	--enable-release --disable-eventrecorder

# Inject the standalone (and QoL) defines the same way the Windows build does.
sed -i '/VERSAILLES_STANDALONE/d' config.mk
echo "DEFINES += $DEFS" >> config.mk

echo "=== [$VARIANT] build ==="
make -j"$NPROC"

echo "=== [$VARIANT] stage into AppDir via make install ==="
rm -rf "$APPDIR"
make install DESTDIR="$APPDIR"
# Present the game under its own name; ScummVM's data search is name-independent.
mv "$APPDIR/usr/bin/scummvm" "$APPDIR/usr/bin/versailles"

echo "=== fetch AppImage tooling (aarch64) ==="
LINUXDEPLOY="$TOOLS/linuxdeploy-aarch64.AppImage"
APPIMAGETOOL="$TOOLS/appimagetool-aarch64.AppImage"
[ -x "$LINUXDEPLOY" ]   || { wget -qO "$LINUXDEPLOY"   https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-aarch64.AppImage;   chmod +x "$LINUXDEPLOY"; }
[ -x "$APPIMAGETOOL" ]  || { wget -qO "$APPIMAGETOOL"  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-aarch64.AppImage; chmod +x "$APPIMAGETOOL"; }

export APPIMAGE_EXTRACT_AND_RUN=1   # runners have no FUSE
export ARCH=aarch64

echo "=== [$VARIANT] bundle shared libraries (linuxdeploy) ==="
"$LINUXDEPLOY" --appdir "$APPDIR" \
	-e "$APPDIR/usr/bin/versailles" \
	-d "$ROOT/dists/nativeforge/versailles.desktop" \
	-i "$ROOT/dists/nativeforge/versailles.png"

# Override linuxdeploy's generic AppRun with ours (game_data-next-to-AppImage logic).
install -m 0755 "$ROOT/dists/nativeforge/AppRun" "$APPDIR/AppRun"

echo "=== [$VARIANT] build AppImage ==="
IMG="$OUT/Versailles1685-NativeForge-$VARIANT-aarch64.AppImage"
"$APPIMAGETOOL" "$APPDIR" "$IMG"

echo "=== DONE: $IMG ==="
ls -lh "$IMG"
