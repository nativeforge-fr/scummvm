#!/usr/bin/env bash
# Native Forge — build the Linux (aarch64) Versailles installer as a Qt AppImage.
# Bundles: the Qt installer GUI + the portable `vfimport` core.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"
IMP="$ROOT/dists/nativeforge/importer"
INST="$ROOT/dists/nativeforge/installer-linux"
APPDIR="$ROOT/AppDir-installer"
TOOLS="$ROOT/.nf-tools"; OUT="$ROOT/dist"
NPROC="$(nproc 2>/dev/null || echo 2)"
mkdir -p "$TOOLS" "$OUT"

echo "=== build vfimport core ==="
make -C "$IMP"

echo "=== build Qt installer ==="
cmake -S "$INST" -B "$INST/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$INST/build" -j"$NPROC"

echo "=== assemble AppDir ==="
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/512x512/apps"
cp "$INST/build/versailles-installer" "$APPDIR/usr/bin/"
cp "$IMP/vfimport"                    "$APPDIR/usr/bin/"
cp "$INST/versailles-installer.desktop" "$APPDIR/usr/share/applications/versailles-installer.desktop"
cp "$ROOT/dists/nativeforge/versailles.png" "$APPDIR/usr/share/icons/hicolor/512x512/apps/versailles-installer.png"
# Bundle the CJK fonts if present in the repo (optional; enables CJK import).
if [ -d "$ROOT/dists/nativeforge/fonts_cjk" ]; then
  mkdir -p "$APPDIR/usr/bin/fonts_cjk"
  cp "$ROOT/dists/nativeforge/fonts_cjk"/*.otf "$APPDIR/usr/bin/fonts_cjk/" 2>/dev/null || true
fi

echo "=== fetch AppImage tooling (aarch64) ==="
LD="$TOOLS/linuxdeploy-aarch64.AppImage"
LDQT="$TOOLS/linuxdeploy-plugin-qt-aarch64.AppImage"
AT="$TOOLS/appimagetool-aarch64.AppImage"
[ -x "$LD" ]   || { wget -qO "$LD"   https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-aarch64.AppImage; chmod +x "$LD"; }
[ -x "$LDQT" ] || { wget -qO "$LDQT" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-aarch64.AppImage; chmod +x "$LDQT"; }
[ -x "$AT" ]   || { wget -qO "$AT"   https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-aarch64.AppImage; chmod +x "$AT"; }
cp "$LDQT" "$TOOLS/linuxdeploy-plugin-qt"   # linuxdeploy discovers plugins by name in PATH
chmod +x "$TOOLS/linuxdeploy-plugin-qt"

export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=aarch64
export PATH="$TOOLS:$PATH"
export QMAKE="${QMAKE:-qmake6}"

echo "=== bundle Qt + libs (linuxdeploy + qt plugin) ==="
"$LD" --appdir "$APPDIR" \
  -e "$APPDIR/usr/bin/versailles-installer" \
  -e "$APPDIR/usr/bin/vfimport" \
  -d "$APPDIR/usr/share/applications/versailles-installer.desktop" \
  -i "$APPDIR/usr/share/icons/hicolor/512x512/apps/versailles-installer.png" \
  --plugin qt

echo "=== build AppImage ==="
IMG="$OUT/Versailles1685-Installer-aarch64.AppImage"
"$AT" "$APPDIR" "$IMG"
echo "=== DONE: $IMG ==="
ls -lh "$IMG"
