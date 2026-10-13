#!/bin/sh
# Once: fetch what the harness builds against into deps/ (git-ignored), and build LVGL.
set -e
cd "$(dirname "$0")" && mkdir -p deps build/lvgl
[ -d deps/lvgl ] || git clone -q --depth 1 -b v9.5.0 https://github.com/lvgl/lvgl deps/lvgl
[ -d deps/ArduinoJson ] || git clone -q --depth 1 -b v7.4.2 https://github.com/bblanchon/ArduinoJson deps/ArduinoJson
[ -d deps/mdi ] || git clone -q --depth 1 https://github.com/Templarian/MaterialDesign-Webfont deps/mdi
[ -d deps/tessera-dev ] || git clone -q --depth 1 -b dev https://github.com/MaxGramser/homeassistant_espscreen deps/tessera-dev
(cd deps/lvgl && find src -name '*.c') | while read f; do
  echo "gcc -c -O1 -DLV_CONF_INCLUDE_SIMPLE -I. -Ideps/lvgl deps/lvgl/$f -o build/lvgl/$(echo $f | tr / _).o"
done | xargs -P 8 -I{} sh -c "{}"
ar rcs build/liblvgl.a build/lvgl/*.o
echo "harness ready: sh build.sh"
