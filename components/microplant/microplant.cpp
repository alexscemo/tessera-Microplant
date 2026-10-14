#include "microplant.h"
#include "microplant_avatar.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace esphome::microplant {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---- Words and signs of the four readings ----

// Icons of Tessera's set (screen_manager/app/tile_icons.py): water, thermometer, water-percent, weather-sunny.
static constexpr uint32_t ICONS[METRIC_COUNT] = {0xF058C, 0xF050F, 0xF058E, 0xF0599};
static constexpr uint32_t ICON_WATER = 0xF058C;
// Keys of translations/<language>.json, part "screen".
static const char *const NAME_KEYS[METRIC_COUNT] = {"soil", "temperature", "humidity", "light"};
static const char *const TITLE_KEYS[METRIC_COUNT] = {"soil_title", "temperature_title", "humidity_title", "light_title"};
static const char *const LOW_HINTS[METRIC_COUNT] = {"hint_soil_low", "hint_temperature_low", "hint_humidity_low",
                                                    "hint_light_low"};
static const char *const HIGH_HINTS[METRIC_COUNT] = {"hint_soil_high", "hint_temperature_high", "hint_humidity_high",
                                                     "hint_light_high"};

static const char *mood_key(Mood mood) {
  switch (mood) {
    case Mood::THRIVING: return "mood_thriving";
    case Mood::THANKS: return "mood_thanks";
    case Mood::THIRSTY: return "mood_thirsty";
    case Mood::PARCHED: return "mood_parched";
    case Mood::SOGGY: return "mood_soggy";
    case Mood::TOO_HOT: return "mood_hot";
    case Mood::TOO_COLD: return "mood_cold";
    case Mood::DARK: return "mood_dark";
    case Mood::SCORCHED: return "mood_scorched";
    case Mood::DRY_AIR: return "mood_dry_air";
    case Mood::HUMID: return "mood_humid";
    default: return "mood_waiting";
  }
}

static const char *status_key(Status s) {
  switch (s) {
    case Status::GOOD: return "good";
    case Status::LOW_WARN: return "low";
    case Status::LOW_BAD: return "too_low";
    case Status::HIGH_WARN: return "high";
    case Status::HIGH_BAD: return "too_high";
    default: return "no_reading";
  }
}

// Green, amber and red, in both looks (plugin API 0.9).
static Tone tone_of(Status s) {
  switch (level(s)) {
    case 2: return Tone::ALERT;
    case 1: return Tone::BUSY;
    default: return s == Status::GOOD ? Tone::GOOD : Tone::NORMAL;
  }
}

// The colour of words and icons: only red is dark enough to read on every ground, in both looks. Green and amber stay
// on the blocks (bars, the ruler) and the words keep their own role.
static Tone word_tone(Status s) { return level(s) == 2 ? Tone::ALERT : Tone::NORMAL; }

// The degree sign when the font has it, else nothing ("21.5C").
static const char *degree(Font font) { return ui::covers("\xC2\xB0", font) ? "\xC2\xB0" : ""; }

// Local time minus UTC, in seconds, at `now`: for a moment Home Assistant writes without a zone.
static int32_t local_offset(uint32_t now) {
  if (!now) return 0;
  const tessera::LocalTime t = tessera::local_time(now);
  const int64_t local = static_cast<int64_t>(days_from_civil(t.year, static_cast<unsigned>(t.month),
                                                             static_cast<unsigned>(t.day))) * 86400 +
                        t.hour * 3600 + t.minute * 60 + t.second;
  return static_cast<int32_t>(local - static_cast<int64_t>(now));
}

// A number of an attribute or a state, as Home Assistant sends it: a number, or a text of one.
static float number_of(JsonVariantConst v) {
  if (v.isNull()) return NAN;
  if (v.is<const char *>()) return parse_number(v.as<const char *>());
  if (v.is<float>()) return v.as<float>();
  return NAN;
}

static bool starts_with(const char *text, const char *prefix) { return strncmp(text, prefix, strlen(prefix)) == 0; }

// A place on the tile, to lay out and to tell where a finger was.
struct Box {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py, int margin = 0) const {
    return w > 0 && px >= x - margin && px < x + w + margin && py >= y - margin && py < y + h + margin;
  }
};

static lv_obj_t *place_label(lv_obj_t *parent, Font font, theme::Role role, int x, int y, int w,
                             lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t *l = ui::label(parent, font, role);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_size(l, std::max(1, w), ui::line_height(font));
  lv_obj_set_style_text_align(l, align, 0);
  return l;
}

// =====================================================================================================================
//  The tile: the plant and its four readings
// =====================================================================================================================

class PlantTile : public tessera::Tile {
 public:
  explicit PlantTile(Microplant *plugin) : plugin_(plugin) { plugin_->attach(this); }
  ~PlantTile() override { plugin_->detach(this); }

  void create(const tessera::TileContext &c) override;
  void on_state(JsonObjectConst data) override;
  void on_tick(uint32_t epoch) override;
  void on_theme() override;
  void on_tap() override;

  // An answer of the app: true when it was this tile's.
  bool answer(JsonObjectConst message);
  bool thirsty() const { return needs_water(mood_); }
  bool parched() const { return mood_ == Mood::PARCHED; }
  const char *name() const { return name_text_.c_str(); }

 private:
  // The four ways a reading's cell is drawn, by the room it has.
  struct Cell {
    lv_obj_t *ground{}, *icon{}, *value{}, *name{}, *bar{};
    Box box;
  };

  void layout_full();
  void layout_wide();
  void layout_tall();
  void layout_small();
  void make_cells(const Box &area, int columns);
  void make_cell(int metric, bool ground);
  void place_cells();
  void place_cell(int metric, const Box &b);
  bool has_light() const;
  void make_water_key(const Box &b);
  bool demo() const { return plugin_->demo() || !chosen_; }
  bool polls() const;
  void ask(uint32_t now);
  void read_info(JsonObjectConst info);
  void read_values(uint32_t now);
  void paint(uint32_t now);
  uint32_t watered(uint32_t now) const;
  std::string watered_text(uint32_t now) const;
  void water(uint32_t now);
  void open_detail(int metric);
  int worst_metric() const;
  std::string plant_name() const;

  Microplant *plugin_;
  lv_obj_t *parent_{};
  int width_ = 0, height_ = 0, tile_ = -1;
  tessera::Text entity_, name_text_, entity_name_;
  char watered_entity_[64] = "";
  Range ranges_[METRIC_COUNT]{DEFAULT_RANGES[0], DEFAULT_RANGES[1], DEFAULT_RANGES[2], DEFAULT_RANGES[3]};
  bool plant_domain_ = false;
  // Plant Monitor (plant/get_info): the sensor of each reading, its value and its range from the plant's own settings.
  char info_entities_[METRIC_COUNT][64]{};
  float info_[METRIC_COUNT]{NAN, NAN, NAN, NAN};
  bool info_known_ = false;  // Plant Monitor answered once

