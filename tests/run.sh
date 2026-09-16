#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/chirk-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
for pin in 19 47; do
  c++ -std=c++17 -fsanitize=address,undefined -g \
    -DCONFIG_IDF_TARGET_ESP32S3=1 -DARDUINO_USB_CDC_ON_BOOT=1 -DPIN_12V_EN="$pin" \
    -I"$project_dir/tests/stubs" -I"$project_dir" \
    "$project_dir/tests/diagnostics_test.cpp" "$project_dir/XUsb.cpp" \
    "$project_dir/XPower.cpp" "$project_dir/XanderByte.cpp" -o "$test_dir/test-$pin"
  "$test_dir/test-$pin"
done
c++ -std=c++17 -fsanitize=address,undefined -g \
  -I"$project_dir/tests/radio_stubs" -I"$project_dir/tests/stubs" -I"$project_dir" \
  "$project_dir/tests/radio_test.cpp" -o "$test_dir/radio"
"$test_dir/radio"
