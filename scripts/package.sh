#!/usr/bin/env bash
#
# Builds the distributable macOS installer for ZS-motion.
#
#   ./scripts/package.sh
#       Universal release build, then an *unsigned* .pkg. Fine for installing on
#       this machine and for testing the installer itself; Gatekeeper will warn on
#       any other Mac.
#
#   ./scripts/package.sh --sign
#       Same, but every bundle is signed with the "Developer ID Application"
#       certificate and the installer with "Developer ID Installer", both taken
#       from the login keychain. Needed before notarisation.
#
#   ./scripts/package.sh --sign --notarize <keychain-profile>
#       Also submits the installer to Apple, waits for the verdict, and staples the
#       ticket so it validates offline. Store the profile once with:
#
#           xcrun notarytool store-credentials <keychain-profile> \
#               --apple-id <your-apple-id> --team-id <your-team-id>
#
#       (notarytool prompts for an app-specific password and keeps it in the
#       keychain — this script never sees or handles the credentials.)
#
# The build goes to build-dist/ and never touches the plug-ins already installed
# in ~/Library, so packaging cannot disturb a working setup.
#
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

product="ZS-motion"
identifier="ru.zsrecords.zsmotion"
buildDir="build-dist"
outDir="dist"

version="$(sed -n 's/^project(ZSmotion VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[[ -n "$version" ]] || { echo "Could not read the version out of CMakeLists.txt" >&2; exit 1; }

wantSign=0
notaryProfile=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sign)     wantSign=1; shift ;;
        --notarize) notaryProfile="${2:-}"; shift 2
                    [[ -n "$notaryProfile" ]] || { echo "--notarize needs a keychain profile name" >&2; exit 1; } ;;
        -h|--help)  sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)          echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# Notarisation is only meaningful on a signed build.
if [[ -n "$notaryProfile" && $wantSign -eq 0 ]]; then
    echo "--notarize requires --sign (Apple will not notarise an unsigned installer)." >&2
    exit 1
fi

# ─── Certificates ───────────────────────────────────────────────────────────
appCert=""
installerCert=""

