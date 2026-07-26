#!/bin/bash
# Try kohiko in a nested window instead of a real session:
#   scripts/run-xephyr.sh [display-number]
set -e
cd "$(dirname "$0")/.."

DISPLAY_NUM=${1:-1}

Xephyr ":$DISPLAY_NUM" -screen 1600x900 -ac -noreset &
XEPHYR_PID=$!
trap 'kill "$XEPHYR_PID" 2>/dev/null' EXIT

sleep 1

DISPLAY=":$DISPLAY_NUM" ./build/kohiko
