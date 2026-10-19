# Status (2026-10-19, version 0.7.1)

## Done and verified

- **Runs on a real screen**: the Guition 4-inch square (480 x 480), where it works nicely (reported by the
  maintainer, 2026-10-19). Listed in the README under "Tried on".
- Merged into the plugin index as a Community plugin (tessera-plugins PR #7, 2026-10-16): every release reaches the
  app by itself.
- `tools/check.py` passes (`tessera-Microplant: ok`, API 0.10 checked, `store.png` present).
- **The official bench passes**: `tools/preview.py --strict` builds the plugin with ESPHome for all four screens (cyd,
  guition, waveshare43-portrait, jc8012p4a1), renders 193 pictures and ends with 0 warnings. What is left are notes
  only: sizes a grid cannot hold. Since 0.7.1 the bench's Monstera is a little thirsty, so the `thirsty` bar item shows.
- `store.png` made by `tools/store_image.py` from that run.
- C++ also compiles warning-free in `dev/host-harness` (cloud session) against Tessera **dev**'s headers, LVGL 9.5.0
  and ArduinoJson 7.4.2; `model_test.cpp` passes there. Its `deps/` are not on the Windows PC, so it cannot run there
  as is (`setup.sh` first).

### What the first bench runs found and how it was fixed

- **It did not compile under ESPHome**: `esphome/core/helpers.h` defines `HOT` as a macro, so `Mood::HOT` broke the
  enum and everything after it. The moods are now `TOO_HOT` and `TOO_COLD`, and the harness stub defines `HOT`,
  `ALWAYS_INLINE` and `PACKED` so such a clash fails there too. (The stale `plant_model.h` in the WSL bench build was
  a red herring.)
- **Values over another part**: a cell's icon, value, name and bar were siblings drawn over its ground block; they are
  now children of the ground.
- **Words too faint**: green and amber words and icons on light grounds. Only red (ALERT) colours words now; green and
  amber stay on the blocks (the cell bars, the card's ruler and chart band).
- **Card outside its area on the CYD**: the card drops the "Good between ..." line, then the "24 h ago / Now" foot,
  when the chart would not keep 40 px.
- **Values cut with dots** in the narrow 1x1 cells of the portrait screen (seen in the pictures, not a finding): those
  cells use short texts ("22" with a degree sign, "8.2k").
- `preview.yaml` now has 24 h statistics for every sensor, not only the soil.
- Review fixes: the screen's own waterings are kept per tile ("3=1791703800,5=...", "*" for every plant), so watering
  one plant without a helper no longer waters the others; a Plant Monitor temperature above 45 is read as °F and
  converted. Both in `microplant_model.h`, tested in `model_test.cpp` (it builds alone with g++, no deps needed).

## Not verified yet

- Tried on glass only on the Guition. Not checked one by one yet: memory/flash on the CYD, the other boards, the HA
  websocket answers' real shape (`history_during_period` compressed rows, statistics ms timestamps), °F sensors, the
  Italian texts on glass.
- **Report upstream**: the firmware drops a plugin tile's entity of a domain it has no tile for (`plant`), see
  DESIGN.md "Plant Monitor". MicroPlant guesses the id from the plant's name meanwhile; a renamed entity id breaks it.
- **Suggest upstream, after measuring** (`rendered_frames` on a Guition): for the ESP32-S3 RGB boards only,
  `CONFIG_COMPILER_OPTIMIZATION_PERF` (-O2) in `packages/hardware/esp32s3-rgb.yaml` (bigger image, more PSRAM traffic);
  LVGL's S3 SIMD blending (`LV_USE_DRAW_SW_ASM`, needs esp_lvgl_port's asm, ESPHome lacks it);
  `CONFIG_ESP32S3_DATA_CACHE_64KB` (costs 32 KB of internal RAM the Wi-Fi needs). The rest is already tuned.
- Plant Monitor's `plant/get_info` answer is taken from its source code (Olen/homeassistant-plant), not from a real
  Home Assistant yet.
- Starting `tools/preview.py` from Windows loses the backslashes of the path when it hands over to WSL: a bug in the
  plugins repository, worth reporting upstream. Run it inside WSL (`run-bench.bat` does).

## Next steps

1. Try it on another board (a CYD first, for memory) and add it to the README's "Tried on" line.
2. Release 0.7.1 for the calmer store picture.

## Ideas not done

- Show `health_of()` somewhere (a ring or a small score) — the brief mentioned a health score.
- The per-tile waterings follow the tile index: moving tiles around in the editor can swap two plants' dates.
- `wait: wrong_entity` is treated as "no entity" (demo); could say so.
- A tap action so Tessera's own sensor tiles can open the metric card.