if [[ $wantSign -eq 1 ]]; then
    appCert="$(security find-identity -v -p codesigning \
               | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)"
    installerCert="$(security find-identity -v \
               | sed -n 's/.*"\(Developer ID Installer: [^"]*\)".*/\1/p' | head -1)"

    if [[ -z "$appCert" || -z "$installerCert" ]]; then
        cat >&2 <<'EOF'
Signing was requested but the certificates are not in the keychain.

You need both of these, and only you can create them — they are tied to your
Apple Developer Program membership:

  * Developer ID Application  — signs the .vst3, .component and .app
  * Developer ID Installer    — signs the .pkg

  1. Join the Apple Developer Program (developer.apple.com, paid membership).
  2. In Xcode: Settings > Accounts > Manage Certificates > + >
     "Developer ID Application", then again for "Developer ID Installer".
     (Or create them at developer.apple.com/account/resources/certificates and
     double-click the downloaded .cer files.)
  3. Check they are visible:  security find-identity -v

Then run this script again. Without them, drop --sign to build an unsigned
installer for local use.
EOF
        exit 1
    fi

    echo "Signing with:"
    echo "  $appCert"
    echo "  $installerCert"
    echo
fi

# ─── Build ──────────────────────────────────────────────────────────────────
if [[ ! -d JUCE ]]; then
    echo "JUCE is missing — fetching it..."
    git clone --depth 1 --branch master https://github.com/juce-framework/JUCE.git JUCE
fi

echo "Building $product $version (universal)..."

cmake -B "$buildDir" -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DZSMOTION_COPY_AFTER_BUILD=OFF \
      -DZSMOTION_BUILD_TESTS=OFF \
      -DZSMOTION_BUILD_SHOT=OFF > /dev/null

cmake --build "$buildDir" -j "$(sysctl -n hw.ncpu)" > /dev/null

artefacts="$buildDir/ZSmotion_artefacts/Release"

vst3="$artefacts/VST3/$product.vst3"
au="$artefacts/AU/$product.component"
app="$artefacts/Standalone/$product.app"

for bundle in "$vst3" "$au" "$app"; do
    [[ -d "$bundle" ]] || { echo "Expected build output is missing: $bundle" >&2; exit 1; }
done

# Confirm we really did get a universal binary — a silently single-arch release
# would fail on half the machines it is sent to.
for bundle in "$vst3" "$au" "$app"; do
    binary="$bundle/Contents/MacOS/$product"
    archs="$(lipo -archs "$binary")"

    for wanted in arm64 x86_64; do
        [[ "$archs" == *"$wanted"* ]] || {
            echo "$binary is missing the $wanted slice (got: $archs)" >&2; exit 1; }
    done
done

echo "  universal: arm64 + x86_64"

# ─── Sign the bundles ───────────────────────────────────────────────────────
if [[ $wantSign -eq 1 ]]; then
    echo "Signing bundles..."

    # The plug-ins inherit the host's permissions and need no entitlements; the
    # standalone records from an input device, so it asks for audio-input.
    for bundle in "$vst3" "$au"; do
        codesign --force --timestamp --options runtime \
                 --sign "$appCert" "$bundle"
    done

    codesign --force --timestamp --options runtime \
             --entitlements Resources/pkg/standalone.entitlements \
             --sign "$appCert" "$app"

    for bundle in "$vst3" "$au" "$app"; do
        codesign --verify --strict --verbose=1 "$bundle" 2>&1 | sed 's/^/  /'
    done
fi

# ─── Component packages ─────────────────────────────────────────────────────
pkgDir="$buildDir/pkg"
rm -rf "$pkgDir"
mkdir -p "$pkgDir" "$outDir"

echo "Building component packages..."

pkgbuild --quiet --component "$vst3" \
         --install-location "/Library/Audio/Plug-Ins/VST3" \
         --identifier "$identifier.vst3" --version "$version" \
         "$pkgDir/vst3.pkg"

pkgbuild --quiet --component "$au" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         --identifier "$identifier.au" --version "$version" \
         "$pkgDir/au.pkg"

pkgbuild --quiet --component "$app" \
         --install-location "/Applications" \
         --identifier "$identifier.app" --version "$version" \
         "$pkgDir/app.pkg"

# ─── Distribution ───────────────────────────────────────────────────────────
distXml="$buildDir/distribution.xml"

cat > "$distXml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$product $version</title>
    <organization>ru.zsrecords</organization>

    <welcome    file="welcome.html"    mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>

    <!-- Brand artwork, generated from the plug-in's own theme. The same dark
         image serves both appearances, so light and dark mode agree. -->
    <background          file="pkg-background.png" alignment="bottomleft" scaling="none"/>
    <background-darkAqua file="pkg-background.png" alignment="bottomleft" scaling="none"/>

    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <allowed-os-versions><os-version min="10.15"/></allowed-os-versions>
    <domains enable_localSystem="true"/>

    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>

    <choice id="vst3" title="VST3"
            description="Плагин VST3 для Live, REAPER, Cubase, Studio One и других.">
        <pkg-ref id="$identifier.vst3"/>
    </choice>

    <choice id="au" title="Audio Unit"
            description="Плагин AU для Logic Pro, GarageBand и других хостов Apple.">
        <pkg-ref id="$identifier.au"/>
    </choice>

    <choice id="app" title="Standalone"
            description="Отдельное приложение — можно работать без DAW.">
        <pkg-ref id="$identifier.app"/>
    </choice>

    <pkg-ref id="$identifier.vst3" version="$version">vst3.pkg</pkg-ref>
    <pkg-ref id="$identifier.au"   version="$version">au.pkg</pkg-ref>
    <pkg-ref id="$identifier.app"  version="$version">app.pkg</pkg-ref>
</installer-gui-script>
EOF

unsignedPkg="$buildDir/$product-$version-unsigned.pkg"

productbuild --quiet \
             --distribution "$distXml" \
             --package-path "$pkgDir" \
             --resources Resources/pkg \
             "$unsignedPkg"

# ─── Sign the installer ─────────────────────────────────────────────────────
finalPkg="$outDir/$product-$version.pkg"

if [[ $wantSign -eq 1 ]]; then
    echo "Signing the installer..."
    productsign --sign "$installerCert" "$unsignedPkg" "$finalPkg"
    pkgutil --check-signature "$finalPkg" | sed 's/^/  /'
else
    finalPkg="$outDir/$product-$version-unsigned.pkg"
    cp "$unsignedPkg" "$finalPkg"
fi

# ─── Notarise ───────────────────────────────────────────────────────────────
if [[ -n "$notaryProfile" ]]; then
    echo
    echo "Submitting to Apple for notarisation (this uploads the installer)..."

    xcrun notarytool submit "$finalPkg" \
          --keychain-profile "$notaryProfile" --wait

    echo "Stapling the ticket..."
    xcrun stapler staple "$finalPkg"
    xcrun stapler validate "$finalPkg" | sed 's/^/  /'
fi

# ─── Done ───────────────────────────────────────────────────────────────────
echo
echo "Installer:"
echo "  $finalPkg  ($(du -h "$finalPkg" | cut -f1))"
echo
echo "Contents:"
echo "  /Library/Audio/Plug-Ins/VST3/$product.vst3"
echo "  /Library/Audio/Plug-Ins/Components/$product.component"
echo "  /Applications/$product.app"

if [[ $wantSign -eq 0 ]]; then
    echo
    echo "NOTE: unsigned. Gatekeeper will refuse it on other Macs — run with"
    echo "      --sign (and --notarize) to build something distributable."
fi
