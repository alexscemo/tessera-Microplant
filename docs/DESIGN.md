# Design decisions and architecture

## The original brief, and where it had to bend

The brief (claude.ai project "MircoPlant") asked for: four metrics with thresholds and a last-watered counter; an
animated vector plant whose state follows a health score; four status-coloured tiles; tap plant / "Watered" to reset
the timer with a happy animation; tap a tile for a detail modal; an async HTTP GET/JSON poll every 30–60 s of a local
API or Home Assistant `/api/states/...`; a mock data mode; config variables at the top.

Tessera's rules forced these changes (all deliberate; do not "fix" them back):

| Brief | What we did | Why |
|---|---|---|
| HTTP GET of a local API / HA REST from the screen | The tile's entity is **pushed** by Tessera; the other sensors are **polled through the app** with `tessera::send` → websocket `history/history_during_period` every *Plant poll seconds* (default 60, 30–300). A local JSON API becomes a HA `rest:` sensor (README has the YAML) | The screen may never open a connection (`http_request` is refused by `check.py`), and an app `fetch` reaches only public hosts named in `permissions.network`, never a LAN address |
| Config variables for URLs/entities at the top of the C++ | The plant's sensors and ranges come from **Plant Monitor** (0.3.0 removed the 3 sensor and 8 range options: too error-prone to type; the user chose this); the watering helper is the one **tile option**; poll interval and demo are **ESPHome entities** (settings); fixed constants are in `namespace config` at the top of `microplant_model.h` | Tessera: values a person changes are options or settings, never globals in code |
| Detail modal with buttons | A read-only **wide card** (`metric`): value, ruler, hint, 24 h chart | Cards get no taps in the plugin API; the Watered key therefore lives on the tile |
| Distinct tap targets | `tessera::tap_point()` hit-testing in `PlantTile::on_tap()` | No LVGL click handlers allowed; `tap_point` is API 0.10 |
| Green/amber/red | `Tone::GOOD/BUSY/ALERT` with `ui::set_fill` / `ui::set_tone` | Theme roles only; GOOD and set_fill are API 0.9 |

**Consequence:** `api: "0.10"`, which builds only on Tessera's **dev** firmware until its next release. The two other
plugins of this user (`tessera-film-stasera`, `tessera-claude-usage`) stay on 0.8 for that reason. Going to 0.8 would
mean: no `tap_point` (one tap action for the whole tile), no GOOD tone (green via `theme::MARK_GREEN`, no red block),
no `tessera::Text/List`.

## Data flow

```
Home Assistant ──push (entity changes)──▶ app ──▶ PlantTile::on_state   (tile entity: state + attributes)
PlantTile::on_tick ──every poll_seconds──▶ tessera::send(plant/get_info) and/or (history/history_during_period, [helper])
                 app ──▶ Microplant::on_message ──▶ PlantTile::answer (matches "re")   (Plant Monitor, watering helper)
MetricCard::open ──▶ tessera::send(recorder/statistics_during_period, 24 h, hourly mean)
                 app maps it via manifest `answers` → {"means":[...]} ──▶ MetricCard::answer
```

