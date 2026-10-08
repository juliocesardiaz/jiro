#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  echo "Build this native demo on an Apple Silicon Mac running macOS 14.4 or newer." >&2
  exit 1
fi
if ! xcrun --find swiftc >/dev/null 2>&1; then
  echo "Install Apple's developer tools with: xcode-select --install" >&2
  exit 1
fi
sdk="$(xcrun --sdk macosx --show-sdk-path)"
app="$PWD/build/SonicPatch Demo.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" build/demo-objects
xcrun --sdk macosx clang++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -target arm64-apple-macos14.4 -isysroot "$sdk" \
  -c Demo/Audio/AudioControl.cpp -o build/demo-objects/AudioControl.o
xcrun --sdk macosx swiftc -swift-version 5 -O -parse-as-library \
  -target arm64-apple-macosx14.4 -sdk "$sdk" \
  -import-objc-header Demo/Audio/AudioControl.h \
  Demo/DemoApp.swift Demo/DemoModel.swift Demo/AudioHardware.swift Demo/MixerView.swift \
  build/demo-objects/AudioControl.o -lc++ \
  -framework AppKit -framework SwiftUI -framework Combine -framework CoreAudio -framework AudioToolbox \
  -o "$app/Contents/MacOS/SonicPatchDemo"
cp Demo/Info.plist "$app/Contents/Info.plist"
codesign --force --sign - --identifier com.sonicpatch.demo "$app"
codesign --verify --strict "$app"
"$app/Contents/MacOS/SonicPatchDemo" --smoke-test
echo "Built: $app"
if [[ "${1:-}" != --no-open ]]; then
  open "$app"
fi
