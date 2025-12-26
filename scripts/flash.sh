#!/bin/bash

# ESP-IDF Flash Script

set -e

export IDF_PATH="$HOME/tools/esp-idf"

export PATH="/opt/homebrew/bin:$PATH"

. "$IDF_PATH/export.sh"

PORT="${1:-/dev/cu.usbserial-0001}"

idf.py -p "$PORT" flash

idf.py -p "$PORT" monitor