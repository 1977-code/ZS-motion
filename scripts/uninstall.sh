#!/usr/bin/env bash
#
# Removes the installed ZS-motion plug-ins from the user plug-in folders.
#
set -euo pipefail

rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/ZS-motion.vst3"
rm -rf "$HOME/Library/Audio/Plug-Ins/Components/ZS-motion.component"

echo "Removed:"
echo "  ~/Library/Audio/Plug-Ins/VST3/ZS-motion.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/ZS-motion.component"
