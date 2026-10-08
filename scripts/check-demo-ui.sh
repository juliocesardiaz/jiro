#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
app="$PWD/build/SonicPatch Demo.app"
"$app/Contents/MacOS/SonicPatchDemo" >build/demo-startup.log 2>&1 &
demo_pid=$!
trap 'kill "$demo_pid" 2>/dev/null || true' EXIT
sleep 5
if ! kill -0 "$demo_pid" 2>/dev/null; then
  cat build/demo-startup.log
  echo 'Native UI exited unexpectedly during startup.' >&2
  exit 1
fi
# Best-effort visual evidence on runners with an active GUI session. Capture
# permission and audio playback are deliberately not initiated by this check.
open "$app"
sleep 2
screencapture -x build/SonicPatch-Demo.png || true
echo 'Native UI remained running through the startup check.'
