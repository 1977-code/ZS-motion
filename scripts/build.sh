#!/usr/bin/env bash
#
# Builds ZS-motion as a universal (arm64 + x86_64) release and copies the
# results into the user plug-in folders.
#
#   ./scripts/build.sh            release build + install
#   ./scripts/build.sh --tests    also build and run the offline DSP checks
#
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [[ ! -d JUCE ]]; then
    echo "JUCE is missing — fetching it..."
    git clone --depth 1 --branch master https://github.com/juce-framework/JUCE.git JUCE
fi

want_tests=0
[[ "${1:-}" == "--tests" ]] && want_tests=1

cmake -B build -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -DZSMOTION_COPY_AFTER_BUILD=ON \
      -DZSMOTION_BUILD_TESTS=$want_tests

cmake --build build -j "$(sysctl -n hw.ncpu)"

if [[ $want_tests -eq 1 ]]; then
    echo
    ./build/ZSmotionTests_artefacts/Release/ZSmotionTests
fi

echo
echo "Built:"
for artefacts in build/ZSmotion_artefacts/Release build/ZSmotionFan_artefacts/Release; do
    find "$artefacts" -maxdepth 2 \( -name "*.vst3" -o -name "*.component" -o -name "*.app" \) \
        2>/dev/null | sed 's/^/  /'
done
echo
echo "Installed to:"
for name in ZS-motion ZS-MOTION-FAN; do
    echo "  ~/Library/Audio/Plug-Ins/VST3/$name.vst3"
    echo "  ~/Library/Audio/Plug-Ins/Components/$name.component"
done
