#pragma once
// MicroPlant: a houseplant's soil, warmth, air and light on a Tessera screen, around a plant that shows how it feels.
//
// Where the data comes from (no network from the screen: Tessera's rule, docs/FIRMWARE_API.md "Rules"):
//  - The tile belongs to one entity, chosen in the editor: the soil moisture sensor, a `plant` entity of Home
//    Assistant's Plant Monitor, or a template sensor with the readings as attributes. Home Assistant pushes it to the
//    screen at every change.
//  - The other sensors are entity ids in the tile's options. The tile asks Home Assistant for their states through the
//    app (`history/history_during_period`, permissions.ha_commands) every "Plant poll seconds" (60 by default) while it
//    is on the glass. A sensor that reads a local HTTP/JSON API is a RESTful sensor in Home Assistant (README.md).
//  - Demo data when no entity is chosen or the setting "Plant demo data" is on.
//
// The configuration constants (poll interval, amber/red margins, demo speed) are at the top of microplant_model.h; the good
// ranges and the entities are the tile's options in tessera-plugin.yaml.
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text/text.h"
#include "esphome/components/smart_display/plugin_api.h"
#include "microplant_model.h"

namespace esphome::microplant {

class PlantTile;
class MetricCard;

// What a reading's card shows: handed from the tile that was tapped to the card it opens.
struct Detail {
  Metric metric = MOISTURE;
  char entity[64] = "";   // the sensor whose history the card asks for, "" for none (an attribute, the demo)
  Range range{0, 0};
  float value = NAN;
  bool light_percent = false;
  bool fahrenheit = false;  // the sensor's history is in °F (the card shows °C)
  bool demo = false;
  uint32_t demo_watered = 0;
  int tile = -1;            // the tile it was opened from, whose new readings it shows
};

class Microplant : public Component, public tessera::Plugin {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool settings(tessera::SettingsPage &page) override;
  void on_message(JsonObjectConst message) override;
  void on_standby(bool dark) override { standby_ = dark; }

  void set_demo_switch(switch_::Switch *s) { demo_ = s; }
  void set_poll_number(number::Number *n) { poll_ = n; }
  void set_watered_text(text::Text *t) { watered_ = t; }

  // The settings of the screen (plugin.yaml), read where they live.
  bool demo() const { return demo_ && demo_->state; }
  uint32_t poll_seconds() const;
  bool standby() const { return standby_; }
  // The screen's own "last watered" (the text entity "Plant watered at"), for a plant without a watering helper.
  uint32_t screen_watered() const;
  void store_screen_watered(uint32_t epoch);
  // The demo plant's watering, kept for the life of the screen so every demo tile tells the same story.
  uint32_t demo_watered(uint32_t now);
  void set_demo_watered(uint32_t epoch) { demo_watered_ = epoch; }

  // The tiles on the glass, to hand them their answers and to know whether a plant is thirsty (the top bar).
  void attach(PlantTile *tile) { tiles_.push_back(tile); }
  void detach(PlantTile *tile);
  // A tile read a new value: the card that is open for it shows it.
  void live(int tile, Metric metric, float value);

  Detail detail_;                    // what the next card opens with
  MetricCard *open_card_ = nullptr;  // the card that is open, to hand it its answer

 protected:
  tessera::BarItem bar() const;

  switch_::Switch *demo_{};
  number::Number *poll_{};
  text::Text *watered_{};
  tessera::List<PlantTile *> tiles_;
  uint32_t demo_watered_ = 0;
  bool standby_ = false;
};

}  // namespace esphome::microplant
