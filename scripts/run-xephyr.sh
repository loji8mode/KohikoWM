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

# `make` (the primary build) drops the binary at ./kohiko; the CMake
# build (scripts/build.sh) drops it at ./build/kohiko instead. Prefer
# the former (it's what `Required` in the project notes expects) and
# fall back to the latter so this script doesn't break for whichever
# build system was actually used.
if [ -x ./kohiko ]; then
    KOHIKO_BIN=./kohiko
elif [ -x ./build/kohiko ]; then
    KOHIKO_BIN=./build/kohiko
else
    echo "run-xephyr.sh: no kohiko binary found (looked for ./kohiko and ./build/kohiko)" >&2
    echo "run-xephyr.sh: build it first with 'make' or 'scripts/build.sh'" >&2
    exit 1
fi

DISPLAY=":$DISPLAY_NUM" "$KOHIKO_BIN"
