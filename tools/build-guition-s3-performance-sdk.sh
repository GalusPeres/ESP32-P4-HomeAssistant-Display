#!/usr/bin/env bash
set -eo pipefail

readonly BUILDER_COMMIT="8cabf2c3eaa169754f55f58675e224c918815eb7"
readonly PINNED_IDF_COMMIT="87912cd291d68f4319f13695718af6754879a83f"
readonly ARDUINO_COMPONENT_COMMIT="86c2c0046d4c732aa7cf6e049ac3b76a4da148b3"
readonly TINYUSB_COMMIT="2883403ed010c54c33a38c28358a4dd0c67d67c0"

test -d /arduino-esp32/.git
test -f /hometiles/guition-s3-performance-sdkconfig.defaults
test -f /hometiles/patches/arduino-esp32-3.3.7-usbmsc-initialize-lun.patch

git checkout --detach "$BUILDER_COMMIT"
git submodule update --init --recursive

cp /hometiles/guition-s3-performance-sdkconfig.defaults \
  configs/defconfig.hometiles_s3_performance

builds_tmp="$(mktemp)"
jq '(.targets[] | select(.target == "esp32s3") | .features) |=
    if index("hometiles_s3_performance") then .
    else . + ["hometiles_s3_performance"] end' \
  configs/builds.json > "$builds_tmp"
mv "$builds_tmp" configs/builds.json

export IDF_BRANCH="release/v5.5"
export IDF_COMMIT="$PINNED_IDF_COMMIT"
export AR_BRANCH="idf-release/v5.5"

if ! test -d components/arduino_tinyusb/tinyusb/.git; then
  ./tools/update-components.sh
fi
if ! git -C components/arduino_tinyusb/tinyusb cat-file -e \
    "$TINYUSB_COMMIT^{commit}"; then
  git -C components/arduino_tinyusb/tinyusb fetch origin "$TINYUSB_COMMIT"
fi
git -C components/arduino_tinyusb/tinyusb checkout --detach "$TINYUSB_COMMIT"

if ! test -d components/arduino/.git; then
  ./tools/install-arduino.sh
fi
if ! git -C components/arduino cat-file -e \
    "$ARDUINO_COMPONENT_COMMIT^{commit}"; then
  git -C components/arduino fetch origin tag 3.3.7
fi
if ! git -C components/arduino cat-file -e \
    "$ARDUINO_COMPONENT_COMMIT^{commit}"; then
  git -C components/arduino fetch origin "$ARDUINO_COMPONENT_COMMIT"
fi
git -C components/arduino checkout --detach "$ARDUINO_COMPONENT_COMMIT"

arduino_patch=/hometiles/patches/arduino-esp32-3.3.7-usbmsc-initialize-lun.patch
if git -C components/arduino apply --reverse --check "$arduino_patch" 2>/dev/null; then
  echo "Arduino USBMSC initialization patch already applied"
else
  git -C components/arduino apply --check "$arduino_patch"
  git -C components/arduino apply "$arduino_patch"
fi

source ./tools/install-esp-idf.sh
test "$(git -C "$IDF_PATH" rev-parse HEAD)" = "$PINNED_IDF_COMMIT"

./build.sh -s -c /arduino-esp32 -t esp32s3

sdk_path=/arduino-esp32/tools/esp32-arduino-libs/esp32s3
test -f "$sdk_path/sdkconfig"
grep -qx 'CONFIG_COMPILER_OPTIMIZATION_PERF=y' "$sdk_path/sdkconfig"
grep -qx 'CONFIG_SPIRAM_XIP_FROM_PSRAM=y' "$sdk_path/sdkconfig"
grep -qx 'CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y' "$sdk_path/sdkconfig"

echo "Guition ESP32-S3 performance SDK ready: $sdk_path"
