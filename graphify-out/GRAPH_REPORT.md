# Graph Report - tessera-Microplant  (2026-10-13)

## Corpus Check
- Corpus is ~27,810 words - fits in a single context window. You may not need a graph.

## Summary
- 514 nodes · 898 edges · 38 communities (28 shown, 10 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 84 edges (avg confidence: 0.86)
- Token cost: 123,005 input · 0 output

## Community Hubs (Navigation)
- Plant Avatar Animations
- Tile Layout Geometry
- Model Logic & Messages
- PlantTile State
- Metric Card
- Host Harness Renderer
- Avatar Poses & Mouths
- Plugin Class & Routing
- Harness Fake HA Answers
- Plant Moods
- UI Helper API
- Tessera Tooling & Rules
- Reading Cells & Bar Item
- ESPHome Text Stub
- Headers & Component Base
- Tile-to-Card Detail
- ESPHome Number Stub
- Plugin Registration API
- Store Picture
- Placed Objects
- HA Data Flow
- Taps & Watering
- ESPHome Codegen
- Settings & Stored Waterings
- Harness & Design Notes
- Release & Review Fixes
- ESPHome Switch Stub
- Opened Card Record
- Demo Mode & Settings
- Brief Deviations
- Tile Sizes & Memory
- Manifest & Moods
- Send API
- Display Flush
- Sent Message Record
- build.sh
- setup.sh

## God Nodes (most connected - your core abstractions)
1. `PlantTile` - 87 edges
2. `MetricCard` - 46 edges
3. `Microplant` - 40 edges
4. `PlantAvatar` - 39 edges
5. `Mood` - 20 edges
6. `main()` - 19 edges
7. `Status` - 17 edges
8. `Detail` - 14 edges
9. `PlantAvatar::Pose` - 13 edges
10. `Metric` - 13 edges

## Surprising Connections (you probably didn't know these)
- `Message routing by re (card then tiles via tessera::List)` --references--> `Microplant`  [INFERRED]
  docs/DESIGN.md → components/microplant/microplant.h
- `Tile layouts (full, wide, tall, small)` --references--> `PlantTile`  [EXTRACTED]
  docs/DESIGN.md → components/microplant/microplant.h
- `Tile plant (1x1-3x3, 12 options, domains sensor/plant)` --implements--> `PlantTile`  [INFERRED]
  tessera-plugin.yaml → components/microplant/microplant.h
- `Host harness (LVGL 9.5 host build rendering PNGs)` --semantically_similar_to--> `tools/preview.py official bench`  [INFERRED] [semantically similar]
  dev/host-harness/README.md → AGENTS.md
- `Sensor polling via history/history_during_period` --references--> `PlantTile`  [EXTRACTED]
  docs/DESIGN.md → components/microplant/microplant.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Home Assistant to tile/card data pipeline** — docs_design_data_flow, docs_design_history_polling, docs_design_message_routing, tessera_plugin_ha_commands, tessera_plugin_statistics_answer, plugin_microplant_poll [EXTRACTED 1.00]
- **Settings shared by ESPHome entities, manifest and screen** — plugin_microplant_demo, plugin_microplant_poll, plugin_microplant_watered, tessera_plugin_settings, plugin_microplant_component [EXTRACTED 1.00]
- **Check, bench and store picture workflow** — agents_check_py, agents_preview_py, agents_store_image_py, claude_run_bench_bat, dev_host_harness_readme_host_harness [INFERRED 0.85]
- **Plant tile composition** — store_plant_avatar, store_mood_label, store_watered_timer, store_watered_key, store_reading_cells [EXTRACTED 1.00]

## Communities (38 total, 10 thin omitted)

### Community 0 - "Plant Avatar Animations"
Cohesion: 0.06
Nodes (35): blink(), box(), dot(), fall(), lerp(), PlantAvatar, cheek_, drop_ (+27 more)

### Community 1 - "Tile Layout Geometry"
Cohesion: 0.09
Nodes (18): Box, h, w, x, y, icon, degree(), demo_value() (+10 more)

### Community 2 - "Model Logic & Messages"
Cohesion: 0.07
Nodes (30): local_offset(), celsius_of_attribute(), civil_from_days(), days_from_civil(), evaluate(), health_of(), is_high(), iso_utc() (+22 more)

### Community 3 - "PlantTile State"
Cohesion: 0.04
Nodes (43): PlantTile, asked_, attribute_, avatar_, cells_, chosen_, compact_values_, entities_ (+35 more)

### Community 4 - "Metric Card"
Cohesion: 0.07
Nodes (28): MetricCard, ago_, asked_, chart_, chart_band_, chart_line_, chart_note_, d_ (+20 more)

### Community 5 - "Host Harness Renderer"
Cohesion: 0.10
Nodes (8): clock_text(), covers(), date_text(), local_time(), next_cp(), slurp(), tap_point(), ttf()

### Community 6 - "Avatar Poses & Mouths"
Cohesion: 0.11
Nodes (18): Mouth, FLAT, FROWN, GRIN, OPEN, SMILE, WAVY, PlantAvatar::Pose (+10 more)

### Community 7 - "Plugin Class & Routing"
Cohesion: 0.11
Nodes (7): Microplant, detail_, open_card_, poll_, tiles_, watered_, WATERED_STORE_LENGTH

### Community 8 - "Harness Fake HA Answers"
Cohesion: 0.13
Nodes (16): action(), advance(), answer_polls(), attrs(), days_text(), fill(), font_path(), format() (+8 more)

### Community 9 - "Plant Moods"
Cohesion: 0.13
Nodes (14): Mood, DARK, DRY_AIR, HUMID, PARCHED, SCORCHED, SOGGY, THANKS (+6 more)

### Community 10 - "UI Helper API"
Cohesion: 0.27
Nodes (11): block(), font(), label(), set_color(), set_fill(), set_font(), set_tone(), text() (+3 more)

### Community 11 - "Tessera Tooling & Rules"
Cohesion: 0.22
Nodes (9): tools/check.py plugin checker, MicroPlant Tessera plugin, tools/preview.py official bench, tools/store_image.py (store.png), tessera-plugins repository (rules, docs, tools), run-bench.bat (Windows to WSL bench runner), translations/*.json (en, it), Bench verification (193 pictures, 0 warnings) (+1 more)

### Community 12 - "Reading Cells & Bar Item"
Cohesion: 0.18
Nodes (6): Cell, bar, box, ground, name, value

### Community 13 - "ESPHome Text Stub"
Cohesion: 0.31
Nodes (5): Text, state, TextCall, t_, v_

### Community 15 - "Tile-to-Card Detail"
Cohesion: 0.20
Nodes (10): Detail, demo, demo_watered, entity, fahrenheit, light_percent, metric, range (+2 more)

### Community 16 - "ESPHome Number Stub"
Cohesion: 0.31
Nodes (5): Number, state, NumberCall, n_, v_

### Community 17 - "Plugin Registration API"
Cohesion: 0.22
Nodes (5): place(), Plugin::add_bar_item(), Plugin::add_card(), Plugin::add_tap_action(), Plugin::add_tile()

### Community 18 - "Store Picture"
Cohesion: 0.28
Nodes (9): Mood words (Thriving, Thank you!, Too wet), Animated plant avatar (pot with face, leaves, flower), Four reading cells (Soil 48%, Temp 22.4C, Air 55%, Light 8.2k lx) with status bars, Tessera screens shown (large landscape, mid, small square, bar clock), MicroPlant store picture, Tagline: a living plant that shows soil, warmth, air and light and says when to water, Tile layouts across sizes (full, wide, tall, small), Watered key (blue button) (+1 more)

### Community 19 - "Placed Objects"
Cohesion: 0.25
Nodes (7): Placed, h, parent, tile, w, x, y

### Community 20 - "HA Data Flow"
Cohesion: 0.25
Nodes (6): Data flow (push entity, polled sensors, card statistics), Preview history_during_period answers, Preview tile Monstera (soil sensor + polled options + input_datetime), Preview statistics_during_period 24 h means, ha_commands permission (history_during_period, statistics_during_period), answers mapping statistics to means[]

### Community 21 - "Taps & Watering"
Cohesion: 0.29
Nodes (3): Text microplant_watered (Plant watered at), Set up (add plugin, entity, sensors, watering helper, ranges), home_assistant_actions (input_datetime.set_datetime, input_button.press, button.press)

### Community 24 - "Harness & Design Notes"
Cohesion: 0.29
Nodes (5): host.cpp (plugin API stand-in), Host harness (LVGL 9.5 host build rendering PNGs), setup.sh / build.sh (deps: LVGL, ArduinoJson, MDI, Tessera dev), Vector plant avatar (poses, colours, animations), Idea: show health_of() score on screen

### Community 25 - "Release & Review Fixes"
Cohesion: 0.40
Nodes (3): Release 0.1.0 (2026-10-13), model_test.cpp (tests microplant_model.h), Preview tile Ficus (Plant Monitor, too wet)

### Community 27 - "Opened Card Record"
Cohesion: 0.40
Nodes (5): Opened, card, entity, tile, title

### Community 28 - "Demo Mode & Settings"
Cohesion: 0.40
Nodes (5): Demo mode (deterministic draining soil, 10-min cycle), microplant ESPHome component config, Switch microplant_demo (Plant demo data), Number microplant_poll (Plant poll seconds, 30-300), Preview tile New plant (demo)

### Community 30 - "Tile Sizes & Memory"
Cohesion: 0.50
Nodes (3): Tile layouts (full, wide, tall, small), Not verified on a real screen yet, Tile plant (1x1-3x3, 12 options, domains sensor/plant)

### Community 31 - "Manifest & Moods"
Cohesion: 0.50
Nodes (4): Plant moods (Thriving, Thirsty, Too wet, Too hot, ...), Plugin manifest (id microplant, version 0.1.0, api 0.10), Manifest settings (plant_demo_data, plant_poll_seconds, plant_watered_at), Bar item thirsty (watering can)

### Community 34 - "Sent Message Record"
Cohesion: 0.67
Nodes (3): Sent, json, re

## Knowledge Gaps
- **187 isolated node(s):** `x`, `y`, `w`, `h`, `ground` (+182 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 253 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **10 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `PlantTile` connect `PlantTile State` to `Plant Avatar Animations`, `Tile Layout Geometry`, `Model Logic & Messages`, `Plugin Class & Routing`, `Plant Moods`, `Reading Cells & Bar Item`, `ESPHome Text Stub`, `Headers & Component Base`, `HA Data Flow`, `Tile Sizes & Memory`?**
  _High betweenness centrality (0.426) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `Microplant` (e.g. with `main()` and `Message routing by re (card then tiles via tessera::List)`) actually correct?**
  _`Microplant` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `x`, `y`, `w` to the rest of the system?**
  _187 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Plant Avatar Animations` be split into smaller, more focused modules?**
  _Cohesion score 0.05714285714285714 - nodes in this community are weakly interconnected._
- **Why does `PlantAvatar` connect `Plant Avatar Animations` to `Plant Moods`, `PlantTile State`, `Headers & Component Base`?**
  _High betweenness centrality (0.152) - this node is a cross-community bridge._
- **Should `Tile Layout Geometry` be split into smaller, more focused modules?**
  _Cohesion score 0.08665269042627533 - nodes in this community are weakly interconnected._
- **Why does `Microplant` connect `Plugin Class & Routing` to `Model Logic & Messages`, `PlantTile State`, `Metric Card`, `Harness Fake HA Answers`, `Reading Cells & Bar Item`, `ESPHome Text Stub`, `Headers & Component Base`, `Tile-to-Card Detail`, `ESPHome Number Stub`, `HA Data Flow`, `Settings & Stored Waterings`, `ESPHome Switch Stub`?**
  _High betweenness centrality (0.148) - this node is a cross-community bridge._