  // What the tile knows.
  bool chosen_ = false;                 // an entity is chosen and Home Assistant sent it
  float state_value_ = NAN;             // the tile entity's state as a number
  float attribute_[METRIC_COUNT]{NAN, NAN, NAN, NAN};
  bool light_percent_ = false, fahrenheit_ = false;
  uint32_t watered_attribute_ = 0, watered_polled_ = 0, watered_here_ = 0, thanks_until_ = 0;
  uint32_t asked_ = 0, info_asked_ = 0, next_ask_ = 0, last_answer_ = 0, first_ask_ = 0;
  uint8_t failures_ = 0;

  // What it shows.
  float value_[METRIC_COUNT]{NAN, NAN, NAN, NAN};
  Status status_[METRIC_COUNT]{};
  Mood mood_ = Mood::WAITING;

  PlantAvatar avatar_;
  lv_obj_t *name_{}, *mood_label_{}, *watered_label_{}, *note_{}, *worst_{};
  lv_obj_t *key_{}, *key_icon_{}, *key_label_{};
  Box key_box_;
  Box head_;  // a wide tile's top line: the plant's name, then its mood
  Cell cells_[METRIC_COUNT];
  Box cells_area_;
  int cells_columns_ = 0;
  bool light_shown_ = true;
  Font value_font_ = Font::TITLE;
  bool compact_values_ = false;
  bool roomy_note_ = false;  // the note has a line of its own (a page-size tile): it may say more
};

void PlantTile::create(const tessera::TileContext &c) {
  parent_ = c.parent;
  width_ = c.width;
  height_ = c.height;
  tile_ = c.tile;
  entity_ = c.entity ? c.entity : "";
  name_text_ = c.name && *c.name ? c.name : "";
  plant_domain_ = starts_with(entity_.c_str(), "plant.");
  snprintf(watered_entity_, sizeof(watered_entity_), "%s", c.options["watered_entity"] | "");

  // The shape of the tile picks its layout: a page of its own, wide, tall, or one cell.
  const bool wide = width_ >= ui::px(300), high = height_ >= ui::px(240);
  if (wide && high) layout_full();
  else if (wide) layout_wide();
  else if (height_ >= ui::px(200)) layout_tall();
  else layout_small();
  on_theme();
}

// A page of its own (2x3 on the 4-inch square): the plant, its name, mood and watering with a key, the cells under it.
void PlantTile::layout_full() {
  const int gap = ui::px(10);
  const int cells_h = std::clamp(height_ * 34 / 100, ui::px(88), ui::px(132));
  const int top_h = height_ - cells_h - gap;
  const int a = std::min(top_h, width_ * 46 / 100);
  avatar_.create(parent_, 0, (top_h - a) / 2, a);
  const int x = a + gap, tw = width_ - x;
  // The watering key at the foot of the column, the words above it.
  const int kh = std::max(ui::mm(9), ui::line_height(Font::BODY_LARGE) + ui::px(18));
  make_water_key({x, top_h - kh, tw, kh});
  int y = std::max(0, ui::px(2));
  name_ = place_label(parent_, Font::TITLE, theme::MUTED, x, y, tw);
  y += ui::line_height(Font::TITLE) + ui::px(2);
  const Font mood_font = ui::line_height(Font::HEADLINE) * 3 + y < top_h - kh ? Font::HEADLINE : Font::TITLE;
  mood_label_ = place_label(parent_, mood_font, theme::INK, x, y, tw);
  y += ui::line_height(mood_font) + ui::px(4);
  watered_label_ = place_label(parent_, Font::BODY, theme::MUTED, x, y, tw);
  y += ui::line_height(Font::BODY) + ui::px(4);
  if (y + ui::line_height(Font::BODY) <= top_h - kh - ui::px(4)) {
    note_ = place_label(parent_, Font::BODY, theme::MUTED, x, y, tw);
    roomy_note_ = true;
  }
  make_cells({0, height_ - cells_h, width_, cells_h}, 4);
}

// Wide and low (2x1, 2x2): the plant on the left, its mood above the cells on the right. The plant waters.
void PlantTile::layout_wide() {
  const int gap = ui::px(8);
  const int a = std::min(height_, width_ * 32 / 100);
  avatar_.create(parent_, 0, (height_ - a) / 2, a);
  const int x = a + gap, tw = width_ - x;
  const int note_w = std::max(ui::text_width(plugin_->text("demo"), Font::BODY),
                              ui::text_width(plugin_->text("offline"), Font::BODY)) + ui::px(4);
  head_ = {x, 0, tw - note_w - ui::px(6), ui::line_height(Font::TITLE)};
  name_ = place_label(parent_, Font::TITLE, theme::INK, x, 0, head_.w);
  mood_label_ = place_label(parent_, Font::TITLE, theme::MUTED, x, 0, head_.w);
  note_ = place_label(parent_, Font::BODY, theme::MUTED, width_ - note_w,
                      (ui::line_height(Font::TITLE) - ui::line_height(Font::BODY)) / 2, note_w, LV_TEXT_ALIGN_RIGHT);
  int y = ui::line_height(Font::TITLE);
  if (height_ >= ui::px(150)) {
    watered_label_ = place_label(parent_, Font::BODY, theme::MUTED, x, y, tw);
    y += ui::line_height(Font::BODY);
  }
  y += ui::px(6);
  const int area_h = height_ - y;
  // Two rows of two where they fit, else one row of four.
  make_cells({x, y, tw, area_h}, area_h >= ui::px(104) ? 2 : 4);
}

// Narrow and tall (1x2, 1x3): the plant on top, its mood and watering under it, the cells two by two.
void PlantTile::layout_tall() {
  const int a = std::min(width_, height_ * 44 / 100);
  avatar_.create(parent_, (width_ - a) / 2, 0, a);
  int y = a + ui::px(2);
  mood_label_ = place_label(parent_, Font::TITLE, theme::INK, 0, y, width_, LV_TEXT_ALIGN_CENTER);
  y += ui::line_height(Font::TITLE);
  watered_label_ = place_label(parent_, Font::BODY, theme::MUTED, 0, y, width_, LV_TEXT_ALIGN_CENTER);
  y += ui::line_height(Font::BODY) + ui::px(6);
  if (height_ - y >= ui::px(60)) make_cells({0, y, width_, height_ - y}, 2);
}

// One cell (1x1): the plant, its mood, and the reading that matters most.
void PlantTile::layout_small() {
  const int a = std::min(height_, width_ * 48 / 100);
  avatar_.create(parent_, 0, (height_ - a) / 2, a);
  const int x = a + ui::px(6), tw = width_ - x;
  const int lines = ui::line_height(Font::TITLE) + ui::line_height(Font::BODY) * 2;
  int y = std::max(0, (height_ - lines) / 2);
  mood_label_ = place_label(parent_, Font::TITLE, theme::INK, x, y, tw);
  y += ui::line_height(Font::TITLE);
  worst_ = place_label(parent_, Font::BODY, theme::INK_SOFT, x, y, tw);
  y += ui::line_height(Font::BODY);
  if (y + ui::line_height(Font::BODY) <= height_)
    watered_label_ = place_label(parent_, Font::BODY, theme::MUTED, x, y, tw);
}