- **Plant Monitor** (`plant.*` tile entity): every poll also sends `plant/get_info` (Olen's integration), whose answer
  `{"result": {"moisture": {"current", "min", "max", "sensor", "unit_of_measurement"}, "temperature", "illuminance",
  "humidity", ...}}` gives the value, the range (else `DEFAULT_RANGES`) and the sensor (for the card's history).
  Tessera's firmware (main and dev, 2026-10-14) drops a plugin tile's entity whose domain has no Tessera tile
  (`valid_entity` in `page_receiver.cpp`), so `context.entity` is "" for a `plant.*`. Until that is fixed upstream the
  tile spots a Plant Monitor plant by its `species` attribute and guesses `plant.<slug of its name>` (`plant_entity_of`).
- **Sources per reading** (`PlantTile::read_values`): demo → Plant Monitor → attribute of the tile entity
  (`moisture`, `temperature`, `humidity`, `brightness` or `illuminance`) → for soil, the entity's own state.
- **Tile entity domains:** `sensor` (a soil sensor, or a template/REST sensor with attributes) or `plant` (HA Plant
  Monitor; its state is ok/problem, so the state is never read as moisture).
- **Poll request** (≤512 bytes): `start_time` = now − 60 s, `include_start_time_state`, `minimal_response`,
  `no_attributes: false` (we need `unit_of_measurement` for °F→°C and lux vs %, and `timestamp` of an input_datetime).
  The last row's `s` is the value; the first row's `a` holds the attributes.
- **Polling runs only** while the tile is on the glass, the screen is awake, and not in demo. A lost answer is given up
  after one interval + `RETRY_SECONDS`; failures retry sooner. "Offline" shows after `OFFLINE_AFTER_SECONDS` without an
  answer (last readings kept).
- **Routing:** the plugin keeps a `tessera::List<PlantTile*>` (tiles attach in their constructor, detach in the
  destructor) and one `open_card_`. `on_message` offers the answer to the card, then to each tile, by `re`.
- **Card live values:** each tile calls `plugin->live(tile, metric, value)` on every read; the open card updates if it
  was opened from that tile for that metric.

## Health, statuses and moods (`microplant_model.h`)

- `evaluate()`: inside [low, high] GOOD; outside by less than the margin AMBER (`*_WARN`); further RED (`*_BAD`).
  Margins: soil and air ±10 points, temperature ±3 °C, light ×/÷ 2.
- `mood_of()`: a watering in the last `THANKS_SECONDS` → THANKS. No readings → WAITING. Otherwise the **worst level
  wins**, ties broken by order soil → temperature → light → humidity:
  soil low THIRSTY (red: PARCHED), soil high SOGGY, HOT/COLD, DARK/SCORCHED, DRY_AIR/HUMID; all good THRIVING.
- `health_of()` (100 − 15 per amber − 35 per red) exists but is not shown on screen yet.

## The plant (`microplant_avatar.*`)

- Built in a square of `size` px in unit coordinates (`at(0.5f)` = middle). Draw order: foliage container (stem line,
  4 leaves × 3 lines each: thin stalk, wide body, narrow tip; flower = two circles), then pot body + rim (MARK_AMBER),
  face (eyes, cheeks, mouth polyline in `QR_DARK`, dark in both looks), extras (sweat/puddle drop, falling drop,
  4 sparkles, a note label "z"/"!").
- Colours: Tessera mark roles (same in light and dark) and `lv_color_mix` of roles; upper leaves mix with `CARD` so
  `on_theme()` re-applies the pose.
- `Pose` per mood: droop (0 up … 1 hanging; stem bends), tuck (cold), eye openness, mouth shape
  (smile/grin/flat/frown/open/wavy), flower, cheeks, sweat, note, leaf tint toward amber/blue.
- Animations (`lv_anim`, short, never infinite): blink every ~7 s, sway every ~11 s, shiver every 4 s when cold, and
  `celebrate()` = three jumps + falling drop + staggered sparkles. Exec callbacks touch only `a->var`. Quiet in standby.
- Leaf geometry was iterated with renders; the current angles (lower 16° up → −44° drooped, upper 38° → −6°) keep the
  drooped leaves apart. Revisit there if the wilt looks muddy.

## Layouts (`PlantTile::create` picks by size, thresholds in `ui::px`)

| Mode | When | Content |
|---|---|---|
| full | w ≥ 300 px & h ≥ 240 px (2x2, 2x3) | plant left, name/mood/watered/note right, **Watered key**, 4 cells in a row under |
| wide | w ≥ 300, low (2x1) | plant left, name then mood on one line (`head_`; `plant_name()`: the editor's name unless it is the default `tile_default`, else the entity's friendly name) + note + "N days ago" with a drop at the right (`place_head()`), cells 4 in a row (or 2x2 when ≥ 104 px) |
| tall | narrow, h ≥ 200 (1x2, 1x3) | plant top, mood, watered, cells 2x2 |
| small | 1x1 | plant, mood, worst reading, watered |

Without a light reading (`has_light()`: Plant Monitor answered without one, or no light attribute) the light cell is hidden and `place_cells()` spreads the other three, the last row's cells sharing its width. Cells adapt: ground block + bar when ≥ 40 px high; side-by-side icon/value/name when ≥ 120 px wide, stacked otherwise;
one value font for all four (largest where `"88.8°C"` fits).

## Taps

`tap_point()` → Watered key or plant (with margin) = **water**; a cell = open its card; elsewhere or unknown point =
card of the worst reading.

**Watering** (`PlantTile::water`): demo → resets the demo clock (`plugin->set_demo_watered`). Otherwise optimistic local
moment, plus: option `watered_entity` by domain → `input_datetime.set_datetime` (timestamp) / `input_button.press` /
`button.press`; with no helper and no `watered_at` attribute → the screen's text entity *Plant watered at*. The shown
moment is the max of local, polled helper, attribute and (fallback) screen entity.

## The card (`MetricCard`)

Value in `Font::DIGITS` where covered (else VALUE), status word in its tone + a hint key per metric and direction,
a 5-zone ruler (red/amber/green/amber/red) with a marker, range edge labels, "Good between …", and a 24-point line chart
over a green band of the range. Light uses a **log scale** for ruler and chart. Demo history = one demo cycle.
No history for attributes (no statistic id) or sensors without `state_class`.

## Settings (`plugin.yaml` ↔ `settings` keys)

| Entity name | Key | Kind |
|---|---|---|
| Plant demo data | `plant_demo_data` | switch |
| Plant poll seconds | `plant_poll_seconds` | number 30–300 step 30 |
| Plant watered at | `plant_watered_at` | text (epoch seconds) |

The screen's settings page adds: Demo data toggle, Ask every, and a **Watered now** action showing the stored moment.

## Demo mode

On when the switch is on or the tile has no entity. Soil starts at 76 % and drains a point every 12 s; temperature,
humidity and light follow a 10-minute cycle. The first demo watering is "3 days 4 hours ago" (parched), so the first
tap shows the thank-you. Deterministic from the epoch, shared by all demo tiles of a screen.

## Memory

Manifest `memory: 6800` per tile (≈25 labels/blocks of text and cells, ≈22 plant objects incl. 13 lines, members).
`flash_kb: 26` is an estimate; measure on a CYD build.
