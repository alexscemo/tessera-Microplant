@AGENTS.md
@docs/DESIGN.md
@docs/STATUS.md

# MicroPlant: working notes for Claude Code

A Tessera plugin (id `microplant`, plugin API **0.10**, maintainer `alexscemo`, MIT). It shows a houseplant on an ESP32
touch screen: an animated plant whose pose follows its health, four reading cells (soil moisture, temperature, air
humidity, light) in green/amber/red, a "Watered" timer and key, and a card per reading with its range and 24 h history.
Target: the 4-inch square 480x480 Guition (standard look, 170 dpi, 2x3 grid), but every tile size from 1x1 to 3x3.

AGENTS.md (imported above) points at the plugins repository's rules; they are **not optional**. Read
`../tessera-plugins/AGENTS.md` and its `docs/` before changing anything. The short version that bites most often:
no network from the screen, no LVGL click handlers, no timers/tasks, no colour numbers (`0x` + 6 hex digits is refused
by the checker, so icon codepoints must stay 5 hex digits like `0xF058C`), no words in code (every string is a key in
`translations/*.json`), objects made only in `create()`/`open()`.

## Where things are

| File | What |
|---|---|
| `tessera-plugin.yaml` | Manifest: tile `plant` (one option: the watering helper), card `metric` (wide), bar item `thirsty`, settings, `ha_commands`, `answers`, actions |
| `plugin.yaml` | Three ESPHome entities (the settings) and the component config |
| `components/microplant/microplant_model.h` | Pure logic, no LVGL: **config constants at the top**, metrics, ranges, statuses, moods, demo data, time parsing, value text |
| `components/microplant/microplant_shapes.h` | The kinds of plant drawn, as data: id, species words, leaf geometry (add a kind here) |
| `components/microplant/microplant_avatar.h/.cpp` | The vector plant (LVGL blocks + lines), poses per mood, animations |
| `components/microplant/microplant.h` | Plugin class, `Detail` (tile → card hand-off) |
| `components/microplant/microplant.cpp` | `PlantTile` (layouts, polling, taps, watering), `MetricCard`, plugin glue (settings rows, bar item, message routing) |
| `components/microplant/__init__.py` | ESPHome codegen: wires the switch/number/text entities into the C++ |
| `translations/en.json`, `it.json` | Every word; English complete, Italian has all screen keys and the app's core keys |
| `preview.yaml` | Data for the official bench (`tools/preview.py`) |
| `dev/host-harness/` | Our own host build: compiles the plugin against Tessera dev headers + LVGL 9.5 and renders PNGs (no ESPHome needed) |
| `run-bench.bat` | Double-click on Windows: check + bench in WSL + store picture, output to `bench-log.txt` (both git-ignored) |

## Commands

```sh
# from ../tessera-plugins
python3 tools/check.py ../tessera-Microplant                 # must print "...: ok"
python3 tools/preview.py ../tessera-Microplant --strict       # the official bench (in WSL on Windows; see STATUS.md)
python3 tools/store_image.py ../tessera-Microplant --from ../tessera-Microplant/.preview

# our harness (Linux/WSL), see dev/host-harness/README.md
sh dev/host-harness/build.sh && dev/host-harness/build/host   # renders into dev/host-harness/out/
```

## Conventions in this code

- Comments and READMEs in the plain-English style of the Tessera repos ("The tile asks Home Assistant ...").
- Tile/card members that hold text are `tessera::Text` or fixed `char[]`, never `std::string` members (PSRAM rule, 0.10).
- `ui::set_*` on every tick is fine (they only change what differs). Paint in `paint()`, read data in `read_values()`.
- LVGL animations act only on their own object (never capture `this`); LVGL deletes them with the object.
- Raise `version` in the manifest and add a `## x.y.z - date` section to `CHANGELOG.md` for every change that reaches
  screens.