void PlantTile::make_water_key(const Box &b) {
  const int label_w = ui::text_width(plugin_->text("water_key"), Font::BODY_LARGE);
  const int icon_w = ui::line_height(Font::ICON_SMALL);
  key_box_ = b;
  key_box_.w = std::min(b.w, std::max(ui::px(132), label_w + icon_w + ui::px(44)));
  key_ = ui::block(parent_, theme::ACCENT);
  lv_obj_remove_flag(key_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(key_, key_box_.x, key_box_.y);
  lv_obj_set_size(key_, key_box_.w, key_box_.h);
  lv_obj_set_style_radius(key_, LV_RADIUS_CIRCLE, 0);
  const int inner = icon_w + ui::px(6) + label_w;
  const int ix = std::max(ui::px(8), (key_box_.w - inner) / 2);
  key_icon_ = place_label(key_, Font::ICON_SMALL, theme::ON_ACCENT, ix, (key_box_.h - icon_w) / 2, icon_w);
  ui::set_text(key_icon_, ui::icon(ICON_WATER));
  const int lx = ix + icon_w + ui::px(6);
  key_label_ = place_label(key_, Font::BODY_LARGE, theme::ON_ACCENT, lx,
                           (key_box_.h - ui::line_height(Font::BODY_LARGE)) / 2, key_box_.w - lx - ui::px(8));
  ui::set_text(key_label_, plugin_->text("water_key"));
}

void PlantTile::make_cells(const Box &area, int columns) {
  const int gap = ui::px(8);
  const int rows = METRIC_COUNT / columns;
  const int cw = (area.w - gap * (columns - 1)) / columns;
  const int ch = (area.h - gap * (rows - 1)) / rows;
  cells_area_ = area;
  cells_columns_ = columns;
  // One font for every value, the largest in which the widest of them fits.
  value_font_ = Font::BODY;
  const int room = cw >= ui::px(120) ? cw - ui::line_height(Font::ICON_SMALL) - ui::px(18) : cw - ui::px(10);
  for (Font f : {Font::HEADLINE, Font::TITLE, Font::BODY_LARGE}) {
    if (ui::text_width("88.8\xC2\xB0" "C", f) <= room && ui::line_height(f) * 2 <= ch) {
      value_font_ = f;
      break;
    }
  }
  // Too narrow even for the smallest font: the short texts (no decimal of a temperature, no "lx").
  compact_values_ = value_font_ == Font::BODY && (ui::text_width("88.8\xC2\xB0" "C", Font::BODY) > room ||
                                                  ui::text_width("88.8k lx", Font::BODY) > room);
  for (int i = 0; i < METRIC_COUNT; ++i) make_cell(i, ch >= ui::px(40));
  place_cells();
}

// The parts of a cell: on a soft ground, the reading's icon, its value, its name and a bar in its colour. Where they
// go is place_cell's.
void PlantTile::make_cell(int metric, bool ground) {
  Cell &cell = cells_[metric];
  // With a ground, everything of the cell lives inside it (at positions from its corner), so no part sits over another.
  lv_obj_t *host = parent_;
  if (ground) {
    cell.ground = ui::block(parent_, theme::TRACK);
    lv_obj_remove_flag(cell.ground, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_pad_all(cell.ground, 0, 0);
    host = cell.ground;
    cell.bar = ui::block(host, theme::TRACK);
    lv_obj_remove_flag(cell.bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(cell.bar, LV_RADIUS_CIRCLE, 0);
  }
  cell.icon = place_label(host, Font::ICON_SMALL, theme::MUTED, 0, 0, 1);
  cell.value = place_label(host, value_font_, theme::INK, 0, 0, 1);
  cell.name = place_label(host, Font::BODY, theme::MUTED, 0, 0, 1);
  ui::set_text(cell.icon, ui::icon(ICONS[metric]));
  ui::set_text(cell.name, plugin_->text(NAME_KEYS[metric]));
}

// The cells in their area: four, or three without a light sensor, the last row's cells sharing its whole width.
void PlantTile::place_cells() {
  const int gap = ui::px(8);
  const int count = light_shown_ ? METRIC_COUNT : METRIC_COUNT - 1;
  const int columns = std::min(cells_columns_, count);
  const int rows = (count + columns - 1) / columns;
  const int ch = (cells_area_.h - gap * (rows - 1)) / rows;
  for (int i = 0; i < METRIC_COUNT; ++i) {
    if (i == LIGHT && !light_shown_) {
      place_cell(i, {});
      continue;
    }
    const int row = i / columns, column = i % columns;
    const int in_row = row == rows - 1 ? count - row * columns : columns;
    const int cw = (cells_area_.w - gap * (in_row - 1)) / in_row;
    place_cell(i, {cells_area_.x + column * (cw + gap), cells_area_.y + row * (ch + gap), cw, ch});
  }
}

static void put(lv_obj_t *label, bool shown, int x, int y, int w, lv_text_align_t align) {
  if (shown) lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, std::max(1, w));
  lv_obj_set_style_text_align(label, align, 0);
}

// A cell in its box, its parts as far as they fit; an empty box hides it (and keeps taps away: Box::contains).
void PlantTile::place_cell(int metric, const Box &b) {
  Cell &cell = cells_[metric];
  cell.box = b;
  const bool shown = b.w > 0;
  const int pad = ui::px(6), bar_h = ui::px(4);
  const int icon_h = ui::line_height(Font::ICON_SMALL), value_h = ui::line_height(value_font_),
            name_h = ui::line_height(Font::BODY);
  int ox = b.x, oy = b.y;
  if (cell.ground) {
    if (shown) lv_obj_remove_flag(cell.ground, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(cell.ground, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(cell.ground, b.x, b.y);
    lv_obj_set_size(cell.ground, std::max(1, b.w), std::max(1, b.h));
    lv_obj_set_pos(cell.bar, pad * 2, b.h - pad - bar_h);
    lv_obj_set_size(cell.bar, std::max(1, b.w - pad * 4), bar_h);
    ox = oy = 0;
  }
  const int room = b.h - (cell.ground ? pad * 2 + bar_h : 0);
  const int top = oy + (cell.ground ? pad : 0);
  if (b.w >= ui::px(120)) {
    // Side by side: the icon, then the value with the name under it.
    const bool named = value_h + name_h <= room;
    const int text_h = value_h + (named ? name_h : 0);
    const int y = top + std::max(0, (room - text_h) / 2);
    put(cell.icon, shown, ox + pad * 2, top + (room - icon_h) / 2, icon_h, LV_TEXT_ALIGN_LEFT);
    const int tx = ox + pad * 3 + icon_h, tw = b.w - (tx - ox) - pad;
    put(cell.value, shown, tx, y, tw, LV_TEXT_ALIGN_LEFT);
    put(cell.name, shown && named, tx, y + value_h, tw, LV_TEXT_ALIGN_LEFT);
  } else {
    // Stacked: the icon, the value and the name, each as far as there is room, in the middle.
    const bool iconed = icon_h + value_h <= room, named = icon_h + value_h + name_h <= room;
    const int total = value_h + (iconed ? icon_h : 0) + (named ? name_h : 0);
    int y = top + std::max(0, (room - total) / 2);
    put(cell.icon, shown && iconed, ox, y, b.w, LV_TEXT_ALIGN_CENTER);
    if (iconed) y += icon_h;
    put(cell.value, shown, ox + pad / 2, y, b.w - pad, LV_TEXT_ALIGN_CENTER);
    y += value_h;
    put(cell.name, shown && named, ox, y, b.w, LV_TEXT_ALIGN_CENTER);
  }
}

// Whether the plant has a light reading: many have no light sensor, and their tile shows three cells.
bool PlantTile::has_light() const {
  if (demo() || *info_entities_[LIGHT] || !std::isnan(attribute_[LIGHT])) return true;
  // A plant of Plant Monitor is shown with it until it answered, so a tile with a light sensor does not jump.
  return plant_domain_ && !info_known_;
}

void PlantTile::on_state(JsonObjectConst data) {
  JsonVariantConst state = data["state"];
  chosen_ = !state.isNull();
  state_value_ = plant_domain_ ? NAN : number_of(state);
  entity_name_ = data["name"] | "";
  JsonObjectConst attrs = data["attributes"];
  // Tessera's firmware keeps a plugin tile's entity only of a domain it has tiles for, so a plant comes without its id:
  // a plant of Plant Monitor (it has a species, maybe "") is found by its name instead.
  if (entity_.empty() && attrs["species"].is<const char *>()) {
    char id[64];
    plant_entity_of(data["name"] | "", id, sizeof(id));
    entity_ = id;
    plant_domain_ = true;
  }
  attribute_[MOISTURE] = number_of(attrs["moisture"]);
  attribute_[TEMPERATURE] = celsius_of_attribute(number_of(attrs["temperature"]));
  attribute_[HUMIDITY] = number_of(attrs["humidity"]);
  attribute_[LIGHT] = number_of(attrs["brightness"]);
  if (std::isnan(attribute_[LIGHT])) attribute_[LIGHT] = number_of(attrs["illuminance"]);
  // watered_at comes as seconds since 1970 (an attribute whose name ends in _at), or as a text when it is not a moment.
  JsonVariantConst at = attrs["watered_at"];
  watered_attribute_ = at.is<const char *>() ? parse_moment(at.as<const char *>(), local_offset(tessera::epoch()))
                                             : (at | 0u);
  const uint32_t now = tessera::epoch();
  read_values(now);
  paint(now);
}

bool PlantTile::polls() const { return plant_domain_ || *watered_entity_; }

// Ask Home Assistant, through the app: a plant of Plant Monitor for its readings, the watering helper for its moment.
void PlantTile::ask(uint32_t now) {
  if (!first_ask_) first_ask_ = now;
  next_ask_ = now + plugin_->poll_seconds();
  // A plant of Plant Monitor tells its sensors, their values and its ranges in one answer.
  if (plant_domain_) {
    JsonDocument request;
    request["ask"] = "plant/get_info";
    request["data"]["entity_id"] = entity_.c_str();
    info_asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
    if (!info_asked_) next_ask_ = now + config::RETRY_SECONDS;
  }
  if (!*watered_entity_) return;
  char start[32];
  iso_utc(now - config::POLL_WINDOW_SECONDS, start, sizeof(start));
  JsonDocument request;
  request["ask"] = "history/history_during_period";
  JsonObject data = request["data"].to<JsonObject>();
  data["start_time"] = start;
  data["entity_ids"].to<JsonArray>().add(watered_entity_);
  data["include_start_time_state"] = true;
  data["significant_changes_only"] = false;
  data["minimal_response"] = true;   // the attributes only with the first state of each
  data["no_attributes"] = false;     // for an input_datetime's timestamp
  asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
  if (!asked_) next_ask_ = now + config::RETRY_SECONDS;
}

// {"result": {"moisture": {"current": "41", "min": 15, "max": 60, "sensor": "sensor.x", "unit_of_measurement": "%"},
// "temperature": {...}, "illuminance": {...}, "humidity": {...}, ...}}: a reading Plant Monitor lacks stays empty.
void PlantTile::read_info(JsonObjectConst info) {
  static const char *const KEYS[METRIC_COUNT] = {"moisture", "temperature", "humidity", "illuminance"};
  for (int i = 0; i < METRIC_COUNT; ++i) {
    JsonObjectConst m = info[KEYS[i]];
    snprintf(info_entities_[i], sizeof(info_entities_[i]), "%s", m["sensor"] | "");
    float v = number_of(m["current"]), low = number_of(m["min"]), high = number_of(m["max"]);
    const char *unit = m["unit_of_measurement"] | "";
    if (i == TEMPERATURE && strstr(unit, "F")) {
      v = (v - 32.0f) * 5.0f / 9.0f;
      low = (low - 32.0f) * 5.0f / 9.0f;
      high = (high - 32.0f) * 5.0f / 9.0f;
      fahrenheit_ = true;
    }
    if (i == LIGHT) light_percent_ = strcmp(unit, "%") == 0;
    info_[i] = v;
    // The plant's own range, set in Plant Monitor; the default range where it has none.
    if (low < high) ranges_[i] = {low, high};
  }
}

// {"re", "ok": true, "result": {"input_datetime.x": [{"s": "2026-10-11 08:30:00", "a": {...}, "lu": 1791...}]}}
bool PlantTile::answer(JsonObjectConst m) {
  if (info_asked_ && (m["re"] | 0u) == info_asked_) {
    info_asked_ = 0;
    const uint32_t now = tessera::epoch();
    if (!(m["ok"] | false)) {
      ++failures_;
      next_ask_ = now + config::RETRY_SECONDS;
      return true;
    }
    failures_ = 0;
    last_answer_ = now;
    info_known_ = true;
    // Home Assistant's answer is {"result": {...}} itself.
    JsonObjectConst info = m["result"]["result"];
    read_info(info.isNull() ? m["result"].as<JsonObjectConst>() : info);
    read_values(now);
    paint(now);
    tessera::refresh();
    return true;
  }
  if (!asked_ || (m["re"] | 0u) != asked_) return false;
  asked_ = 0;
  const uint32_t now = tessera::epoch();
  if (!(m["ok"] | false)) {
    ++failures_;
    next_ask_ = now + config::RETRY_SECONDS;
    return true;
  }
  failures_ = 0;
  last_answer_ = now;
  if (*watered_entity_) {
    JsonArrayConst rows = m["result"][watered_entity_];
    if (rows.size()) {
      // An input_button's state is the moment it was pressed; an input_datetime's is a local time, with the moment in
      // its attribute `timestamp` (of the first row: the last row's own when it changed within the minute asked).
      uint32_t at = parse_moment(rows[rows.size() - 1]["s"] | "", local_offset(now));
      const float ts = number_of(rows[0]["a"]["timestamp"]);
      if (rows.size() == 1 && !std::isnan(ts) && ts > 0) at = static_cast<uint32_t>(ts);
      if (at) watered_polled_ = at;
    }
  }
  read_values(now);
  paint(now);
  tessera::refresh();
  return true;
}

// The readings now, from the demo or from Home Assistant: Plant Monitor's sensors first, then an attribute of the
// tile's entity, then (for the soil) the entity's own state.
void PlantTile::read_values(uint32_t now) {
  const bool fake = demo();
  for (int i = 0; i < METRIC_COUNT; ++i) {
    float v = NAN;
    if (fake) {
      v = now ? demo_value(static_cast<Metric>(i), now, plugin_->demo_watered(now)) : NAN;
    } else if (*info_entities_[i]) {
      v = info_[i];
    } else {
      v = attribute_[i];
      if (i == MOISTURE && std::isnan(v)) v = state_value_;
    }
    value_[i] = v;
    status_[i] = evaluate(static_cast<Metric>(i), v, ranges_[i]);
    plugin_->live(tile_, static_cast<Metric>(i), v);
  }
  mood_ = mood_of(status_, now && thanks_until_ > now);
}

uint32_t PlantTile::watered(uint32_t now) const {
  if (demo()) return plugin_->demo_watered(now);
  uint32_t at = std::max({watered_here_, watered_polled_, watered_attribute_});
  // A plant without a helper or an attribute of its own keeps its watering on the screen (the text entity).
  if (!*watered_entity_ && !watered_attribute_) at = std::max(at, plugin_->screen_watered(tile_));
  return at;
}

std::string PlantTile::watered_text(uint32_t now) const {
  const uint32_t at = watered(now);
  if (!at) return plugin_->text("watered_never");
  const int32_t days = tessera::days_from_today(at);
  if (days == INT32_MIN) return "";
  if (days >= 0) return plugin_->text("watered_today");
  if (days == -1) return plugin_->text("watered_yesterday");
  return tessera::format(plugin_->text("watered_days"), -days);
}

// The tile's name from the editor, unless it is still the tile's default; else the plant's own name in Home Assistant.
std::string PlantTile::plant_name() const {
  const std::string own = roomy::plain(name_text_);
  if (!own.empty() && own != plugin_->text("tile_default")) return own;
  if (!entity_name_.empty()) return roomy::plain(entity_name_);
  return own.empty() ? std::string(plugin_->text("plant")) : own;
}

int PlantTile::worst_metric() const {
  int worst = MOISTURE;
  for (int i = 0; i < METRIC_COUNT; ++i)
    if (level(status_[i]) > level(status_[worst])) worst = i;
  return worst;
}

void PlantTile::paint(uint32_t now) {
  avatar_.set_mood(mood_);
  if (name_) {
    const std::string name = plant_name();
    ui::set_text(name_, name);
    if (head_.w > 0) {
      // The name as wide as it is (at most two thirds of the line), the mood in the rest.
      const int gap = ui::px(8);
      const int nw = std::min(ui::text_width(name.c_str(), Font::TITLE) + ui::px(2), head_.w * 2 / 3);
      lv_obj_set_width(name_, nw);
      lv_obj_set_pos(mood_label_, head_.x + nw + gap, head_.y);
      lv_obj_set_width(mood_label_, std::max(1, head_.w - nw - gap));
    }
  }
  ui::set_text(mood_label_, plugin_->text(mood_key(mood_)));
  if (watered_label_) ui::set_text(watered_label_, watered_text(now));
  if (cells_columns_ && has_light() != light_shown_) {
    light_shown_ = !light_shown_;
    place_cells();
  }
  char text[24];
  for (int i = 0; i < METRIC_COUNT; ++i) {
    Cell &cell = cells_[i];
    if (!cell.value) continue;
    value_text(static_cast<Metric>(i), value_[i], light_percent_, degree(value_font_), text, sizeof(text),
               compact_values_);
    ui::set_text(cell.value, text);
    const Tone tone = tone_of(status_[i]);
    if (cell.icon) ui::set_tone(cell.icon, word_tone(status_[i]), theme::MUTED);
    if (cell.bar) ui::set_fill(cell.bar, tone, theme::TRACK);
  }
  if (worst_) {
    const int w = worst_metric();
    if (std::isnan(value_[w])) {
      ui::set_text(worst_, chosen_ || demo() ? "" : plugin_->text("choose"));
    } else {
      value_text(static_cast<Metric>(w), value_[w], light_percent_, degree(Font::BODY), text, sizeof(text));
      ui::set_text(worst_, std::string(plugin_->text(NAME_KEYS[w])) + " " + text);
      ui::set_tone(worst_, word_tone(status_[w]), theme::INK_SOFT);
    }
  }
  if (note_) {
    const bool offline = !demo() && polls() &&
                         ((last_answer_ && now > last_answer_ + config::OFFLINE_AFTER_SECONDS) ||
                          (!last_answer_ && failures_ >= 2));
    const char *key = demo() ? (chosen_ || !roomy_note_ ? "demo" : "demo_choose") : offline ? "offline" : "";
    ui::set_text(note_, *key ? plugin_->text(key) : "");
    ui::set_tone(note_, demo() ? Tone::ACCENT : Tone::ALERT, theme::MUTED);
  }
}

void PlantTile::on_tick(uint32_t epoch) {
  if (epoch && !demo() && polls() && !asked_ && !info_asked_ && epoch >= next_ask_ && !plugin_->standby()) ask(epoch);
  // An ask without an answer for a whole interval is given up, so the next one can go.
  if ((asked_ || info_asked_) && epoch > next_ask_ + config::RETRY_SECONDS) {
    asked_ = info_asked_ = 0;
    ++failures_;
  }
  read_values(epoch);
  paint(epoch);
  avatar_.on_tick(epoch, plugin_->standby());
}

void PlantTile::on_theme() {
  avatar_.on_theme();
  // A wide tile's top line names the plant and says its mood more softly; a page-size tile the other way round.
  if (name_) ui::set_color(name_, head_.w > 0 ? theme::INK : theme::MUTED);
  if (mood_label_) ui::set_color(mood_label_, head_.w > 0 ? theme::MUTED : theme::INK);
  if (watered_label_) ui::set_color(watered_label_, theme::MUTED);
  if (key_icon_) ui::set_color(key_icon_, theme::ON_ACCENT);
  if (key_label_) ui::set_color(key_label_, theme::ON_ACCENT);
  if (key_) lv_obj_set_style_bg_color(key_, ui::color(theme::ACCENT), 0);
  for (Cell &cell : cells_) {
    if (cell.ground) lv_obj_set_style_bg_color(cell.ground, ui::color(theme::TRACK), 0);
    if (cell.value) ui::set_color(cell.value, theme::INK);
    if (cell.name) ui::set_color(cell.name, theme::MUTED);
  }
  // The tones (cell icons, bars, the note) are painted again with the look's colours.
  paint(tessera::epoch());
}

// Where the finger was decides: the plant or the key waters it, a cell opens that reading's card, the rest opens the
// card of the reading that matters most.
void PlantTile::on_tap() {
  const tessera::Point p = tessera::tap_point();
  const uint32_t now = tessera::epoch();
  if (p.x < 0) {
    open_detail(worst_metric());
    return;
  }
  if ((key_ && key_box_.contains(p.x, p.y, ui::px(6))) || avatar_.hit(p.x, p.y)) {
    water(now);
    return;
  }
  for (int i = 0; i < METRIC_COUNT; ++i)
    if (cells_[i].value && cells_[i].box.contains(p.x, p.y, ui::px(4))) {
      open_detail(i);
      return;
    }
  open_detail(worst_metric());
}

void PlantTile::water(uint32_t now) {
  if (!now) return;
  thanks_until_ = now + config::THANKS_SECONDS;
  if (demo()) {
    plugin_->set_demo_watered(now);
  } else {
    watered_here_ = now;
    if (*watered_entity_) {
      // Tell Home Assistant: a helper keeps the moment for every screen and survives a restart.
      char stamp[16];
      snprintf(stamp, sizeof(stamp), "%lu", static_cast<unsigned long>(now));
      if (starts_with(watered_entity_, "input_datetime."))
        tessera::action("input_datetime.set_datetime", watered_entity_, "timestamp", stamp);
      else if (starts_with(watered_entity_, "input_button."))
        tessera::action("input_button.press", watered_entity_);
      else if (starts_with(watered_entity_, "button."))
        tessera::action("button.press", watered_entity_);
    } else if (!watered_attribute_) {
      plugin_->store_screen_watered(tile_, now);
    }
  }
  read_values(now);
  paint(now);
  avatar_.celebrate();
}

void PlantTile::open_detail(int metric) {
  Detail &d = plugin_->detail_;
  d = Detail{};
  d.metric = static_cast<Metric>(metric);
  d.range = ranges_[metric];
  d.value = value_[metric];
  d.light_percent = light_percent_;
  d.fahrenheit = fahrenheit_;
  d.demo = demo();
  d.demo_watered = plugin_->demo_watered(tessera::epoch());
  d.tile = tile_;
  // The sensor with a history: an option's own, or the tile's entity for the soil when it is that sensor.
  const char *entity = "";
  if (!d.demo) {
    if (*info_entities_[metric]) entity = info_entities_[metric];
    else if (metric == MOISTURE && !plant_domain_ && std::isnan(attribute_[MOISTURE])) entity = entity_.c_str();
  }
  snprintf(d.entity, sizeof(d.entity), "%s", entity);
  tessera::open_card(plugin_->plugin_id(), "metric", d.entity, tile_, plugin_->text(TITLE_KEYS[metric]));
}

// =====================================================================================================================
//  The card of one reading: its value, where it stands in its range, what to do, and its last 24 hours
// =====================================================================================================================

class MetricCard : public tessera::Card {
 public:
  explicit MetricCard(Microplant *plugin) : plugin_(plugin) {}
  ~MetricCard() override {
    if (plugin_->open_card_ == this) plugin_->open_card_ = nullptr;
  }

  void open(const tessera::CardContext &c) override;
  void on_tick(uint32_t epoch) override;
  void on_theme() override;
  bool answer(JsonObjectConst message);
  // The tile it was opened from read a new value.
  void live(int tile, Metric metric, float value) {
    if (tile == d_.tile && metric == d_.metric) d_.value = value;
  }

 private:
  void paint_value();
  void paint_history(const float *values, int count);
  int to_x(float v) const;
  // Light spans orders of magnitude (50 lx to 50k lx): its ruler and chart are logarithmic.
  float axis(float v) const { return d_.metric == LIGHT ? std::log10(std::max(v, 1.0f)) : v; }

  Microplant *plugin_;
  Detail d_;
  int width_ = 0, height_ = 0;
  float scale_low_ = 0, scale_high_ = 1;
  uint32_t asked_ = 0;
  lv_obj_t *value_{}, *status_{}, *hint_{}, *range_{}, *marker_{};
  lv_obj_t *zones_[5]{};
  lv_obj_t *edge_low_{}, *edge_high_{};
  lv_obj_t *history_title_{}, *history_span_{}, *chart_band_{}, *chart_line_{}, *chart_note_{};
  lv_obj_t *ago_{}, *now_{};
  Box chart_;
  lv_point_precise_t points_[config::HISTORY_HOURS + 1]{};
  Font value_font_ = Font::VALUE;
};

void MetricCard::open(const tessera::CardContext &c) {
  d_ = plugin_->detail_;
  width_ = c.width;
  height_ = c.height;
  plugin_->open_card_ = this;
  const int gap = ui::px(12);

  // The value, large, with its status and what to do beside it.
  char sample[24];
  value_text(d_.metric, 88.8f, d_.light_percent, degree(Font::VALUE), sample, sizeof(sample));
  value_font_ = d_.metric == TEMPERATURE || ui::covers(sample, Font::DIGITS) ? Font::DIGITS : Font::VALUE;
  value_text(d_.metric, 88.8f, d_.light_percent, degree(value_font_), sample, sizeof(sample));
  if (!ui::covers(sample, value_font_)) value_font_ = Font::VALUE;
  const int value_w = std::min(width_ / 2, ui::text_width("88.8k lx", value_font_) + ui::px(8));
  const int vh = ui::line_height(value_font_);
  value_ = place_label(c.parent, value_font_, theme::INK, 0, 0, value_w);
  const int sx = value_w + gap, sw = width_ - sx;
  const int words_h = ui::line_height(Font::HEADLINE) + ui::line_height(Font::BODY);
  const int sy = std::max(0, (vh - words_h) / 2);
  status_ = place_label(c.parent, Font::HEADLINE, theme::INK, sx, sy, sw);
  hint_ = place_label(c.parent, Font::BODY, theme::MUTED, sx, sy + ui::line_height(Font::HEADLINE), sw);
  int y = std::max(vh, words_h) + gap;

  // The ruler: red, amber, green, amber, red, with a mark where the value is and the range's edges under it.
  float below, above;
  red_edges(d_.metric, d_.range, below, above);
  const float span = above - below;
  scale_low_ = d_.metric == LIGHT ? below / 2.5f : below - span * 0.25f;
  scale_high_ = d_.metric == LIGHT ? above * 2.5f : above + span * 0.25f;
  if (d_.metric == MOISTURE || d_.metric == HUMIDITY) {
    scale_low_ = std::max(0.0f, scale_low_);
    scale_high_ = std::min(100.0f, scale_high_);
  }
  const int ruler_h = ui::px(12), mark_h = ui::px(26);
  const int ry = y + (mark_h - ruler_h) / 2;
  const float edges[6] = {scale_low_, below, d_.range.low, d_.range.high, above, scale_high_};
  static constexpr Tone ZONES[5] = {Tone::ALERT, Tone::BUSY, Tone::GOOD, Tone::BUSY, Tone::ALERT};
  for (int i = 0; i < 5; ++i) {
    zones_[i] = ui::block(c.parent, theme::TRACK);
    lv_obj_remove_flag(zones_[i], LV_OBJ_FLAG_CLICKABLE);
    const int x0 = to_x(edges[i]), x1 = to_x(edges[i + 1]);
    lv_obj_set_pos(zones_[i], x0, ry);
    lv_obj_set_size(zones_[i], std::max(1, x1 - x0), ruler_h);
    lv_obj_set_style_radius(zones_[i], i == 0 || i == 4 ? ruler_h / 2 : 0, 0);
    ui::set_fill(zones_[i], ZONES[i], theme::TRACK);
  }
  marker_ = ui::block(c.parent, theme::INK);
  lv_obj_remove_flag(marker_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(marker_, ui::px(6), mark_h);
  lv_obj_set_style_border_width(marker_, ui::px(2), 0);
  lv_obj_set_y(marker_, y);
  y += mark_h + ui::px(2);
  const int edge_w = ui::px(90);
  edge_low_ = place_label(c.parent, Font::BODY, theme::MUTED, std::max(0, to_x(d_.range.low) - edge_w / 2), y, edge_w,
                          LV_TEXT_ALIGN_CENTER);
  edge_high_ = place_label(c.parent, Font::BODY, theme::MUTED,
                           std::min(width_ - edge_w, to_x(d_.range.high) - edge_w / 2), y, edge_w,
                           LV_TEXT_ALIGN_CENTER);
  char low[16], high[16];
  value_text(d_.metric, d_.range.low, d_.light_percent, degree(Font::BODY), low, sizeof(low));
  value_text(d_.metric, d_.range.high, d_.light_percent, degree(Font::BODY), high, sizeof(high));
  ui::set_text(edge_low_, low);
  ui::set_text(edge_high_, high);
  y += ui::line_height(Font::BODY) + ui::px(4);
  // What the chart needs under it: its title, a chart of at least min_chart, and the "24 h ago ... Now" foot.
  const int head = ui::line_height(Font::BODY_LARGE) + ui::px(6), foot = ui::line_height(Font::BODY) + ui::px(2);
  const int min_chart = ui::px(40);
  // The range in words, when the chart still has its room under it (on a small screen the ruler's edges say it).
  if (y + ui::line_height(Font::BODY) + gap + head + min_chart + foot <= height_) {
    range_ = place_label(c.parent, Font::BODY, theme::MUTED, 0, y, width_);
    ui::set_text(range_, tessera::fill(tessera::fill(plugin_->text("range"), "low", low).c_str(), "high", high));
    y += ui::line_height(Font::BODY) + gap;
  } else {
    y += ui::px(4);
  }
  // The foot only when the chart keeps min_chart above it; else the chart takes what is left.
  const bool footed = y + head + min_chart + foot <= height_;

  // The last 24 hours: a line over the green band of its range.
  history_title_ = place_label(c.parent, Font::BODY_LARGE, theme::INK, 0, y, width_ / 2);
  ui::set_text(history_title_, plugin_->text("history_title"));
  history_span_ = place_label(c.parent, Font::BODY, theme::MUTED, width_ / 2, y, width_ / 2, LV_TEXT_ALIGN_RIGHT);
  y += head;
  chart_ = {0, y, width_, std::max(ui::px(8), height_ - y - (footed ? foot : 0))};
  chart_band_ = ui::block(c.parent, theme::TRACK);
  lv_obj_remove_flag(chart_band_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(chart_band_, LV_OPA_30, 0);
  lv_obj_set_style_opa(chart_band_, LV_OPA_TRANSP, 0);
  chart_line_ = lv_line_create(c.parent);
  lv_obj_remove_flag(chart_line_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_pos(chart_line_, chart_.x, chart_.y);
  lv_obj_set_size(chart_line_, chart_.w, chart_.h);
  lv_obj_set_style_line_width(chart_line_, ui::px(3), 0);
  lv_obj_set_style_line_rounded(chart_line_, true, 0);
  chart_note_ = place_label(c.parent, Font::BODY, theme::MUTED, 0,
                            std::min(height_ - ui::line_height(Font::BODY), chart_.y + chart_.h / 2 - ui::line_height(Font::BODY) / 2),
                            width_, LV_TEXT_ALIGN_CENTER);
  if (footed) {
    const int fy = chart_.y + chart_.h + ui::px(2);
    ago_ = place_label(c.parent, Font::BODY, theme::SUBTLE, 0, fy, width_ / 2);
    ui::set_text(ago_, plugin_->text("history_ago"));
    now_ = place_label(c.parent, Font::BODY, theme::SUBTLE, width_ / 2, fy, width_ / 2, LV_TEXT_ALIGN_RIGHT);
    ui::set_text(now_, plugin_->text("history_now"));
  }

  const uint32_t now = tessera::epoch();
  if (d_.demo && now) {
    // The demo's own story: one cycle of it, drawn as a day.
    float values[config::HISTORY_HOURS];
    const uint32_t step = config::DEMO_CYCLE_SECONDS / config::HISTORY_HOURS;
    for (int i = 0; i < config::HISTORY_HOURS; ++i)
      values[i] = demo_value(d_.metric, now - (config::HISTORY_HOURS - 1 - i) * step, d_.demo_watered);
    paint_history(values, config::HISTORY_HOURS);
  } else if (*d_.entity && now) {
    // The hourly means of Home Assistant's statistics (a sensor with a state_class), mapped by the manifest's
    // `answers` to one list of numbers.
    char start[32], end[32];
    iso_utc(now - config::HISTORY_HOURS * 3600, start, sizeof(start));
    iso_utc(now, end, sizeof(end));
    JsonDocument request;
    request["ask"] = "recorder/statistics_during_period";
    JsonObject data = request["data"].to<JsonObject>();
    data["start_time"] = start;
    data["end_time"] = end;
    data["statistic_ids"].to<JsonArray>().add(d_.entity);
    data["period"] = "hour";
    data["types"].to<JsonArray>().add("mean");
    asked_ = tessera::send(plugin_, request.as<JsonObjectConst>());
    ui::set_text(chart_note_, plugin_->text(asked_ ? "history_wait" : "history_none"));
  } else {
    ui::set_text(chart_note_, plugin_->text("history_none"));
  }
  on_theme();
}

int MetricCard::to_x(float v) const {
  const float t = (axis(v) - axis(scale_low_)) / (axis(scale_high_) - axis(scale_low_));
  return static_cast<int>(std::clamp(t, 0.0f, 1.0f) * static_cast<float>(width_ - 1));
}

void MetricCard::paint_value() {
  char text[24];
  value_text(d_.metric, d_.value, d_.light_percent, degree(value_font_), text, sizeof(text));
  ui::set_text(value_, text);
  const Status s = evaluate(d_.metric, d_.value, d_.range);
  ui::set_text(status_, plugin_->text(status_key(s)));
  ui::set_tone(status_, word_tone(s), theme::INK);
  ui::set_text(hint_, plugin_->text(is_low(s)    ? LOW_HINTS[d_.metric]
                                    : is_high(s) ? HIGH_HINTS[d_.metric]
                                    : s == Status::GOOD ? "hint_good" : "hint_none"));
  const bool known = !std::isnan(d_.value);
  lv_obj_set_style_opa(marker_, known ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  if (known) lv_obj_set_x(marker_, std::clamp(to_x(d_.value) - ui::px(3), 0, width_ - ui::px(6)));
}

void MetricCard::paint_history(const float *values, int count) {
  float lo = d_.range.low, hi = d_.range.high;
  float seen_lo = INFINITY, seen_hi = -INFINITY;
  int n = 0;
  for (int i = 0; i < count; ++i) {
    if (std::isnan(values[i])) continue;
    seen_lo = std::min(seen_lo, values[i]);
    seen_hi = std::max(seen_hi, values[i]);
  }
  if (seen_lo > seen_hi) {
    ui::set_text(chart_note_, plugin_->text("history_none"));
    return;
  }
  lo = axis(std::min(lo, seen_lo));
  hi = axis(std::max(hi, seen_hi));
  const float pad = (hi - lo) * 0.1f + (d_.metric == LIGHT ? 0.05f : 0.5f);
  lo -= pad;
  hi += pad;
  auto y_of = [&](float v) {
    return static_cast<int>((hi - axis(v)) / (hi - lo) * static_cast<float>(chart_.h - 1));
  };
  const int last = std::min(count, config::HISTORY_HOURS + 1);
  for (int i = 0; i < last; ++i) {
    if (std::isnan(values[i])) continue;
    const int x = last > 1 ? i * (chart_.w - 1) / (last - 1) : chart_.w / 2;
    points_[n++] = {x, y_of(values[i])};
  }
  lv_line_set_points(chart_line_, points_, static_cast<uint32_t>(n));
  // The green band of the range behind it.
  const int top = y_of(d_.range.high), bottom = y_of(d_.range.low);
  lv_obj_set_pos(chart_band_, chart_.x, chart_.y + std::max(0, top));
  lv_obj_set_size(chart_band_, chart_.w, std::max(2, std::min(chart_.h, bottom) - std::max(0, top)));
  lv_obj_set_style_opa(chart_band_, LV_OPA_COVER, 0);
  ui::set_fill(chart_band_, Tone::GOOD, theme::TRACK);
  ui::set_text(chart_note_, "");
  char low[16], high[16];
  value_text(d_.metric, seen_lo, d_.light_percent, degree(Font::BODY), low, sizeof(low));
  value_text(d_.metric, seen_hi, d_.light_percent, degree(Font::BODY), high, sizeof(high));
  ui::set_text(history_span_, tessera::fill(tessera::fill(plugin_->text("history_span"), "low", low).c_str(), "high", high));
}

// {"re", "ok": true, "result": {"means": [41.2, 40.8, null, ...]}} (the manifest's `answers`).
bool MetricCard::answer(JsonObjectConst m) {
  if (!asked_ || (m["re"] | 0u) != asked_) return false;
  asked_ = 0;
  if (!(m["ok"] | false)) {
    ui::set_text(chart_note_, plugin_->text("history_none"));
    return true;
  }
  float values[config::HISTORY_HOURS + 1];
  int n = 0;
  for (JsonVariantConst v : m["result"]["means"].as<JsonArrayConst>()) {
    if (n > config::HISTORY_HOURS) break;
    float x = number_of(v);
    if (d_.fahrenheit && !std::isnan(x)) x = (x - 32.0f) * 5.0f / 9.0f;
    values[n++] = x;
  }
  paint_history(values, n);
  return true;
}

void MetricCard::on_tick(uint32_t) { paint_value(); }

void MetricCard::on_theme() {
  ui::set_color(value_, theme::INK);
  ui::set_color(hint_, theme::MUTED);
  if (range_) ui::set_color(range_, theme::MUTED);
  ui::set_color(edge_low_, theme::MUTED);
  ui::set_color(edge_high_, theme::MUTED);
  ui::set_color(history_title_, theme::INK);
  ui::set_color(history_span_, theme::MUTED);
  ui::set_color(chart_note_, theme::MUTED);
  if (ago_) ui::set_color(ago_, theme::SUBTLE);
  if (now_) ui::set_color(now_, theme::SUBTLE);
  lv_obj_set_style_bg_color(marker_, ui::color(theme::INK), 0);
  lv_obj_set_style_border_color(marker_, ui::color(theme::CARD), 0);
  lv_obj_set_style_line_color(chart_line_, ui::color(theme::ACCENT), 0);
  static constexpr Tone ZONES[5] = {Tone::ALERT, Tone::BUSY, Tone::GOOD, Tone::BUSY, Tone::ALERT};
  for (int i = 0; i < 5; ++i) ui::set_fill(zones_[i], ZONES[i], theme::TRACK);
  if (lv_obj_get_style_opa(chart_band_, LV_PART_MAIN) != LV_OPA_TRANSP) ui::set_fill(chart_band_, Tone::GOOD, theme::TRACK);
  paint_value();
}

// =====================================================================================================================
//  The plugin
// =====================================================================================================================

void Microplant::setup() {
  add_tile("plant", [this]() { return new PlantTile(this); });
  add_card("metric", [this]() { return new MetricCard(this); }, true);
  add_bar_item("thirsty", [this]() { return bar(); });
}

void Microplant::detach(PlantTile *tile) {
  tiles_.erase(std::remove(tiles_.begin(), tiles_.end(), tile), tiles_.end());
}

void Microplant::on_message(JsonObjectConst message) {
  if (open_card_ && open_card_->answer(message)) return;
  for (PlantTile *tile : tiles_)
    if (tile->answer(message)) return;
}

void Microplant::live(int tile, Metric metric, float value) {
  if (open_card_) open_card_->live(tile, metric, value);
}

uint32_t Microplant::poll_seconds() const {
  if (!poll_ || std::isnan(poll_->state)) return config::POLL_SECONDS;
  return std::clamp(static_cast<uint32_t>(poll_->state), config::POLL_MIN_SECONDS, config::POLL_MAX_SECONDS);
}

uint32_t Microplant::screen_watered(int tile) const {
  return watered_ ? stored_watering(watered_->state.c_str(), tile) : 0;
}

void Microplant::store_screen_watered(int tile, uint32_t epoch) {
  if (!watered_ || !epoch) return;
  char text[WATERED_STORE_LENGTH + 1];
  store_watering(watered_->state.c_str(), tile, epoch, text, sizeof(text));
  auto call = watered_->make_call();
  call.set_value(text);
  call.perform();
}

uint32_t Microplant::demo_watered(uint32_t now) {
  if (!demo_watered_ && now > config::DEMO_FIRST_WATERED_AGO) demo_watered_ = now - config::DEMO_FIRST_WATERED_AGO;
  return demo_watered_;
}

// "Water Monstera" in the top bar while a plant on the glass is thirsty: amber, red when it is parched.
tessera::BarItem Microplant::bar() const {
  tessera::BarItem item;
  for (PlantTile *tile : tiles_) {
    if (!tile->thirsty()) continue;
    item.shown = true;
    item.icon = ICON_WATER;
    item.tone = tile->parched() || item.tone == Tone::ALERT ? Tone::ALERT : Tone::BUSY;
    if (item.text.empty())
      item.text = *tile->name() ? tessera::fill(text("bar_water"), "name", tile->name()) : text("bar_water_plain");
  }
  return item;
}

bool Microplant::settings(tessera::SettingsPage &page) {
  page.icon = "\U000F0E66";  // sprout
  page.toggle(text("set_demo"), [this]() { return demo(); },
              [this](bool on) {
                if (!demo_) return;
                if (on) demo_->turn_on();
                else demo_->turn_off();
              });
  page.number(text("set_poll"), config::POLL_MIN_SECONDS, config::POLL_MAX_SECONDS, 30, " s",
              [this]() { return static_cast<int>(poll_seconds()); },
              [this](int v) {
                if (!poll_) return;
                auto call = poll_->make_call();
                call.set_value(static_cast<float>(v));
                call.perform();
              });
  page.action(text("set_watered"), "\U000F058C", [this]() { store_screen_watered(-1, tessera::epoch()); }, nullptr,
              [this]() -> std::string {
                const uint32_t at = screen_watered(-1);
                return at ? tessera::date_text(at) + " " + tessera::clock_text(at) : std::string(text("set_never"));
              });
  return true;
}

}  // namespace esphome::microplant
