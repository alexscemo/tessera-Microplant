# Changelog

## 0.6.1 - 2026-10-14
- Fewer redraws: a wide tile no longer draws its "3 days ago" again every second, nor an open card its marker. Lighter
  on an ESP32-S3, whose panel and drawing share the same PSRAM.

## 0.6.0 - 2026-10-14
- A new option, **Sensor battery**: the plant sensor's battery level. At 25 % or less a small red battery shows at the
  plant's top left corner, on every tile size, until the battery is changed.

## 0.5.0 - 2026-10-14
- A wide tile (2x1, 2x2) shows the days since the last watering at its top right, with a water drop: "3 days ago".
  Nothing shows there before the first watering. The 2x2 tile's "Watered ..." line gave way to it.

## 0.4.0 - 2026-10-14
- The plant's name stands first on the top line of a wide tile (2x1, 2x2), its mood after it. It is the tile's name
  from the editor, or the plant's own name in Home Assistant while the tile keeps its default name.

## 0.3.1 - 2026-10-14
- A plant without a light sensor shows three readings instead of an empty light cell.

## 0.3.0 - 2026-10-14
- The sensor and range options are gone: choose the plant of Plant Monitor as the tile's entity, and its sensors and
  good ranges come from the plant. Other entities use ranges that suit a leafy houseplant.
- Sensors typed into the old options are no longer read.

## 0.2.0 - 2026-10-14
- A plant of Plant Monitor brings its own sensors and good ranges: no sensors or ranges to fill in.
- The Plant tile can take a whole page (2x3 on the 4-inch square).

## 0.1.0 - 2026-10-13
- First version: a plant tile that shows how your houseplant feels, from 1x1 to a page of its own.
- Soil moisture, temperature, air humidity and light, each green, amber or red against a range you set.
- Tap the plant or **Watered** when you water it; tap a reading for its range, what to do and its last 24 hours.
- Demo data until you choose a sensor, or with the setting **Plant demo data**.
- Plants without a watering helper each keep their own watering date on the screen.
- A Plant Monitor that sends temperatures in °F shows them right.
- Needs Tessera's dev firmware (plugin API 0.10) until its next release.
