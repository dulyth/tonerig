#!/usr/bin/env bash
# Fetches the pinned JUCE release into third_party/JUCE (skipped if present).
set -euo pipefail

JUCE_TAG="${JUCE_TAG:-8.0.15}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/third_party/JUCE"

if [ -d "$DEST/modules" ]; then
    echo "JUCE already present at $DEST"
    exit 0
fi

mkdir -p "$ROOT/third_party"
echo "Cloning JUCE $JUCE_TAG ..."
git clone --depth 1 --branch "$JUCE_TAG" https://github.com/juce-framework/JUCE.git "$DEST"
echo "Done."
