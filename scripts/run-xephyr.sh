#!/bin/bash

Xephyr :1 \
-screen 1600x900 \
-ac \
-noreset &
sleep 1

DISPLAY=:1 ./build/kohiko