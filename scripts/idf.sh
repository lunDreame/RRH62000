#!/bin/bash

# ESP-IDF Environment Script

set -e

export IDF_PATH="$HOME/tools/esp-idf"
export ESP_MATTER_PATH="$HOME/tools/esp-matter"

export PATH="/opt/homebrew/bin:$PATH"

. "$IDF_PATH/export.sh"
. "$ESP_MATTER_PATH/export.sh"

cd "$(dirname "$0")/.."