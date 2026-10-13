# Host harness

Our own stand-in for Tessera's core, to compile and *see* the plugin without ESPHome (the official bench,
`../tessera-plugins/tools/preview.py`, needs ESPHome from PyPI). It is not the bench: use both.

- `host.cpp` implements the plugin API functions (`tessera::epoch`, `send`, `action`, `open_card`, `tap_point`,
  `ui::label/block/set_tone/set_fill/covers/...`, settings rows) on LVGL 9.5, with Tessera's real `plugin_api.h`,
  `theme.h` (the colour table, light and dark), `ui_scale.h` and `roomy.h`, and the screen's fonts (Roboto, the MDI
  icons) through TinyTTF at the standard look's sizes (VALUE 38, HEADLINE 27, TITLE 18 bold, BODY_LARGE 21, BODY 16,
  ICON_SMALL 26, DIGITS 64).
- It lays out a 480x480 Guition page (2x3 grid, margin 16, gap 12, card padding 12, top at 56) and renders scenes
  into `out/`: full tile demo/thriving/thank-you, the moods at 1x1/2x1, 2x2, tall tiles, and two cards, light and
  dark. It feeds poll answers and statistics answers like the app does, taps through `tap_point`, and prints the
  actions, requests, opened cards, settings rows and the bar item.
- `model_test.cpp` tests `microplant_model.h` alone.

Linux or WSL (g++, git, python3 with Pillow):

```sh
sh setup.sh     # once: clones LVGL 9.5.0, ArduinoJson 7.4.2, the MDI font and Tessera dev into deps/, builds LVGL
sh build.sh     # builds, runs the tests and the renders
```

`TESSERA_DEV=/path/to/homeassistant_espscreen` uses a checkout of your own instead of `deps/tessera-dev`.
The ESPHome stubs in `stubs/` are minimal: switch, number and text entities with `state` and `make_call()`.
