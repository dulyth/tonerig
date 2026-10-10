#!/usr/bin/env bash
# Builds and verifies ToneRig on Linux.
#
# Needs (Debian/Ubuntu):
#   sudo apt install build-essential cmake ninja-build git \
#        libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
#        libxcomposite-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$ROOT/scripts/fetch-juce.sh"

cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$ROOT/build" --parallel \
    --target ToneRig_VST3 ToneRigDspTest ToneRigVst3HostTest

echo
echo "== DSP chain tests =="
"$ROOT/build/ToneRigDspTest_artefacts/Release/ToneRigDspTest"
echo
echo "== VST3 module load tests =="
"$ROOT/build/ToneRigVst3HostTest"

echo
echo "Plugin: $ROOT/build/ToneRig_artefacts/Release/VST3/ToneRig.vst3"
echo "Install: mkdir -p ~/.vst3 && cp -r \"$ROOT/build/ToneRig_artefacts/Release/VST3/ToneRig.vst3\" ~/.vst3/"
