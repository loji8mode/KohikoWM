#!/bin/bash
# Meant to be the last line of ~/.xinitrc:
#   exec /path/to/Kohiko/scripts/run.sh
set -e
cd "$(dirname "$0")/.."
exec ./build/kohiko
