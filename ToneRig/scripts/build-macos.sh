#!/usr/bin/env bash
# Builds and verifies ToneRig on macOS (Xcode command line tools + cmake needed).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$ROOT/scripts/fetch-juce.sh"

# Universal binary; drop the CMAKE_OSX_ARCHITECTURES line if you only need your own Mac.
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" "$@"
cmake --build "$ROOT/build" --parallel --target ToneRig_VST3 ToneRigDspTest

echo
echo "== DSP chain tests =="
"$ROOT/build/ToneRigDspTest_artefacts/Release/ToneRigDspTest"

echo
echo "Plugin: $ROOT/build/ToneRig_artefacts/Release/VST3/ToneRig.vst3"
echo "Install: mkdir -p ~/Library/Audio/Plug-Ins/VST3 && cp -r \"$ROOT/build/ToneRig_artefacts/Release/VST3/ToneRig.vst3\" ~/Library/Audio/Plug-Ins/VST3/"
