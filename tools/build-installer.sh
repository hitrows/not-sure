#!/bin/bash
#
# build-installer.sh - builds dist/NotSure-<version>.pkg from a clean checkout.
#
# Produces one double-clickable .pkg that installs, on macOS 10.15+:
#   - AU   -> /Library/Audio/Plug-Ins/Components   (system: every account sees it)
#   - VST3 -> /Library/Audio/Plug-Ins/VST3         (system)
#   - 9 .aupreset files -> ~/Library/Audio/Presets/Hitrows/Not Sure  (per installing user)
#
# The preset split is the tricky part: plugins are system-wide, but Logic only
# reads .aupreset from the *user* preset folder. A pkg installed to the system
# domain cannot write into "$HOME" directly, so the presets ride in a
# payload-free component whose postinstall copies them into the console user's
# home. That is the one part worth re-checking on a clean machine.
#
# The .pkg is UNSIGNED on purpose (beta). See the commented block at the bottom
# for the Developer ID / notarisation path once a certificate exists.
#
# Usage: tools/build-installer.sh
set -euo pipefail

VERSION="0.8.0"
CMAKE="/opt/homebrew/bin/cmake"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
DIST="$ROOT/dist"
WORK="$(mktemp -d /tmp/notsure-installer.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

echo "==> Building Release (AU + VST3)"
# Standalone stays in the project's FORMATS but is not built or packaged here.
"$CMAKE" --build "$BUILD" --config Release --target NotSure_AU NotSure_VST3

AU_SRC="$BUILD/NotSure_artefacts/Release/AU/Not Sure.component"
VST3_SRC="$BUILD/NotSure_artefacts/Release/VST3/Not Sure.vst3"
[ -d "$AU_SRC" ]   || { echo "error: AU not found at $AU_SRC"; exit 1; }
[ -d "$VST3_SRC" ] || { echo "error: VST3 not found at $VST3_SRC"; exit 1; }

echo "==> Generating presets"
# make-presets reads a ClassInfo template from the freshly built+installed AU
# (COPY_PLUGIN_AFTER_BUILD put it in ~/Library) and writes the .aupreset tree to
# the staging dir. Refresh the registrar so the just-built AU is the one found.
PRESET_STAGE="$WORK/presets"
mkdir -p "$PRESET_STAGE"
clang++ -std=c++17 -fobjc-arc -ObjC++ "$ROOT/tools/make-presets.mm" \
    -framework AudioToolbox -framework AudioUnit \
    -framework Foundation -framework CoreFoundation \
    -o "$WORK/make-presets"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1
"$WORK/make-presets" "$PRESET_STAGE"

echo "==> Building preset postinstall component"
# Payload-free component: the preset tree rides inside the scripts folder, and
# postinstall copies it into the console user's home at install time.
PRESET_SCRIPTS="$WORK/presets-scripts"
mkdir -p "$PRESET_SCRIPTS/Presets"
cp -R "$PRESET_STAGE/." "$PRESET_SCRIPTS/Presets/"
cat > "$PRESET_SCRIPTS/postinstall" <<'POSTINSTALL'
#!/bin/bash
# Runs as root. Copy the bundled presets into the *installing* user's home,
# which is where Logic looks for .aupreset - the system domain cannot.
set -e
CONSOLE_USER="$(stat -f%Su /dev/console)"
if [ -z "$CONSOLE_USER" ] || [ "$CONSOLE_USER" = "root" ]; then
    exit 0   # no logged-in user to install for; nothing to do
fi
USER_HOME="$(dscl . -read "/Users/$CONSOLE_USER" NFSHomeDirectory | awk '{print $2}')"
DEST="$USER_HOME/Library/Audio/Presets/Hitrows/Not Sure"
mkdir -p "$DEST"
cp -R "$(dirname "$0")/Presets/." "$DEST/"
chown -R "$CONSOLE_USER" "$USER_HOME/Library/Audio/Presets/Hitrows"
exit 0
POSTINSTALL
chmod +x "$PRESET_SCRIPTS/postinstall"

echo "==> Staging plugins (system payload)"
# Staged immediately before pkgbuild, then xattrs cleared: doing this right
# before packaging (rather than early) keeps AppleDouble (._*) metadata out of
# the payload. Staging early and packaging later leaked ._* into /Library.
PLUGINS_ROOT="$WORK/plugins-root"
mkdir -p "$PLUGINS_ROOT/Components" "$PLUGINS_ROOT/VST3"
cp -R "$AU_SRC"   "$PLUGINS_ROOT/Components/"
cp -R "$VST3_SRC" "$PLUGINS_ROOT/VST3/"
xattr -cr "$PLUGINS_ROOT"

echo "==> pkgbuild component packages"
PKGS="$WORK/pkgs"
mkdir -p "$PKGS"
pkgbuild --root "$PLUGINS_ROOT" \
    --install-location "/Library/Audio/Plug-Ins" \
    --identifier "com.hitrows.notsure.plugins" \
    --version "$VERSION" \
    "$PKGS/plugins.pkg"
pkgbuild --nopayload \
    --scripts "$PRESET_SCRIPTS" \
    --identifier "com.hitrows.notsure.presets" \
    --version "$VERSION" \
    "$PKGS/presets.pkg"

echo "==> productbuild"
RES="$WORK/resources"
mkdir -p "$RES"
cp "$ROOT/README-BETA.md" "$RES/readme.txt"

mkdir -p "$DIST"
productbuild --distribution "$ROOT/tools/distribution.xml" \
    --package-path "$PKGS" \
    --resources "$RES" \
    "$DIST/NotSure-$VERSION.pkg"

echo "==> Done: $DIST/NotSure-$VERSION.pkg (unsigned)"

# ---------------------------------------------------------------------------
# Signing + notarisation, once a Developer ID exists. Uncomment and fill in.
# ---------------------------------------------------------------------------
# APP_ID="Developer ID Application: Your Name (TEAMID)"
# INST_ID="Developer ID Installer: Your Name (TEAMID)"
# codesign --force --timestamp --options runtime --sign "$APP_ID" "$AU_SRC"
# codesign --force --timestamp --options runtime --sign "$APP_ID" "$VST3_SRC"
# # then rebuild the component pkgs from the signed bundles, and:
# productsign --sign "$INST_ID" "$DIST/NotSure-$VERSION.pkg" "$DIST/NotSure-$VERSION-signed.pkg"
# xcrun notarytool submit "$DIST/NotSure-$VERSION-signed.pkg" \
#     --keychain-profile "notary" --wait
# xcrun stapler staple "$DIST/NotSure-$VERSION-signed.pkg"
