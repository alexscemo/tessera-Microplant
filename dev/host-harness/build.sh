#!/bin/sh
# Build the plugin with the host stand-in of Tessera's core, run it (PNG-able PPMs in out/), and run the model tests.
set -e
cd "$(dirname "$0")" && mkdir -p out
SD=${TESSERA_DEV:-deps/tessera-dev}/components/smart_display
mkdir -p stubs/esphome/components && ln -sfn "$(cd "$SD" && pwd)" stubs/esphome/components/smart_display
g++ -std=gnu++17 -O0 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
  -DLV_CONF_INCLUDE_SIMPLE -I. -Ideps/lvgl -Istubs -I"$SD" -Ideps/ArduinoJson/src \
  host.cpp ../../components/microplant/microplant.cpp ../../components/microplant/microplant_avatar.cpp \
  build/liblvgl.a -lm -o build/host
g++ -std=gnu++17 -Wall -Wextra -o build/model_test model_test.cpp && ./build/model_test
./build/host
python3 -c "
from PIL import Image; import glob
for f in glob.glob('out/*.ppm'): Image.open(f).save(f[:-4] + '.png')" 2>/dev/null || echo "(pip install pillow to turn out/*.ppm into PNGs)"
