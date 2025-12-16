#!/bin/bash
set -e

APPDIR="NetRadiant-Custom.AppDir"
APPNAME="NetRadiant-Custom"
BINARY="radiant.x86_64"
DESKTOP="netradiantcustom.desktop"
ICON="$BINARY"
ICON_SRC="install/bitmaps/logo.svg"
ICON_DEST="$APPDIR/$ICON.svg"


echo "[1/5] Preparing AppDir structure..."
mkdir -p "$APPDIR/usr/bin"

echo "[2/5] Copying executable and support files..."
cp -r install/. "$APPDIR/usr/bin/"

echo "[3/5] Generating desktop file..."
DESKTOP_FILE="$APPDIR/$DESKTOP"
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Name=$APPNAME
Exec=$BINARY %F
Icon=$ICON
Type=Application
Categories=Game;Graphics;
StartupWMClass=NetRadiant-Custom
EOF

install "$ICON_SRC"  "$ICON_DEST"

echo "[4/5] Linuxdeploy downloading..."
wget -c -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod +x linuxdeploy-x86_64.AppImage
wget -c -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod +x linuxdeploy-plugin-qt-x86_64.AppImage

APPIMAGETOOL=./linuxdeploy-x86_64.AppImage

echo "[5/5] Building AppImage..."
"$APPIMAGETOOL" --desktop-file $APPDIR/*.desktop --icon-file=$ICON_DEST --deploy-deps-only $APPDIR/usr/bin/modules --deploy-deps-only $APPDIR/usr/bin/plugins --appdir=$APPDIR --plugin qt --output appimage

echo "Done!"
echo "Built: ${APPNAME}-x86_64.AppImage"