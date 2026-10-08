#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/demo-tests
compiler="${CXX:-c++}"
flags=(-std=c++17 -Wall -Wextra -Werror -g)
if [[ "${SANITIZE:-0}" == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
links=()
if [[ "$(uname -s)" == Darwin ]]; then
  links+=(-framework CoreAudio)
fi
"$compiler" "${flags[@]}" Demo/Audio/AudioControl.cpp Demo/Tests/AudioControlTests.cpp \
  "${links[@]}" -o build/demo-tests/audio-tests
build/demo-tests/audio-tests
